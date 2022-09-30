/*
* This software program is licensed subject to the GNU General Public License
* (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

* (C) Copyright 2011 Bosch Sensortec GmbH
* All Rights Reserved
*/


/* file dw8767.c
brief This file contains all function implementations for the dw8767 in linux
this source file refer to MT6572 platform
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
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/semaphore.h>

#include "lm3697.h"

#ifdef CONFIG_LGE_DISPLAY_COMMON
#include "lge_brightness.h"
#endif

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */

#define CPD_TAG                 "[chargepump][lm3697]"
#define CPD_FUN(f)              printk(CPD_TAG"%s\n", __FUNCTION__)
#define CPD_ERR(fmt, args...)   printk(CPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define CPD_LOG(fmt, args...)   printk(CPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)

#define MIN_VALUE_SETTINGS 10 /* value leds_brightness_set*/
#define MAX_VALUE_SETTINGS 255 /* value leds_brightness_set*/
#define MIN_MAX_SCALE(x) (((x)<MIN_VALUE_SETTINGS) ? MIN_VALUE_SETTINGS : (((x)>MAX_VALUE_SETTINGS) ? MAX_VALUE_SETTINGS:(x)))

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

static struct i2c_client *lm3697_client = NULL;
static const struct i2c_device_id lm3697_i2c_id[] = {{LM3697_DEV_NAME,0},{}};
static const struct of_device_id lm3697_i2c_of_match[] = {{.compatible = "mediatek,i2c_lm3697"}};
static LM3697_BLED_DSV_DATA *g_pdata = NULL;

struct semaphore lm3697_lock;
static bool is_suspend = false;
/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

static char lm3697_i2c_read(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
{
  s32 dummy;

  if (NULL == client)
    return -1;

  while (0 != len--) {
    dummy = i2c_smbus_read_byte_data(client, reg_addr);
    if (dummy < 0) {
      CPD_ERR("i2c bus read error\n");
      return -1;
    }

    reg_addr++;
    data++;
  }

  return 0;
}

static int lm3697_smbus_read_byte(struct i2c_client *client, unsigned char reg_addr, unsigned char *data)
{
  return lm3697_i2c_read(client,reg_addr,data,1);
}

static char lm3697_i2c_write(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
{
  s32 dummy;

  if (NULL == client)
    return -1;

  while (0 != len--) {
    dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
    if (dummy < 0) {
      CPD_ERR("i2c bus read error\n");
      return -1;
    }

    reg_addr++;
    data++;

    if (dummy < 0)
      return -1;
  }

  return 0;
}

static int lm3697_smbus_write_byte(struct i2c_client *client, unsigned char reg_addr, unsigned char *data)
{
  int i = 0;
  int ret_val = 0;

  ret_val = lm3697_i2c_write(client,reg_addr,data,1);

  for (i = 0; i < 5; i++) {
    if (ret_val < 0) {
      lm3697_i2c_write(client,reg_addr,data,1);
    } else {
      CPD_LOG("lm3697_smbus_write_byte fail: %d\n",ret_val);
      return ret_val;
    }
  }

  return ret_val;
}

static int lm3697_reg_update_bits(struct i2c_client *client, unsigned char addr, unsigned char mask, unsigned char data)
{
	unsigned char orig = 0;
	int ret = 0;

	CPD_LOG("%s: reg %02x data %02x\n", __func__, addr, data);
	//CPD_LOG("%s: mask %02x\n", __func__, mask);

	ret = lm3697_smbus_read_byte(client, addr, &orig);
	if (ret < 0)
		return -1;
	orig &= ~mask;
	orig |= (data & mask);
	ret = lm3697_smbus_write_byte(client, addr, &orig);

	return ret;
}

static void lm3697_bled_init_register(void)
{
  char data = 0;

  //BLED init
  data |= (g_pdata->hvled1 << HVLED1_CONFIG_SHFT);
  data |= (g_pdata->hvled2 << HVLED2_CONFIG_SHFT);
  data |= (g_pdata->hvled3 << HVLED3_CONFIG_SHFT);

  lm3697_smbus_write_byte(lm3697_client, LM3697_REG10_ADDR, &data);

  data = 0;
  data |= (g_pdata->mapping_mode << MAPPING_MODE_SHFT);
  data |= (g_pdata->dither_a << DITHER_DISABLE_CONTROL_A_SHFT);
  data |= (g_pdata->dither_b << DITHER_DISABLE_CONTROL_B_SHFT);

  lm3697_smbus_write_byte(lm3697_client, LM3697_REG16_ADDR, &data);

	lm3697_reg_update_bits(lm3697_client, LM3697_REG17_ADDR, FULL_SCALE_CURRENT_MASK, (g_pdata->full_scale_cur << FULL_SCALE_CURRENT_SHFT));
	lm3697_reg_update_bits(lm3697_client, LM3697_REG18_ADDR, FULL_SCALE_CURRENT_MASK, (g_pdata->full_scale_cur << FULL_SCALE_CURRENT_SHFT));

  lm3697_reg_update_bits(lm3697_client, LM3697_REG1A_ADDR, BOOST_FREQUENCY_MASK, (g_pdata->boost_freq << BOOST_FREQUENCY_SHFT));
	lm3697_reg_update_bits(lm3697_client, LM3697_REG1A_ADDR, BOOST_OVP_MASK, (g_pdata->boost_ovp << BOOST_OVP_SHFT));

  lm3697_reg_update_bits(lm3697_client, LM3697_REG1C_ADDR, PWM_ENABLE_A_MASK, (g_pdata->pwm_en_a << PWM_ENABLE_A_SHFT));
	lm3697_reg_update_bits(lm3697_client, LM3697_REG1C_ADDR, PWM_ENABLE_B_MASK, (g_pdata->pwm_en_b << PWM_ENABLE_B_SHFT));

  CPD_LOG("-init register\n");
}

int chargepump_set_backlight_level(unsigned int level)
{
	int ret;
	unsigned int data = 0;
	unsigned char data1 = 0;
	unsigned char lsb_data = 0x00; // 3bit
	unsigned char msb_data = 0x00; // 8bit

	CPD_LOG("chargepump_set_backlight_level  [%d]\n",level);

	if (level == 0){
		if(is_suspend == false){
			CPD_LOG( "backlight off\n");
			ret = down_interruptible(&lm3697_lock); // Add semaphore for lcd and flash i2c communication.
			data1 = 0x00; //backlight2 brightness 0
			if(g_pdata->control_a_en) {
			  lm3697_smbus_write_byte(lm3697_client, LM3697_REG20_ADDR, &data1); // LSB
			  lm3697_smbus_write_byte(lm3697_client, LM3697_REG21_ADDR, &data1); // MSB
        lm3697_reg_update_bits(lm3697_client, LM3697_REG24_ADDR, CONTROL_A_ENABLE_MASK, (0 << CONTROL_A_ENABLE_SHFT));
      }

      if(g_pdata->control_b_en) {
			  lm3697_smbus_write_byte(lm3697_client, LM3697_REG22_ADDR, &data1); // LSB
			  lm3697_smbus_write_byte(lm3697_client, LM3697_REG23_ADDR, &data1); // MSB
        lm3697_reg_update_bits(lm3697_client, LM3697_REG24_ADDR, CONTROL_B_ENABLE_MASK, (0 << CONTROL_B_ENABLE_SHFT));
      }

			is_suspend = true;// Move backlight suspend setting position into semaphore
			up(&lm3697_lock); // Add semaphore for lcd and flash i2c communication.
		}
	} else {
		level = MIN_MAX_SCALE(level);
#ifdef CONFIG_LGE_DISPLAY_COMMON
		data = lge_get_brightness_mapping_value(level);
#endif

		if (is_suspend == true){
			is_suspend = false;
			ret = down_interruptible(&lm3697_lock); // Add semaphore for lcd and flash i2c communication.

			lm3697_bled_init_register();

			lsb_data = (data) & 0x07; // 3bit LSB
			msb_data = (data >> 3) & 0xFF; // 8bit MSB

      if(g_pdata->control_a_en) {
			  lm3697_smbus_write_byte(lm3697_client, LM3697_REG20_ADDR, &lsb_data); // LSB
			  lm3697_smbus_write_byte(lm3697_client, LM3697_REG21_ADDR, &msb_data); // MSB
      }

      if(g_pdata->control_b_en) {
			  lm3697_smbus_write_byte(lm3697_client, LM3697_REG22_ADDR, &lsb_data); // LSB
			  lm3697_smbus_write_byte(lm3697_client, LM3697_REG23_ADDR, &msb_data); // MSB
      }

			CPD_LOG("-backlight brightness Setting[LSB:0x%x]\n",lsb_data);
			CPD_LOG("-backlight brightness Setting[MSB:0x%x]\n",msb_data);

			lm3697_reg_update_bits(lm3697_client, LM3697_REG24_ADDR, CONTROL_A_ENABLE_MASK, (1 << CONTROL_A_ENABLE_SHFT));
	    lm3697_reg_update_bits(lm3697_client, LM3697_REG24_ADDR, CONTROL_B_ENABLE_MASK, (1 << CONTROL_B_ENABLE_SHFT));

			// Move backlight suspend setting position into semaphore
			up(&lm3697_lock); // Add semaphore for lcd and flash i2c communication.
		}

		if (level != 0) // Move backlight suspend setting position into semaphore
		{
			ret = down_interruptible(&lm3697_lock); // Add semaphore for lcd and flash i2c communication.

			lsb_data = (data) & 0x07; // 3bit LSB
			msb_data = (data >> 3) & 0xFF; // 8bit MSB

			if(g_pdata->control_a_en) {
			  lm3697_smbus_write_byte(lm3697_client, LM3697_REG20_ADDR, &lsb_data); // LSB
			  lm3697_smbus_write_byte(lm3697_client, LM3697_REG21_ADDR, &msb_data); // MSB
      }

      if(g_pdata->control_b_en) {
			  lm3697_smbus_write_byte(lm3697_client, LM3697_REG22_ADDR, &lsb_data); // LSB
			  lm3697_smbus_write_byte(lm3697_client, LM3697_REG23_ADDR, &msb_data); // MSB
      }

			CPD_LOG("-backlight brightness Setting[LSB:0x%x]\n",lsb_data);
			CPD_LOG("-backlight brightness Setting[MSB:0x%x]\n",msb_data);

			up(&lm3697_lock); // Add semaphore for lcd and flash i2c communication.
		} // Move backlight suspend setting position into semaphore
	}
	return 0;
}

static ssize_t sysfs_show_dsv(struct device *dev, struct device_attribute *attr, char *buf)
{
    int n = 0;
    int err = 0;
    u8 data = 0;

    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG10_ADDR, &data);
    n +=  sprintf(buf+n, "REG10 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG11_ADDR, &data);
    n +=  sprintf(buf+n, "REG11 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG12_ADDR, &data);
    n +=  sprintf(buf+n, "REG12 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG13_ADDR, &data);
    n +=  sprintf(buf+n, "REG13 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG14_ADDR, &data);
    n +=  sprintf(buf+n, "REG14 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG16_ADDR, &data);
    n +=  sprintf(buf+n, "REG16 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG17_ADDR, &data);
    n +=  sprintf(buf+n, "REG17 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG18_ADDR, &data);
    n +=  sprintf(buf+n, "REG18 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG19_ADDR, &data);
    n +=  sprintf(buf+n, "REG19 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG1A_ADDR, &data);
    n +=  sprintf(buf+n, "REG1A = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG1B_ADDR, &data);
    n +=  sprintf(buf+n, "REG1B = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG1C_ADDR, &data);
    n +=  sprintf(buf+n, "REG1C = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG20_ADDR, &data);
    n +=  sprintf(buf+n, "REG20 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG21_ADDR, &data);
    n +=  sprintf(buf+n, "REG21 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG22_ADDR, &data);
    n +=  sprintf(buf+n, "REG22 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG23_ADDR, &data);
    n +=  sprintf(buf+n, "REG23 = 0x%02x\n",data);
    err = lm3697_smbus_read_byte(lm3697_client, LM3697_REG24_ADDR, &data);
    n +=  sprintf(buf+n, "REG24 = 0x%02x\n",data);
    return n;
}

DEVICE_ATTR(lm3697_reg, 0444, sysfs_show_dsv, NULL);

static int lm3697_parse_dt(void)
{
  LM3697_BLED_DSV_DATA *pdata = NULL;
	struct device_node *np = NULL;
  u8 tmp = 0;
	u32 tmp2 = 0;

	pdata = kzalloc(sizeof(LM3697_BLED_DSV_DATA), GFP_KERNEL);
	if(!pdata){
		CPD_ERR("falied to alloc memory for lm3697 pdata \n");
		return -ENOMEM;
	}

	g_pdata = pdata;

  np = of_find_compatible_node(NULL, NULL, LM3697_DTS_NODE);

  if (!np) {
    CPD_ERR("[%s] DT node: Not found\n", __func__);
    return -1;
  } else {
    if (of_property_read_u8(np, "hvled1", &tmp) >= 0)
      pdata->hvled1 = tmp;
    if (of_property_read_u8(np, "hvled2", &tmp) >= 0)
      pdata->hvled2 = tmp;
    if (of_property_read_u8(np, "hvled3", &tmp) >= 0)
      pdata->hvled3 = tmp;
    if (of_property_read_u8(np, "mapping_mode", &tmp) >= 0)
      pdata->mapping_mode = tmp;
    if (of_property_read_u8(np, "dither_a", &tmp) >= 0)
      pdata->dither_a = tmp;
    if (of_property_read_u8(np, "dither_b", &tmp) >= 0)
      pdata->dither_b = tmp;
    if (of_property_read_u32(np, "full_scale_cur", &tmp2) >= 0)
      pdata->full_scale_cur = tmp2;
    if (of_property_read_u8(np, "sink_feedback_a", &tmp) >= 0)
      pdata->sink_feedback_a = tmp;
    if (of_property_read_u8(np, "sink_feedback_b", &tmp) >= 0)
      pdata->sink_feedback_b = tmp;
    if (of_property_read_u8(np, "sink_feedback_c", &tmp) >= 0)
      pdata->sink_feedback_c = tmp;
    if (of_property_read_u8(np, "boost_freq", &tmp) >= 0)
      pdata->boost_freq = tmp;
    if (of_property_read_u8(np, "boost_ovp", &tmp) >= 0)
      pdata->boost_ovp = tmp;
    if (of_property_read_u8(np, "pwm_en_a", &tmp) >= 0)
      pdata->pwm_en_a = tmp;
    if (of_property_read_u8(np, "pwm_en_b", &tmp) >= 0)
      pdata->pwm_en_b = tmp;
    if (of_property_read_u8(np, "control_a_en", &tmp) >= 0)
      pdata->control_a_en = tmp;
    if (of_property_read_u8(np, "control_b_en", &tmp) >= 0)
      pdata->control_b_en = tmp;
  }

  CPD_LOG("-parse dt success\n");

  return 0;
}

static int lm3697_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
  int ret = 0;

  if (NULL == client) {
    CPD_ERR("i2c_client is NULL\n");
    ret = -1;
    goto exit;
  }

  lm3697_client = client;

  ret = lm3697_parse_dt();

  if(ret)
    CPD_ERR("parsing dt failed\n");

  sema_init(&lm3697_lock, 1);

  device_create_file(&client->dev, &dev_attr_lm3697_reg);

  CPD_LOG("%s addr = 0x%x\n",__func__,lm3697_client->addr);

exit:
  return ret;
}

static int lm3697_remove(struct i2c_client *client)
{
  lm3697_client = NULL;
  i2c_unregister_device(client);

  return 0;
}

static struct i2c_driver lm3697_i2c_driver = {
  .id_table   = lm3697_i2c_id,
  .probe      = lm3697_probe,
  .remove     = lm3697_remove,
  .driver = {
    .owner = THIS_MODULE,
    .name   = LM3697_DEV_NAME,
    .of_match_table = lm3697_i2c_of_match,
  },
};

static int __init lm3697_init(void)
{
  if (i2c_add_driver(&lm3697_i2c_driver) != 0)
    CPD_ERR("Failed to register lm3697 driver");

  CPD_LOG("%s\n",__func__);

  return 0;
}

static void __exit lm3697_exit(void)
{
  i2c_del_driver(&lm3697_i2c_driver);
}

MODULE_DESCRIPTION("lm3697 driver");
MODULE_LICENSE("GPL");

module_init(lm3697_init);
module_exit(lm3697_exit);