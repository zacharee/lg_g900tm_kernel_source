/*
* This software program is licensed subject to the GNU General Public License
* (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

* (C) Copyright 2011 Bosch Sensortec GmbH
* All Rights Reserved
*/


/* file lm36272.c
brief This file contains all function implementations for the lm36272 in linux
this source file refer to MT6883 platform
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/of.h>
#include "lm36272.h"

#ifdef CONFIG_LGE_USE_BRIGHTNESS_TABLE
unsigned int* blmap_arr = NULL;
unsigned int blmap_size = 0;
#endif

#define MIN_VALUE_SETTINGS 10 /* value leds_brightness_set*/
#define MAX_VALUE_SETTINGS 255 /* value leds_brightness_set*/
#define MIN_MAX_SCALE(x) (((x)<MIN_VALUE_SETTINGS) ? MIN_VALUE_SETTINGS : (((x)>MAX_VALUE_SETTINGS) ? MAX_VALUE_SETTINGS:(x)))

#define BACKLIHGT_NAME "charge-pump"
#define LM36272_DEV_NAME "LM36272"

#define CPD_TAG                  "[chargepump] "
#define CPD_FUN(f)               printk(CPD_TAG"%s\n", __FUNCTION__)
#define CPD_ERR(fmt, args...)    printk(CPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define CPD_LOG(fmt, args...)    printk(CPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)

static struct i2c_client *new_client = NULL;
static const struct i2c_device_id lm36272_i2c_id[] = {{LM36272_DEV_NAME,0},{}};
static LM36272_BLED_DSV_DATA *g_pdata = NULL;

#if defined(CONFIG_LEDS_OPR_MODE_RECOVERY)
#define I2C_SET (0x00)
#define PWM_SET (0x01)
#define PWM_CLEAR (~0x01)
static char pwm_en;
#endif

// Flash control
unsigned char strobe_ctrl;
unsigned char flash_ctrl=0; // flash_en register(0x0A) setting position change.
unsigned char mapping_level;

//static unsigned char current_brightness = 0;
static unsigned char is_suspend = 0;
struct semaphore lm36272_lock; // Add semaphore for lcd and flash i2c communication.

#ifdef CONFIG_LGE_USE_BRIGHTNESS_TABLE
static int mt_backlight_brightness_parse_dt(struct device_node *np)
{
	int rc = 0;

	if (!np) {
		pr_err("[%s] DT node: Not found\n", __func__);
		blmap_arr = NULL;
		return -1;
	} else {
		rc = of_property_read_u32(np, "lge.blmap-size", &blmap_size);
		if( blmap_size ){
			blmap_arr = kzalloc(sizeof(int) * blmap_size, GFP_KERNEL);
			if( !blmap_arr ){
				printk("[%s] falied to alloc memory for brightness table \n", __func__);
				return -1;
			} else {
#ifdef CONFIG_LGE_BRIGHTNESS_TABLE_DUALIZATION
				if( mt_get_maker_id() == PRIMARY_LCM ){
					rc = of_property_read_u32_array(np,"lge.blmap", blmap_arr, blmap_size);
					printk("[%s] read 1st backlight map\n", __func__);
				} else {
					rc = of_property_read_u32_array(np,"lge.blmap_2nd", blmap_arr, blmap_size);
					printk("[%s] read 2nd backlight map\n", __func__);
				}
#else
				rc = of_property_read_u32_array(np,"lge.blmap", blmap_arr, blmap_size);
#endif
				if (rc){
					printk("[%s] unable to read backlight map\n", __func__);
					return -1;
				}
				else{
					printk("[%s] Success to read backlight map\n", __func__);
				}
			}
		} else {
			printk("[%s] failed to read backlight map size\n", __func__);
			return -1;
		}
	}
	return 0;
}
#endif

/* i2c read routine for API*/
static char lm36272_i2c_read(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
{
	s32 dummy;

	if (NULL == client)
		return -1;

	while (0 != len--) {
		dummy = i2c_master_send(client, (char *)&reg_addr, 1);
		if (dummy < 0) {
			CPD_ERR("send dummy is %d\n", dummy);
			return -1;
		}

		dummy = i2c_master_recv(client, (char *)data, 1);
		if (dummy < 0) {
			CPD_ERR("recv dummy is %d\n", dummy);
			return -1;
		}

		reg_addr++;
		data++;
	}
	return 0;
}

/* i2c write routine for */
static char lm36272_i2c_write(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
{
	s32 dummy;

	if (NULL == client)
		return -1;

	while (0 != len--) {
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
		reg_addr++;
		data++;
		if (dummy < 0) {
			return -1;
		}
	}
	//CPD_LOG("lm36272_i2c_write \n");
	return 0;
}

static int lm36272_smbus_read_byte(struct i2c_client *client,
            unsigned char reg_addr, unsigned char *data)
{
	return lm36272_i2c_read(client,reg_addr,data,1);
}

static int lm36272_smbus_write_byte(struct i2c_client *client,
            unsigned char reg_addr, unsigned char *data)
{
	int ret_val = 0;
	int i = 0;

	ret_val = lm36272_i2c_write(client,reg_addr,data,1);

	for ( i = 0; i < 5; i++) {
		if (ret_val != 0)
			ret_val = lm36272_i2c_write(client,reg_addr,data,1);
		else {
			//CPD_ERR("[lm36272] : lm36272_smbus_write_byte fail: %d\n",ret_val);
			return ret_val;
		}
	}
	return ret_val;
}

static int lm36272_reg_update_bits(struct i2c_client *client, unsigned char addr, unsigned char mask, unsigned char data)
{
	unsigned char orig = 0;
	int ret = 0;

	CPD_LOG("%s: reg %02x data %02x\n", __func__, addr, data);
	//CPD_LOG("%s: mask %02x\n", __func__, mask);

	ret = lm36272_smbus_read_byte(client, addr, &orig);
	if (ret < 0)
		return -1;
	orig &= ~mask;
	orig |= (data & mask);
	ret = lm36272_smbus_write_byte(client, addr, &orig);

	return ret;
}

static void lm36272_bled_init_register(void)
{
    //BLED init
    lm36272_reg_update_bits(new_client, LM36272_REG2_ADDR, LM36272_BLED_OVP_MASK, (g_pdata->bl_ovp << LM36272_BLED_OVP_SHFT));
    lm36272_reg_update_bits(new_client, LM36272_REG2_ADDR, LM36272_BLED_OVPMODE_MASK, (g_pdata->ovp_mode << LM36272_BLED_OVPMODE_SHFT));
	lm36272_reg_update_bits(new_client, LM36272_REG2_ADDR, LM36272_BLED_PWMRAMP_MASK, (g_pdata->pwm_ramp << LM36272_BLED_PWMRAMP_SHFT));
	lm36272_reg_update_bits(new_client, LM36272_REG3_ADDR, LM36272_BLED_BOOSTFREQ_MASK, (g_pdata->boost_freq << LM36272_BLED_BOOSTFREQ_SHFT));
	lm36272_reg_update_bits(new_client, LM36272_REG3_ADDR, LM36272_BLED_PWMFSAMPLE_MASK, (g_pdata->pwm_fsample << LM36272_BLED_PWMFSAMPLE_SHFT));
	lm36272_reg_update_bits(new_client, LM36272_REG2_ADDR, LM36272_BLED_PWMENABLE_MASK, (g_pdata->pwm_enable << LM36272_BLED_PWMENABLE_SHFT));
}

static void lm36272_dsv_init_register(void)
{
    //DSV init
    lm36272_reg_update_bits(new_client, LM36272_REG9_ADDR, LM36272_DSV_VPOSDISCH_MASK, (g_pdata->vpos_disch << LM36272_DSV_VPOSDISCH_SHFT));
	lm36272_reg_update_bits(new_client, LM36272_REG9_ADDR, LM36272_DSV_VNEGDISCH_MASK, (g_pdata->vneg_disch << LM36272_DSV_VNEGDISCH_SHFT));
	lm36272_reg_update_bits(new_client, LM36272_REG9_ADDR, LM36272_DSV_EXTEN_MASK, (g_pdata->ext_en << LM36272_DSV_EXTEN_SHFT));
	lm36272_reg_update_bits(new_client, LM36272_REG13_ADDR, LM36272_DSV_VPOS_MASK, (g_pdata->vpos << LM36272_DSV_VPOS_SHFT));
	lm36272_reg_update_bits(new_client, LM36272_REG14_ADDR, LM36272_DSV_VNEG_MASK, (g_pdata->vneg << LM36272_DSV_VNEG_SHFT));
}

int chargepump_set_backlight_level(unsigned int level)
{
	int ret;
	unsigned int data = 0;
	unsigned char data1 = 0;
	unsigned char lsb_data = 0x00; // 3bit
	unsigned char msb_data = 0x00; // 8bit
#if 0//defined(CONFIG_LEDS_OPR_MODE_RECOVERY)
	unsigned char reg_status = 0;
#endif

	CPD_LOG("chargepump_set_backlight_level  [%d]\n",level);

	if (level == 0){
		if(is_suspend == false){
			CPD_LOG( "backlight off\n");
			ret = down_interruptible(&lm36272_lock); // Add semaphore for lcd and flash i2c communication.
			data1 = 0x00; //backlight2 brightness 0
			lm36272_smbus_write_byte(new_client, LM36272_REG4_ADDR, &data1);  // LSB 3bit all 0
			lm36272_smbus_write_byte(new_client, LM36272_REG5_ADDR, &data1);  // MSB 3bit all 0
			lm36272_reg_update_bits(new_client, LM36272_REG8_ADDR, LM36272_BLED_LED1EN_MASK, (0 << LM36272_BLED_LED1EN_SHFT));
			lm36272_reg_update_bits(new_client, LM36272_REG8_ADDR, LM36272_BLED_LED2EN_MASK, (0 << LM36272_BLED_LED2EN_SHFT));
			lm36272_reg_update_bits(new_client, LM36272_REG8_ADDR, LM36272_BLED_BLEN_MASK, (0 << LM36272_BLED_BLEN_SHFT));

			is_suspend = true;// Move backlight suspend setting position into semaphore
			up(&lm36272_lock); // Add semaphore for lcd and flash i2c communication.
		}
	} else {
		level = MIN_MAX_SCALE(level);
		data = blmap_arr[level];
		CPD_LOG("%s data = %d\n", __func__, data);

		mapping_level = data;
		if (is_suspend == true){
			is_suspend = false;
			ret = down_interruptible(&lm36272_lock); // Add semaphore for lcd and flash i2c communication.

			lm36272_bled_init_register();

			lsb_data = (data) & 0x07; // 3bit LSB
			msb_data = (data >> 3) & 0xFF; // 8bit MSB

			lm36272_smbus_write_byte(new_client, LM36272_REG4_ADDR, &lsb_data); // LSB
			lm36272_smbus_write_byte(new_client, LM36272_REG5_ADDR, &msb_data); // MSB

			CPD_LOG("[LM36272]-backlight brightness Setting[reg0x04][MSB:0x%x]\n",lsb_data);
			CPD_LOG("[LM36272]-backlight brightness Setting[reg0x05][LSB:0x%x]\n",msb_data);

#if 0//defined(CONFIG_LEDS_OPR_MODE_RECOVERY)
			lm36272_smbus_read_byte(new_client, 0x02, &data1);

			reg_status = data1 & PWM_SET;

			if(pwm_en != reg_status) /*abnormal status*/
			{
				if(led_mode == LED_MODE_CUST_BLS_PWM)
					data1 |= PWM_SET;
				else
					data1 &= PWM_CLEAR;
				lm36272_smbus_write_byte(new_client, 0x09, &data1);
				CPD_LOG("[LM36272]-PWM_EN bit recovery. [reg0x09][value:0x%x]\n", data1);
			}
#endif
			lm36272_reg_update_bits(new_client, LM36272_REG8_ADDR, LM36272_BLED_LED1EN_MASK, (1 << LM36272_BLED_LED1EN_SHFT));
			lm36272_reg_update_bits(new_client, LM36272_REG8_ADDR, LM36272_BLED_LED2EN_MASK, (1 << LM36272_BLED_LED2EN_SHFT));
			lm36272_reg_update_bits(new_client, LM36272_REG8_ADDR, LM36272_BLED_BLEN_MASK, (1 << LM36272_BLED_BLEN_SHFT));

			// Move backlight suspend setting position into semaphore
			up(&lm36272_lock); // Add semaphore for lcd and flash i2c communication.
		}

		if (level != 0) // Move backlight suspend setting position into semaphore
		{
			ret = down_interruptible(&lm36272_lock); // Add semaphore for lcd and flash i2c communication.

			lsb_data = (data) & 0x07; // 3bit LSB
			msb_data = (data >> 3) & 0xFF; // 8bit MSB

			lm36272_smbus_write_byte(new_client, LM36272_REG4_ADDR, &lsb_data); // LSB
			lm36272_smbus_write_byte(new_client, LM36272_REG5_ADDR, &msb_data); // MSB

			CPD_LOG("[LM36272]-backlight brightness Setting[reg0x04][MSB:0x%x]\n",lsb_data);
			CPD_LOG("[LM36272]-backlight brightness Setting[reg0x05][LSB:0x%x]\n",msb_data);

			up(&lm36272_lock); // Add semaphore for lcd and flash i2c communication.
		} // Move backlight suspend setting position into semaphore
	}
	return 0;
}

void lm36272_chargepump_dsv_enable(int enable, int delay)
{
    if(enable == 1) {
        lm36272_dsv_init_register();

		lm36272_reg_update_bits(new_client, LM36272_REG9_ADDR, LM36272_DSV_VPOSEN_MASK, (enable << LM36272_DSV_VPOSEN_SHFT));
		if(delay)
			mdelay(delay);
		lm36272_reg_update_bits(new_client, LM36272_REG9_ADDR, LM36272_DSV_VNEGEN_MASK, (enable << LM36272_DSV_VNEGEN_SHFT));
        CPD_LOG("[lm36272]chargepump DSV on\n");
    } else {
		lm36272_reg_update_bits(new_client, LM36272_REG9_ADDR, LM36272_DSV_VNEGEN_MASK, (enable << LM36272_DSV_VNEGEN_SHFT));
		if(delay)
			mdelay(delay);
		lm36272_reg_update_bits(new_client, LM36272_REG9_ADDR, LM36272_DSV_VPOSEN_MASK, (enable << LM36272_DSV_VPOSEN_SHFT));
        CPD_LOG("[lm36272]chargepump DSV off\n");
    }
}

unsigned int get_backlight_brightness_rawdata(void)
{
	unsigned char lm36272_msb = 0;
	unsigned char lm36272_lsb = 0;
	unsigned int lm36272_level = 0;

	lm36272_smbus_read_byte(new_client, LM36272_REG4_ADDR, &lm36272_lsb);
	lm36272_smbus_read_byte(new_client, LM36272_REG5_ADDR, &lm36272_msb);

	lm36272_level |= ((lm36272_msb & 0xFF) << 3); // 8bit MSB
	lm36272_level |= ((lm36272_lsb & 0x07)); // 3bit LSB8

	return lm36272_level;
}

void set_backlight_brightness_rawdata(unsigned int level)
{
	unsigned char lm36272_msb = 0;
	unsigned char lm36272_lsb = 0;

	lm36272_lsb = (level) & 0x07; // 3bit LSB
	lm36272_msb = (level >> 3) & 0xFF; // 8bit MSB

	lm36272_smbus_write_byte(new_client, LM36272_REG4_ADDR, &lm36272_lsb);
	lm36272_smbus_write_byte(new_client, LM36272_REG5_ADDR, &lm36272_msb);
}

static ssize_t lcd_backlight_show_blmap(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	int i, j;

	buf[0] = '{';

	for (i = 0, j = 2; i < 256 && j < PAGE_SIZE; ++i) {
		if (!(i % 15)) {
			buf[j] = '\n';
			++j;
		}

		sprintf(&buf[j], "%d, ", blmap_arr[i]);
		if (blmap_arr[i] < 10)
			j += 3;
		else if (blmap_arr[i] < 100)
			j += 4;
		else
			j += 5;
	}

	buf[j] = '\n';
	++j;
	buf[j] = '}';
	++j;

	return j;
}

static ssize_t lcd_backlight_store_blmap(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	int i;
	int j;
	int value, ret;

	if (count < 1)
		return count;

	if (buf[0] != '{')
		return -EINVAL;

	for (i = 1, j = 0; i < count && j < 256; ++i) {
		if (!isdigit(buf[i]))
			continue;
		ret = sscanf(&buf[i], "%d", &value);

		if (ret < 1)
			pr_err("read error\n");
		blmap_arr[j] = (unsigned int)value;

		while (isdigit(buf[i]))
			++i;
		++j;
	}

	return count;
}

static ssize_t show_rawdata(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "lm36272 brightness code : %d\n",get_backlight_brightness_rawdata());
}

static ssize_t store_rawdata(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	char *pvalue = NULL;
	unsigned int level = 0;
	size_t size = 0;

	level = simple_strtoul(buf,&pvalue,10);
	size = pvalue - buf;

	if (*pvalue && isspace(*pvalue))
		size++;

	CPD_LOG("%s : [%d] \n",__func__,level);
	set_backlight_brightness_rawdata(level);

	return count;
}

static ssize_t sysfs_show_dsv(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	int n = 0;
	int err = 0;
	unsigned char flash_OnOff=0;

	err = lm36272_smbus_read_byte(new_client, 0x01, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x01 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x02, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x02 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x03, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x03 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x04, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x04 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x05, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x05 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x06, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x06 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x07, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x07 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x08, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x08 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x09, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x09 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x0A, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x0A 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x0B, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x0B 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x0C, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x0C 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x0D, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x0D 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x0E, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x0E 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x0F, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x0F 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x0E, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x0E 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x10, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x10 0x%x \n",flash_OnOff);

	err = lm36272_smbus_read_byte(new_client, 0x11, &flash_OnOff);
	n +=  sprintf(buf+n, "FLASH 0x11 0x%x \n",flash_OnOff);
	return n;
}

static ssize_t sysfs_store_dsv(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	int value;
	int addr;
	int err = 0;
	sscanf(buf, "%d %d",&addr, &value);

	err = lm36272_smbus_write_byte(new_client, addr, (unsigned char*)&value);
	return count;
}

DEVICE_ATTR(lm36272_reg, 0644, sysfs_show_dsv, sysfs_store_dsv);
DEVICE_ATTR(brightness_code, 0644, show_rawdata, store_rawdata);
DEVICE_ATTR(bl_blmap, 0644, lcd_backlight_show_blmap, lcd_backlight_store_blmap);

static int lm36272_parse_dt(void)
{
	LM36272_BLED_DSV_DATA *pdata = NULL;
	struct device_node *np = NULL;
    u8 tmp = 0;
	u32 tmp2 = 0;

	pdata = kzalloc(sizeof(LM36272_BLED_DSV_DATA), GFP_KERNEL);
	if(!pdata){
		CPD_ERR("falied to alloc memory for lm36272 pdata \n");
		return -ENOMEM;
	}

	g_pdata = pdata;

    np = of_find_compatible_node(NULL, NULL, LM36272_DTS_NODE);

    if (!np) {
	    CPD_ERR("[%s] DT node: Not found\n", __func__);
        return -1;
    } else {
        if (of_property_read_u8(np, "bl_ovp", &tmp) >= 0)
            pdata->bl_ovp = tmp;
        if (of_property_read_u8(np, "ovp_mode", &tmp) >= 0)
            pdata->ovp_mode = tmp;
        if (of_property_read_u8(np, "pwm_ramp", &tmp) >= 0)
            pdata->pwm_ramp = tmp;
        if (of_property_read_u8(np, "pwm_enable", &tmp) >= 0)
            pdata->pwm_enable = tmp;
        if (of_property_read_u8(np, "boost_freq", &tmp) >= 0)
            pdata->boost_freq = tmp;
        if (of_property_read_u8(np, "pwm_fsample", &tmp) >= 0)
            pdata->pwm_fsample = tmp;
		if (of_property_read_u8(np, "vpos_disch", &tmp) >= 0)
            pdata->vpos_disch = tmp;
        if (of_property_read_u8(np, "vneg_disch", &tmp) >= 0)
            pdata->vneg_disch = tmp;
		if (of_property_read_u8(np, "ext_en", &tmp) >= 0)
            pdata->ext_en = tmp;
		if (of_property_read_u32(np, "vpos", &tmp2) >= 0)
            pdata->vpos = (tmp2 - 4000) / 50;
        if (of_property_read_u32(np, "vneg", &tmp2) >= 0)
            pdata->vneg = (tmp2 - 4000) / 50;

#ifdef CONFIG_LGE_USE_BRIGHTNESS_TABLE
		mt_backlight_brightness_parse_dt(np);
#endif
    }

	return 0;
}

static int lm36272_i2c_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;

	CPD_FUN();

    new_client = client;

    ret = lm36272_parse_dt();

    //lm36272_bled_init_register();
	//lm36272_dsv_init_register();

    device_create_file(&client->dev, &dev_attr_bl_blmap);
	device_create_file(&client->dev, &dev_attr_brightness_code);
	device_create_file(&client->dev, &dev_attr_lm36272_reg);

	return ret;
}

static int lm36272_i2c_driver_remove(struct i2c_client *client)
{
	new_client = NULL;
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id lm36272_of_match[] = {
    {.compatible = "mediatek,i2c_lm36272",},
    {},
};

MODULE_DEVICE_TABLE(of, lm36272_of_match);
#endif

static struct i2c_driver lm36272_i2c_driver = {
	.driver = {
	// .owner  = THIS_MODULE,
	.name   = LM36272_DEV_NAME,
#ifdef CONFIG_OF
	.of_match_table = lm36272_of_match,
#endif
	},
	.probe      = lm36272_i2c_driver_probe,
	.remove     = lm36272_i2c_driver_remove,
	.id_table   = lm36272_i2c_id,
};

static int __init lm36272_init(void)
{
	if(i2c_add_driver(&lm36272_i2c_driver)){
		CPD_ERR("i2c add driver error\n");
		return -1;
	} else {
		CPD_LOG("i2c_add_driver OK\n");
	}

    CPD_LOG("lm36272_init success\n");

    return 0;
}

static void __exit lm36272_exit(void)
{
	if(g_pdata)
		kfree(g_pdata);

	CPD_LOG("unregister lm36272 i2c driver!\n");
}

MODULE_AUTHOR("Albert Zhang <xu.zhang@bosch-sensortec.com>");
MODULE_DESCRIPTION("lm36272 driver");
MODULE_LICENSE("GPL");

module_init(lm36272_init);
module_exit(lm36272_exit);
