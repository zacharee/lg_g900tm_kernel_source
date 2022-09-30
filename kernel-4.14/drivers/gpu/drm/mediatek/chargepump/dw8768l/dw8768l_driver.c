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

#include "dw8768l.h"

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */

#define CPD_TAG                 "[chargepump][dw8768l]"
#define CPD_FUN(f)              printk(CPD_TAG"%s\n", __FUNCTION__)
#define CPD_ERR(fmt, args...)   printk(CPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define CPD_LOG(fmt, args...)   printk(CPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

static struct i2c_client *dw8768l_client = NULL;
static const struct i2c_device_id dw8768l_i2c_id[] = {{DW8768L_DEV_NAME,0},{}};
static const struct of_device_id dw8768l_i2c_of_match[] = {{.compatible = "mediatek,i2c_dw8768l"}};

unsigned char dsv_data[5] = {
  0x00,
  0x00,
  0x80,
  0x0F,
  0x00,
};

unsigned char default_dsv_data[5] = {
  0x0F,
  0x0F,
  0x83,
  0x0F,
  0x00,
};

// Add semaphore for lcd and flash i2c communication.
//struct semaphore dw8768l_lock;

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#ifdef CONFIG_DW8768L_SUPPORT_BACKLIGHT
#ifdef CONFIG_MTK_AAL_SUPPORT
#define REG_BL_ENABLE   0x09

void dw8767_reg_bl_en_ctrl(unsigned int enable)
{
  unsigned char reg_data = 0x00;
  unsigned char bl_en_data = 0x00;
  unsigned char write_data = 0x00;

  dw8767_smbus_read_byte(dw8768l_client, 0x0A, &reg_data);

  bl_en_data = reg_data;

  if(enable)
      bl_en_data |= REG_BL_ENABLE;
  else
      bl_en_data &= (~REG_BL_ENABLE);

  write_data = bl_en_data;
  dw8767_smbus_write_byte(dw8768l_client, 0x0A, &write_data);

  CPD_LOG("%s : bl_en %s(0x%x)\n",__func__,(enable)? "enable":"disable",write_data);
}

void dw8767_check_pwm_enable(void)
{
  unsigned char pwm_enable = 0;

  dw8767_smbus_read_byte(dw8768l_client, 0x09, &pwm_enable);
  pwm_enable |= 0x40;
  dw8767_smbus_write_byte(dw8768l_client, 0x09, &pwm_enable);

  CPD_LOG("%s for ESS : 0x%x\n",__func__,pwm_enable);
}
#endif

static char dw8768l_i2c_read(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
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

static int dw8768l_smbus_read_byte(struct i2c_client *client, unsigned char reg_addr, unsigned char *data)
{
  return dw8768l_i2c_read(client,reg_addr,data,1);
}

static char dw8768l_i2c_write(struct i2c_client *client, u8 reg_addr, u8 *data, u8 len)
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

static int dw8768l_smbus_write_byte(struct i2c_client *client, unsigned char reg_addr, unsigned char *data)
{
  int i = 0;
  int ret_val = 0;

  ret_val = dw8768l_i2c_write(client,reg_addr,data,1);

  for (i = 0; i < 5; i++) {
    if (ret_val < 0) {
      dw8768l_i2c_write(client,reg_addr,data,1);
    } else {
      CPD_LOG("dw8768l_smbus_write_byte fail: %d\n",ret_val);
      return ret_val;
    }
  }

  return ret_val;
}
#endif

static int dw8768l_read_byte(unsigned char addr, unsigned char *value)
{
    int ret = 0;

    if (NULL == dw8768l_client) {
        CPD_ERR("dw8768l_client is null!!\n");
        return -1;
    }
    ret = i2c_smbus_read_i2c_block_data(dw8768l_client, addr, 1, value);

    if (ret < 0)
        CPD_ERR("i2c write data fail !!\n");

    return ret;
}

static int dw8768l_write_byte(unsigned char addr, unsigned char value)
{
  int ret = 0;
  unsigned char write_data[2] = {0,};

  write_data[0] = addr;
  write_data[1] = value;

  if (NULL == dw8768l_client) {
    CPD_ERR("dw8768l_client is null!!\n");
    ret = -1;
    goto exit;
  }

  ret = i2c_master_send(dw8768l_client, write_data, 2);

  if (ret < 0)
    CPD_ERR("i2c write data fail !!\n");

exit:
  return ret;
}

void dw8768l_ctrl(unsigned int enable)
{
  unsigned char data = 0x00;

  if(enable) {
    // ENA enable
    dw8768l_write_byte(LCD_BIAS_ENA_ADDR, dsv_data[ENAR]);
    mdelay(1);

    // VPOS +5.5v
    dw8768l_write_byte(LCD_BIAS_VPOS_ADDR, dsv_data[VPOS]);
    mdelay(1);

    // VNEG -5.5v
    dw8768l_write_byte(LCD_BIAS_VNEG_ADDR, dsv_data[VNEG]);
  } else {
    // ENA disable
    data = dsv_data[ENAR];
    data &= 0xF7;
    dw8768l_write_byte(LCD_BIAS_ENA_ADDR, data);
    mdelay(1);

    // floating
    data = dsv_data[DISC];
    data &= 0xFC;
    dw8768l_write_byte(LCD_BIAS_DISC_ADDR,data);
  }

  CPD_LOG("%s : %s\n",__func__, (enable)? "enable":"disable");
}

static ssize_t sysfs_show_dsv(struct device *dev, struct device_attribute *attr, char *buf)
{
    int n = 0;
    int err = 0;
    u8 data = 0;

    err = dw8768l_read_byte(LCD_BIAS_VPOS_ADDR, &data);
    n +=  sprintf(buf+n, "VPOS = 0x%02x\n",data);
    err = dw8768l_read_byte(LCD_BIAS_VNEG_ADDR, &data);
    n +=  sprintf(buf+n, "VNEG = 0x%02x\n",data);
    err = dw8768l_read_byte(LCD_BIAS_DISC_ADDR, &data);
    n +=  sprintf(buf+n, "DISC = 0x%02x\n",data);
    err = dw8768l_read_byte(LCD_BIAS_ENA_ADDR, &data);
    n +=  sprintf(buf+n, "ENAR = 0x%02x\n",data);
    err = dw8768l_read_byte(LCD_BIAS_KNOCK_ADDR, &data);
    n +=  sprintf(buf+n, "KNOK = 0x%02x\n",data);
    return n;
}

DEVICE_ATTR(dw8768l_reg, 0444, sysfs_show_dsv, NULL);

static int dw8768l_parse_dt(void)
{
    struct device_node *np = NULL;
    struct device_node *sub_np = NULL;
    int ret = 0;
    u32 tmp = 0;

    np = of_find_compatible_node(NULL, NULL, DW8768L_DTS_NODE);

    sub_np = of_get_child_by_name(np, "primary");

    if (!np || !sub_np) {
      CPD_ERR("[%s] DT node: Not found & default dsv data is used\n", __func__);
      memcpy(dsv_data, default_dsv_data, 5);
      ret = -1;
      goto exit;
    } else {
      if (of_property_read_u32(sub_np, "vpos", &tmp) < 0)
        dsv_data[VPOS] |= 15; //default 5.5V for VPOS
      else
        dsv_data[VPOS] |= (tmp - 4000) / 100;

      if (of_property_read_u32(sub_np, "vneg", &tmp) < 0)
        dsv_data[VNEG] |= 15; //default -5.5V for VNEG
      else
        dsv_data[VNEG] |= (tmp - 4000) / 100;

      if (of_property_read_u32(sub_np, "discharge", &tmp) < 0)
        dsv_data[DISC] |= 3; //default active discharge enable
      else
        dsv_data[DISC] |= tmp;

      if (of_property_read_u32(sub_np, "knock-on", &tmp) < 0)
        dsv_data[KNOK] |= 0; //default knock on disable
      else
        dsv_data[KNOK] |= tmp;

      CPD_LOG("VPOS = 0x%x, VNEG = 0x%x, DISC = 0x%x\n", dsv_data[VPOS], dsv_data[VNEG], dsv_data[DISC]);
    }
    return 0;
exit:
    return ret;
}

static int dw8768l_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
  int ret = 0;

  if (NULL == client) {
    CPD_ERR("i2c_client is NULL\n");
    ret = -1;
    goto exit;
  }

  dw8768l_client = client;

  ret = dw8768l_parse_dt();

  if(ret)
    CPD_ERR("parsing dt failed\n");

  device_create_file(&client->dev, &dev_attr_dw8768l_reg);

  CPD_LOG("%s addr = 0x%x\n",__func__,dw8768l_client->addr);

exit:
  return ret;
}

static int dw8768l_remove(struct i2c_client *client)
{
  dw8768l_client = NULL;
  i2c_unregister_device(client);

  return 0;
}

static struct i2c_driver dw8768l_i2c_driver = {
  .id_table   = dw8768l_i2c_id,
  .probe      = dw8768l_probe,
  .remove     = dw8768l_remove,
  .driver = {
    .owner = THIS_MODULE,
    .name   = DW8768L_DEV_NAME,
    .of_match_table = dw8768l_i2c_of_match,
  },
};

static int __init dw8768l_init(void)
{
  if (i2c_add_driver(&dw8768l_i2c_driver) != 0)
    CPD_ERR("Failed to register dw8768l driver");

  CPD_LOG("%s\n",__func__);

  return 0;
}

static void __exit dw8768l_exit(void)
{
  i2c_del_driver(&dw8768l_i2c_driver);
}

MODULE_DESCRIPTION("dw8768l driver");
MODULE_LICENSE("GPL");

module_init(dw8768l_init);
module_exit(dw8768l_exit);
