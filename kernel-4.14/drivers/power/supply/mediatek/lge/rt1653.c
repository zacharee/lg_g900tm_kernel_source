/*
 *  Copyright (C) 2019 Richtek Technology Corp.
 *  Author : gene_chen <gene_chen@richtek.com>
 *           shufan_lee <shufan_lee@richtek.com>
 *           lucas_tsai <lucas_tsai@richtek.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/crc16.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>

#include "charger_class.h"
#include <mt-plat/mtk_charger.h>
#include <mt-plat/rt-regmap.h>
#include "tcpm.h"

#undef CONFIG_MTP_PROGRAM
#ifdef CONFIG_MTP_PROGRAM
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#endif

#define RT1653_DRV_VERSION	"1.0.8_MTK"

enum rt1653_reg_addr {
	RT1653_REG_LDO_ILIM_EN = 0x31,
	RT1653_REG_LDO_CONTROL,
	RT1653_REG_ILIM_CONTROL,
	RT1653_REG_SYSTEM_STATUS = 0xD6,
	RT1653_REG_ADC_CONTROL,
	RT1653_REG_ADC_DATA1,
	RT1653_REG_ADC_DATA2,
	RT1653_REG_MTP_VENDOR = 0xDE,
	RT1653_REG_MTP_VERSION,
};

enum rt1653_adc_channel {
	VRECT_CHANNEL,
	IOUT_CHANNEL,
	VOUT_CHANNEL,
	VTS_CHANNEL,
	VTJ_CHANNEL,
};

/* ========== LDO_ILIM_EN 0x31 ============ */
#define RT1653_ENI2CLDO_SHIFT		7
#define RT1653_ENI2CILIM_SHIFT		6

#define RT1653_ENI2CLDO_MASK		BIT(RT1653_ENI2CLDO_SHIFT)
#define RT1653_ENI2CILIM_MASK		BIT(RT1653_ENI2CILIM_SHIFT)

/* ========== LDO_CONTROL 0x32 ============ */
#define RT1653_VLDO_SHIFT		0

#define RT1653_VLDO_MASK		0xFF

#define RT1653_VLDO_MIN			0
#define RT1653_VLDO_MAX			12240000
#define RT1653_VLDO_STEP		48000

/* ========== ILIM_CONTROL 0x33 ============ */
#define RT1653_ILIM_SHIFT		0

#define RT1653_ILIM_MASK		0xFF

/* ========== SYSTEM_STATUS 0xD6 ============ */
#define RT1653_EPP_SHIFT		7
#define RT1653_GPIO0_SHIFT		6
#define RT1653_GPIO1_SHIFT		5
#define RT1653_LP_EPP_SHIFT		4
#define RT1653_AP_READY_SHIFT		0

#define RT1653_EPP_MASK			BIT(RT1653_EPP_SHIFT)
#define RT1653_GPIO0_MASK		BIT(RT1653_GPIO0_SHIFT)
#define RT1653_GPIO1_MASK		BIT(RT1653_GPIO1_SHIFT)
#define RT1653_LP_EPP_MASK		BIT(RT1653_LP_EPP_SHIFT)
#define RT1653_AP_READY_MASK		BIT(RT1653_AP_READY_SHIFT)

/* ========== ADC_CONTROL 0xD7 ============ */
#define RT1653_ADC_START_SHIFT		4
#define RT1653_ADC_CHANNEL_SHIFT	0

#define RT1653_ADC_START_MASK		BIT(RT1653_ADC_START_SHIFT)
#define RT1653_ADC_CHANNEL_MASK		0x0F

/* ========== ADC_DATA1 0xD8 ============ */
#define RT1653_ADC_DATA1_SHIFT		0

#define RT1653_ADC_DATA1_MASK		0xFF

/* ========== ADC_DATA2 0xD9 ============ */
#define RT1653_ADC_DATA2_SHIFT		0

#define RT1653_ADC_DATA2_MASK		0xFF

/* ========== MTP_VENDOR 0xDE ============ */
#define RT1653_MTP_VENDOR_SHIFT		0

#define RT1653_MTP_VENDOR_MASK		0xFF

/* ========== MTP_VERSION 0xDF ============ */
#define RT1653_MTP_VERSION_SHIFT	0

#define RT1653_MTP_VERSION_MASK		0xFF

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(RT1653_REG_LDO_ILIM_EN, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1653_REG_LDO_CONTROL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1653_REG_ILIM_CONTROL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1653_REG_SYSTEM_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1653_REG_ADC_CONTROL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1653_REG_ADC_DATA1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1653_REG_ADC_DATA2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1653_REG_MTP_VENDOR, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1653_REG_MTP_VERSION, 1, RT_VOLATILE, {});

static const rt_register_map_t rt1653_regmap[] = {
	RT_REG(RT1653_REG_LDO_ILIM_EN),
	RT_REG(RT1653_REG_LDO_CONTROL),
	RT_REG(RT1653_REG_ILIM_CONTROL),
	RT_REG(RT1653_REG_SYSTEM_STATUS),
	RT_REG(RT1653_REG_ADC_CONTROL),
	RT_REG(RT1653_REG_ADC_DATA1),
	RT_REG(RT1653_REG_ADC_DATA2),
	RT_REG(RT1653_REG_MTP_VENDOR),
	RT_REG(RT1653_REG_MTP_VERSION),
};

static struct rt_regmap_properties rt1653_regmap_props = {
	.name = "rt1653",
	.register_num = ARRAY_SIZE(rt1653_regmap),
	.rm = rt1653_regmap,
	.rt_regmap_mode = RT_CACHE_DISABLE | RT_DBG_SPECIAL,
	.aliases = "rt1653",
};

static int rt1653_read_device(void *client, u32 addr, int len, void *data);
static int rt1653_write_device(void *client, u32 addr,
					     int len, const void *data);
static struct rt_regmap_fops rt1653_regmap_fops = {
	.read_device = rt1653_read_device,
	.write_device = rt1653_write_device,
};
#endif

struct {
	u8 reg;
	u8 shift;
	u16 mask;
} stat_tbl[CHG_STAT_MAX] = {
	[WLESS_CHG_STAT_EPP] = {RT1653_REG_SYSTEM_STATUS, RT1653_EPP_SHIFT,
				RT1653_EPP_MASK},
	[WLESS_CHG_STAT_LP_EPP] = {RT1653_REG_SYSTEM_STATUS, RT1653_LP_EPP_SHIFT,
				RT1653_LP_EPP_MASK},
};

static const char *stat_name[CHG_STAT_MAX] = {
	[WLESS_CHG_STAT_EPP] = "EPP Status",
	[WLESS_CHG_STAT_LP_EPP] = "Low Power EPP Status",
};

#define MTP_DATA_SIZE		256
#define MTP_PAGE_SIZE		16
#define MTP_CRC_SIZE		2
#define RT1653_MTP_VEND_ID	(0xFD)
#define RT1653_MTP_VER_ID	(0xFE)
#define RT1653_MTP_VER_SIZE	2
#define RT1653_TM_EN		(0x10)
#define RT1653_TM_FW_BUSY	(0x80)
#define RT1653_TM_FAIL		(0x40)

#if 0
static u8 mtp_data[MTP_DATA_SIZE] = {
	0x00, 0x16, 0x06, 0x00, 0x05, 0x32, 0x0A, 0x00,
	0x00, 0x3E, 0x30, 0x12, 0x00, 0x37, 0x12, 0x34,
	0x56, 0x78, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x1E, 0x14, 0xFF, 0x01, 0x64, 0x00,
	0xEE, 0x0A, 0x00, 0xDC, 0x64, 0x1E, 0x14, 0x0B,
	0x19, 0x23, 0x05, 0x4A, 0x29, 0x25, 0x00, 0x01,
	0x00, 0x49, 0x60, 0x62, 0x62, 0x62, 0x62, 0x62,
	0x61, 0x5B, 0x5B, 0x5B, 0x62, 0x62, 0x62, 0x60,
	0x5E, 0x5E, 0x5E, 0x5E, 0x5E, 0x00, 0x00, 0x00,
	0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x04, 0xF6, 0xFF, 0x00, 0xFF, 0xFF,
	0xFF, 0x33, 0xFF, 0xFF, 0x80, 0xFF, 0xE7, 0x25,
	0x69, 0x96, 0x96, 0x00, 0xF8, 0xB4, 0x1E, 0x69,
	0x99, 0x0D, 0xF4, 0x1D, 0x3F, 0x2C, 0xF4, 0x1E,
	0xEE, 0x3D, 0x88, 0x34, 0x5F, 0x8A, 0x00, 0x00,
	0x14, 0x00, 0x06, 0x19, 0x07, 0x96, 0x32, 0xD2,
	0x00, 0x10, 0x00, 0x19, 0x19, 0xC0, 0x75, 0x00,
	0x00, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x6D, 0x09, 0xDA, 0xB2, 0x9F, 0x00, 0x10, 0x0A,
	0x00, 0x00, 0x00, 0xC8, 0x32, 0xC8, 0x01, 0x00,
	0x00, 0x00, 0x1E, 0x31, 0x03, 0x1E, 0x00, 0x8C,
	0x04, 0x55, 0x32, 0x03, 0x32, 0x00, 0x00, 0x00,
	0x03, 0x00, 0x00, 0x00, 0x00, 0x11, 0x80, 0xE2,
	0x0E, 0x82, 0x42, 0xEC, 0x00, 0x00, 0x0A, 0x14,
	0xD0, 0x39, 0x72, 0xF1, 0xA8, 0xA1, 0x64, 0xAB,
	0x00, 0x00, 0x00, 0x00, 0x82, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x55, 0xAA, 0x0C, 0x00, 0x32, 0x00,
	0xC8, 0x96, 0x32, 0x1E, 0xB4, 0x2C, 0x0B, 0xB4,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x75, 0x35, 0x97, 0x01,
	0x00, 0x00, 0xC2, 0x00, 0x0F, 0x00, 0x00, 0x00,
	0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00,
};
#endif

#ifdef CONFIG_MTP_PROGRAM
enum rt1653_mtp_ops {
	RT1653_MTP_READ,
	RT1653_MTP_ERASE,
	RT1653_MTP_WRITE,
};
#endif /* CONFIG_MTP_PROGRAM */

enum rt1653_gpio_val {
	RT1653_MODE0_GPIO,
	RT1653_MODE1_GPIO,
	RT1653_WLESS_GPIO,
	RT1653_MAX_GPIO,
};

struct rt1653_platform_data {
	struct gpio gpios[RT1653_MAX_GPIO];
};

struct rt1653_info {
	struct device *dev;
	struct i2c_client *i2c;
	int irq;
	int wless_gpio;
	struct delayed_work dwork;
	struct workqueue_struct *wqs;
	struct power_supply *psy;
	struct mutex attr_lock;
	struct mutex io_lock;
	bool wless_attached;
	bool en;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *rd;
#endif
	wait_queue_head_t wait_queue;

#ifdef CONFIG_MTP_PROGRAM
#ifdef REGULATOR_CTRL_CHG_PUMP /* use external chg pump 5V */
	struct regulator *chg_pump;
#endif
	u8 *raw_data;
	size_t raw_data_cnt;
	bool mtp_success;
	bool mtp_start;
	int mtp_retry_cnt;
#endif
};

struct dt_gpio_attr {
	const char *name;
	int idx;
};

static const struct dt_gpio_attr rt1653_dt_gpios[] = {
	{ .name = "mode_gpio",	.idx = 0 },
	{ .name = "mode_gpio",	.idx = 1 },
	{ .name = "io_gpio",	.idx = 0 },
};

static struct gpio rt1653_gpios[] = {
	{ .flags = GPIOF_OUT_INIT_LOW,	.label = "rt1653_mode0_gpio", },
#ifdef CONFIG_LGE_PM_WIRELESS_CHARGER
	{ .flags = GPIOF_OUT_INIT_LOW,	.label = "rt1653_mode1_gpio", },
#else /* Richtek */
	{ .flags = GPIOF_OUT_INIT_HIGH,	.label = "rt1653_mode1_gpio", },
#endif
	{ .flags = GPIOF_IN,		.label = "rt1653_wless_gpio", },
};

static const u8 rt1653_val_en_hidden_mode[] = {
	0x19, 0x86, 0x01, 0x09, 0x16, 0x88, 0x16, 0x88,
};

static int rt1653_parse_dt(struct device *dev,
			   struct rt1653_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(rt1653_gpios); i++) {
		ret = of_get_named_gpio(np,
					rt1653_dt_gpios[i].name,
					rt1653_dt_gpios[i].idx);
		if (ret < 0)
			return ret;
		rt1653_gpios[i].gpio = ret;
	}

	memcpy(pdata->gpios, rt1653_gpios, sizeof(rt1653_gpios));
	return 0;
}

static int rt1653_read_device(void *client, u32 addr, int len, void *data)
{
	int ret = 0, count = 3;
	struct i2c_client *i2c = client;

	while (count) {
		ret = i2c_smbus_read_i2c_block_data(i2c, addr, len, data);
		if (ret < 0)
			count--;
		else
			break;
		udelay(10);
	}
	return ret;
}

static int rt1653_write_device(void *client, u32 addr,
					     int len, const void *data)
{
	int ret = 0, count = 5;
	struct i2c_client *i2c = client;

	while (count) {
		ret = i2c_smbus_write_i2c_block_data(i2c, addr, len, data);
		if (ret < 0)
			count--;
		else
			break;
		udelay(10);
	}
	return ret;
}

static int rt1653_i2c_block_write(struct rt1653_info *ri, u8 addr, u8 len,
				  const u8 *data)
{
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt1653_write_device(ri->i2c, addr, len, data);
	mutex_unlock(&ri->io_lock);
	if (ret < 0)
		dev_err(ri->dev, "%s: I2CW[0x%02X] fail(%d), len = %d\n",
			__func__, addr, ret, len);
	return ret;
}

static int rt1653_i2c_update_bits(struct rt1653_info *ri, u8 addr,
				  u8 mask, u8 data)
{
	int ret = 0;
	u8 orig = 0;

	mutex_lock(&ri->io_lock);
	ret = rt1653_read_device(ri->i2c, addr, 1, &orig);
	if (ret < 0)
		goto out;

	orig &= ~mask;
	orig |= (data & mask);
	ret = rt1653_write_device(ri->i2c, addr, 1, &orig);
out:
	mutex_unlock(&ri->io_lock);
	if (ret < 0)
		dev_err(ri->dev,
		"%s: I2CU[0x%02X] = 0x%02X with mask(0x%02X) fail(%d)\n",
		__func__, addr, data, mask, ret);
	return ret;
}

static inline int rt1653_set_bit(struct rt1653_info *ri, u8 addr, u8 mask)
{
	return rt1653_i2c_update_bits(ri, addr, mask, mask);
}

static inline int rt1653_clr_bit(struct rt1653_info *ri, u8 addr, u8 mask)
{
	return rt1653_i2c_update_bits(ri, addr, mask, 0x00);
}

static int rt1653_i2c_read_byte(struct rt1653_info *ri, u8 addr)
{
	int ret = 0;
	u8 data = 0;

	mutex_lock(&ri->io_lock);
	ret = rt1653_read_device(ri->i2c, addr, 1, &data);
	mutex_unlock(&ri->io_lock);
	if (ret < 0) {
		dev_err(ri->dev, "%s: I2CR[0x%02X] fail(%d)\n",
				 __func__, addr, ret);
		return ret;
	}
	return data;
}

static int rt1653_i2c_write_byte(struct rt1653_info *ri, u8 addr, u8 data)
{
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt1653_write_device(ri->i2c, addr, 1, &data);
	mutex_unlock(&ri->io_lock);
	if (ret < 0) {
		dev_err(ri->dev, "%s: I2CW[0x%02X] = 0x%02X fail(%d)\n",
			__func__, addr, data, ret);
		return ret;
	}
	return data;
}

static int rt1653_i2c_block_read(struct rt1653_info *ri, u8 addr, u8 len,
				 u8 *data)
{
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt1653_read_device(ri->i2c, addr, len, data);
	mutex_unlock(&ri->io_lock);
	if (ret < 0)
		dev_err(ri->dev, "%s: I2CR[0x%02X] fail(%d), len = %d\n",
			__func__, addr, ret, len);
	return ret;
}

#ifdef CONFIG_MTP_PROGRAM
static int rt1653_wait_firmware_done(struct rt1653_info *ri)
{
	const int max_wait_times = 10;
	int i, ret = 0;

	for (i = 0; i < max_wait_times; i++) {
		usleep_range(1000, 1500);
		ret = rt1653_i2c_read_byte(ri, 0x93);
		if (ret >= 0 && (ret & RT1653_TM_FW_BUSY) == 0)
			break;
	}
	if (ret & RT1653_TM_FAIL) {
		dev_err(ri->dev, "%s: tm fail(%d)\n", __func__, ret);
		ret = -EINVAL;
	}
	if (i == max_wait_times) {
		dev_err(ri->dev, "%s: timeout(%d)\n", __func__, ret);
		ret = -EINVAL;
	}
	return ret;
}

static inline int rt1653_enable_prog_mode(struct rt1653_info *ri, bool en)
{
	int ret = 0;
	u8 val[] = {0x5A, 0xA5, 0x62, 0x86, 0x68, 0x26, 0x5A, 0xA5};
	bool tm_en;

	if (!en)
		val[7] = 0x00;
	ret = rt1653_i2c_block_write(ri, 0xF0, ARRAY_SIZE(val), val);
	if (ret < 0) {
		dev_err(ri->dev, "%s: enable prog mode fail(%d), en = %d\n",
			__func__, ret, en);
		return ret;
	}

	ret = rt1653_i2c_read_byte(ri, 0x21);
	if (ret < 0) {
		dev_err(ri->dev, "%s: read test mode enable fail(%d)\n",
			__func__, ret);
		return ret;
	}
	tm_en = (ret & RT1653_TM_EN) ? true : false;
	if (tm_en != en) {
		dev_err(ri->dev, "%s: check test mode fail, (ret,en) = (%d,%d)",
			__func__, ret, en);
		return -EINVAL;
	}
	return ret;
}

static int rt1653_init_ic_power(struct rt1653_info *ri)
{
	static const u8 pwr_addr[] = {0x10, 0x1F, 0x23, 0x24, 0x22};
	static const u8 pwr_val[] = {0x00, 0x80, 0x75, 0x20, 0x56};
	int i, ret;

	/* Initial IC power for programming */
	for (i = 0; i < ARRAY_SIZE(pwr_addr); i++) {
		ret = rt1653_i2c_write_byte(ri, pwr_addr[i], pwr_val[i]);
		if (ret < 0) {
			dev_err(ri->dev, "%s: fail(%d)\n", __func__, ret);
			return ret;
		}
	}
	return 0;
}

static int rt1653_init_mtp(struct rt1653_info *ri, enum rt1653_mtp_ops ops,
			   u8 addr, u8 data)
{
	int ret = 0;
	static const u8 ops_code[] = {0x80, 0x81, 0x82};

	ret = rt1653_i2c_write_byte(ri, 0xA1, 0x00);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set ifren fail(%d)\n", __func__, ret);
		return ret;
	}

	if (ops == RT1653_MTP_WRITE) {
		ret = rt1653_i2c_write_byte(ri, 0x64, data);
		if (ret < 0) {
			dev_err(ri->dev, "%s: set data fail(%d)\n", __func__,
								   ret);
			return ret;
		}
	}

	/* Set mtp addr */
	ret = rt1653_i2c_write_byte(ri, 0x68, addr);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set addr fail(%d)\n", __func__, ret);
		return ret;
	}

	ret = rt1653_i2c_write_byte(ri, 0x91, ops_code[ops]);
	if (ret < 0) {
		dev_err(ri->dev, "%s: set mode fail(%d)\n", __func__, ret);
		return ret;
	}

	ret = rt1653_i2c_write_byte(ri, 0x93, 0x01);
	if (ret < 0)
		dev_err(ri->dev, "%s: tm command en fail(%d)\n", __func__, ret);
	return ret;
}

static int __rt1653_read_mtp(struct rt1653_info *ri, u8 *data)
{
	int i, ret;

	for (i = 0; i < MTP_DATA_SIZE; i++) {
		ret = rt1653_init_mtp(ri, RT1653_MTP_READ, i, 0);
		if (ret < 0) {
			dev_err(ri->dev, "%s: init fail(%d)\n", __func__, ret);
			return ret;
		}

		ret = rt1653_wait_firmware_done(ri);
		if (ret < 0) {
			dev_err(ri->dev, "%s: wait fail(%d)\n", __func__, ret);
			return ret;
		}

		/* Read data */
		ret = rt1653_i2c_read_byte(ri, 0x65);
		if (ret < 0) {
			dev_err(ri->dev, "%s: read fail(%d)\n", __func__, ret);
			return ret;
		}
		data[i] = ret & 0xFF;
	}

	/* For debug, remove before release */
	for (i = 0; i < MTP_DATA_SIZE; i++)
		dev_info(ri->dev, "%s: mtp[0x%02X] = 0x%02X\n", __func__, i,
							   data[i]);

	return 0;
}

static int rt1653_read_mtp(struct rt1653_info *ri, u8 *data)
{
	int ret = 0;

	ret = rt1653_enable_prog_mode(ri, true);
	if (ret < 0) {
		dev_err(ri->dev, "%s: en prog fail(%d)\n", __func__, ret);
		return ret;
	}
	ret = __rt1653_read_mtp(ri, data);
	if (ret < 0)
		dev_err(ri->dev, "%s: read mtp fail(%d)\n", __func__, ret);
	return rt1653_enable_prog_mode(ri, false);
}

#pragma pack(push, 1)
struct mtp_header {
	u8 row_cnt;
	u8 data_cnt;
	u16 crc;
};
#pragma pack(pop)

static int rt1653_extract_raw_data(struct rt1653_info *ri, u8 *ext_data,
				   u8 *ext_mask)
{
	u8 row_cnt = 0, addr;
	u16 header_data_cnt, data_cnt = 0, size;
	u16 crc;
	int i;
	struct mtp_header header;
	const size_t header_size = sizeof(struct mtp_header);

	memcpy(&header, ri->raw_data, header_size);
	/* avoid update whole 256 byte data, 0: update 1 data,
					     1:	update 2 data, ...etc */
	header_data_cnt = header.data_cnt + 1;

	for (i = 4; i < ri->raw_data_cnt - 1; i += (size + 2)) {
		addr = ri->raw_data[i];
		size = ri->raw_data[i + 1] + 1;
		row_cnt++;
		data_cnt += size;
		dev_info(ri->dev, "%s: 0x%02X, 0x%02X, 0x%02X\n",
			 __func__, addr, size, i);
		if (addr + size > MTP_DATA_SIZE ||
		    (i + 1 + size) >= ri->raw_data_cnt)
			return -EINVAL;
		memcpy(ext_data + addr, ri->raw_data + (i + 2), size);
		memset(ext_mask + addr, 1, size);
	}
	if ((row_cnt != header.row_cnt) || (data_cnt != header_data_cnt))
		return -EINVAL;

	crc = crc16(0xFFFF, ri->raw_data + header_size,
			ri->raw_data_cnt - header_size);
	dev_info(ri->dev, "%s: crc = (0x%04X, 0x%04X)\n", __func__, crc,
							header.crc);
	if (crc != header.crc)
		return -EINVAL;

	return 0;
}

static int rt1653_check_mtp_ver(struct rt1653_info *ri, u8 *ext_data,
				bool *latest_mtp)
{
	int i, ret;
	u8 data[2];

	for (i = 0; i < ARRAY_SIZE(data); i++) {
		/* addr[0xFE] : mtp_data ver */
		ret = rt1653_init_mtp(ri, RT1653_MTP_READ,
				      RT1653_MTP_VEND_ID + i, 0);
		if (ret < 0) {
			dev_err(ri->dev, "%s: init fail(%d)\n", __func__, ret);
			goto out;
		}

		ret = rt1653_wait_firmware_done(ri);
		if (ret < 0) {
			dev_err(ri->dev, "%s: wait fail(%d)\n", __func__, ret);
			goto out;
		}

		/* Read data */
		ret = rt1653_i2c_read_byte(ri, 0x65);
		if (ret < 0) {
			dev_err(ri->dev, "%s: read fail(%d)\n", __func__, ret);
			goto out;
		}
		data[i] = ret;
	}
	if (data[0] != 0x00 && data[1] != 0xFF &&
			(data[1] > ext_data[RT1653_MTP_VER_ID] ||
			 (memcmp(data, ext_data + RT1653_MTP_VEND_ID,
				 ARRAY_SIZE(data)) == 0))) {
		dev_info(ri->dev,
			 "%s: no need update, ic_ver(%d,%d), fw_ver(%d, %d)\n",
			 __func__, data[0], data[1],
			 ext_data[RT1653_MTP_VEND_ID],
			 ext_data[RT1653_MTP_VER_ID]);
		*latest_mtp = true;
	}
out:
	return ret;
}

static void rt1653_apply_raw_data(struct rt1653_info *ri, u8 *orig_data,
				 u8 *ext_data, u8 *ext_mask)
{
	int i;

	for (i = 0; i < MTP_DATA_SIZE; i++) {
		if (!ext_mask[i])
			ext_data[i] = orig_data[i];
	}
}

static int __rt1653_erase_mtp(struct rt1653_info *ri, int page_idx)
{
	int ret = 0;

	ret = rt1653_init_mtp(ri, RT1653_MTP_ERASE,
				  page_idx * MTP_PAGE_SIZE, 0);
	if (ret < 0) {
		dev_err(ri->dev, "%s: init fail(%d)\n", __func__, ret);
		return ret;
	}

	ret = rt1653_wait_firmware_done(ri);
	if (ret < 0) {
		dev_err(ri->dev, "%s: wait fw done fail(%d)\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int rt1653_enable_pump_v33ddd(struct rt1653_info *ri, bool en)
{
	int ret = 0;

	/* V33DDD set to 5V/3.3V */
	ret = rt1653_i2c_write_byte(ri, 0x1F, en ? 0x80 : 0x00);
	if (ret < 0)
		dev_err(ri->dev, "%s: V33DDD pump fail(%d), en = %d\n",
			__func__, ret, en);
	return ret;
}

static int __rt1653_write_mtp_byte(struct rt1653_info *ri, int index,
				   const u8 apply)
{
	int ret = 0;

	ret = rt1653_enable_pump_v33ddd(ri, true);
	if (ret < 0)
		goto out;

	ret = rt1653_init_mtp(ri, RT1653_MTP_WRITE, index, apply);
	if (ret < 0) {
		dev_err(ri->dev, "%s: init fail(%d)\n", __func__, ret);
		goto out;
	}

	/* wait firmware done */
	ret = rt1653_wait_firmware_done(ri);
	if (ret < 0) {
		dev_err(ri->dev, "%s: wait fail(%d)\n", __func__, ret);
		goto out;
	}

	ret = rt1653_enable_pump_v33ddd(ri, false);
	if (ret < 0)
		goto out;

	/* Read and Check mtp */
	ret = rt1653_init_mtp(ri, RT1653_MTP_READ, index, 0);
	if (ret < 0) {
		dev_err(ri->dev, "%s: init fail(%d)\n", __func__, ret);
		goto out;
	}

	/* wait firmware done */
	ret = rt1653_wait_firmware_done(ri);
	if (ret < 0) {
		dev_err(ri->dev, "%s: wait fail(%d)\n", __func__, ret);
		goto out;
	}

	/* Read apply */
	ret = rt1653_i2c_read_byte(ri, 0x65);
	if (ret < 0) {
		dev_err(ri->dev, "%s: read fail(%d)\n", __func__, ret);
		goto out;
	}
	if (apply != (ret & 0xFF)) {
		dev_err(ri->dev,
		"%s: check write mtp fail, addr[0x%02X] = (0x%02X,0x%02X)\n",
		__func__, index, apply, ret & 0xFF);
		ret = -EINVAL;
		goto out;
	}
out:
	rt1653_enable_pump_v33ddd(ri, false);
	return ret;
}

static int rt1653_update_mtp_ver(struct rt1653_info *ri, const u8 *data)
{
	int i, ret = 0;

	for (i = RT1653_MTP_VEND_ID;
	     i < RT1653_MTP_VEND_ID + RT1653_MTP_VER_SIZE; i++) {
		ret = __rt1653_write_mtp_byte(ri, i, data[i]);
		if (ret < 0)
			goto out;
	}
out:
	return ret;
}

static int __rt1653_write_mtp(struct rt1653_info *ri, const u8 *orig_data,
			      const u8 *apply_data)
{
	int i, j, idx, offset, ret;

	ret = rt1653_init_ic_power(ri);
	if (ret < 0) {
		dev_err(ri->dev, "%s: init power fail(%d)\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < MTP_PAGE_SIZE; i++) {
		offset = i * MTP_PAGE_SIZE;
		/* if this page is not to be changed, skip */
		if (orig_data && memcmp(orig_data + offset, apply_data + offset,
					MTP_PAGE_SIZE) == 0)
			continue;

		ret = rt1653_enable_pump_v33ddd(ri, true);
		if (ret < 0)
			goto out;

		ret = __rt1653_erase_mtp(ri, i);
		if (ret < 0) {
			dev_err(ri->dev, "%s: erase fail(%d)\n", __func__, i);
			goto out;
		}

		/* Write mtp */
		for (j = 0; j < MTP_PAGE_SIZE; j++) {
			idx = offset + j;
			/* skip mtp ver */
			if (idx == RT1653_MTP_VEND_ID ||
			    idx == RT1653_MTP_VER_ID)
				continue;

			ret = __rt1653_write_mtp_byte(ri, idx, apply_data[idx]);
			if (ret < 0)
				goto out;
		}
	}
out:
	rt1653_enable_pump_v33ddd(ri, false);
	return ret;
}

static int rt1653_get_mtp_crc(struct rt1653_info *ri, u16 *mtp_crc)
{
	int ret = 0;
	u16 crc = 0;

	/* Start calculate and read firmware CRC */
	ret = rt1653_i2c_write_byte(ri, 0x64, 0xA2);
	if (ret < 0) {
		dev_err(ri->dev, "%s: start cal CRC fail(%d)\n", __func__, ret);
		return ret;
	}

	ret = rt1653_enable_prog_mode(ri, false);
	if (ret < 0) {
		dev_err(ri->dev, "%s: exit prog mode fail(%d)\n",
				 __func__, ret);
		return ret;
	}

	msleep(100);

	ret = rt1653_i2c_block_read(ri, 0x64, MTP_CRC_SIZE, (u8 *)&crc);
	if (ret < 0)
		dev_err(ri->dev, "%s: read crc fail(%d)\n", __func__, ret);
	*mtp_crc = cpu_to_le16(crc);

	/* Must enable prog mode at the end */
	ret = rt1653_enable_prog_mode(ri, true);
	if (ret < 0)
		dev_err(ri->dev, "%s: en prog mode fail(%d)\n", __func__, ret);
	return ret;
}

static int rt1653_check_mtp_crc(struct rt1653_info *ri, u8 *data)
{
	int ret = 0;
	u16 mtp_crc = 0, cal_crc;

	/* Get mtp fw CRC */
	ret = rt1653_get_mtp_crc(ri, &mtp_crc);
	if (ret < 0) {
		dev_err(ri->dev, "%s: get mtp crc fail(%d)\n", __func__, ret);
		return ret;
	}

	cal_crc = crc16(0xFFFF, data, MTP_DATA_SIZE);
	dev_info(ri->dev, "%s: crc = (0x%04x, 0x%04x)\n", __func__, mtp_crc,
							 cal_crc);
	if (cal_crc != mtp_crc) {
		dev_err(ri->dev, "%s: [ERROR] Incorrect CRC\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int rt1653_write_mtp(struct rt1653_info *ri, u8 *data)
{
	int ret = 0;

	ret = rt1653_enable_prog_mode(ri, true);
	if (ret < 0) {
		dev_err(ri->dev, "%s: en prog fail(%d)\n", __func__, ret);
		return ret;
	}

	ret = __rt1653_write_mtp(ri, NULL, data);
	if (ret < 0) {
		dev_err(ri->dev, "%s: write mtp fail(%d)\n", __func__, ret);
		goto out;
	}

	ret = rt1653_update_mtp_ver(ri, data);
	if (ret < 0) {
		dev_err(ri->dev, "%s: update mtp version fail(%d)\n",
			__func__, ret);
		goto out;
	}

	ret = rt1653_check_mtp_crc(ri, data);
	if (ret < 0)
		dev_err(ri->dev, "%s: check crc fail after write mtp(%d)\n",
			__func__, ret);
out:
	rt1653_enable_prog_mode(ri, false);
	return ret;
}

#if 0
static int rt1653_check_mtp_data(struct rt1653_info *ri, u8 *mtp_data)
{
	int ret = 0;
	u8 data[MTP_DATA_SIZE];

	ret = __rt1653_read_mtp(ri, data);
	if (ret < 0) {
		dev_err(ri->dev, "%s: read mtp fail(%d)\n", __func__, ret);
		return ret;
	}

	ret = memcmp(mtp_data, data, MTP_DATA_SIZE);
	if (ret != 0) {
		dev_err(ri->dev, "%s: data mismatch(%d)\n", __func__, ret);
		return -EINVAL;
	}
	return 0;
}
#endif

static ssize_t rt1653_attr_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count);
static ssize_t rt1653_attr_show(struct device *dev,
				struct device_attribute *attr, char *buf);

#define RT_DEV_ATTR(_name, _mode) { \
	.attr = { \
		.name = #_name, \
		.mode = _mode, \
	}, \
	.show = rt1653_attr_show, \
	.store = rt1653_attr_store, \
}

static const struct device_attribute rt1653_dev_attrs[] = {
	RT_DEV_ATTR(raw_data, 0644),
	RT_DEV_ATTR(mtp_result, 0644),
	RT_DEV_ATTR(trigger, 0200),
};

enum {
	RT_DESC_RAW_DATA = 0,
	RT_DESC_MTP_RESULT,
	RT_DESC_TRIGGER,
};

static ssize_t rt1653_attr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct rt1653_info *ri = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - rt1653_dev_attrs;
	u8 data[MTP_DATA_SIZE];
	ssize_t size = 0;

	mutex_lock(&ri->attr_lock);
	dev_dbg(ri->dev, "%s\n", __func__);

	switch (offset) {
	case RT_DESC_RAW_DATA:
		ret = rt1653_read_mtp(ri, data);
		if (ret < 0) {
			size = ret;
			break;
		}
		memcpy(buf, data, MTP_DATA_SIZE);
		size = MTP_DATA_SIZE;
		break;
	case RT_DESC_MTP_RESULT:
		scnprintf(buf, PAGE_SIZE, "%s\n", ri->mtp_success ?
			  "ok" : "fail");
		size = strlen(buf);
		break;
	default:
		break;
	}
	mutex_unlock(&ri->attr_lock);

	return size;
}

/* single step process enum */
enum {
	RT_TRIGGER_START = 0,
	RT_TRIGGER_MTP_UPDATE,
	RT_TRIGGER_MTP_READ,
	RT_TRIGGER_MTP_FORCE_WRITE,
	RT_TRIGGER_STOP,
};
#define RT_MAGIC_NUM (5526789)

static int rt1653_force_write_mtp(struct rt1653_info *ri)
{
	if (!ri->mtp_start)
		return -EINVAL;
	return rt1653_write_mtp(ri, ri->raw_data);
}

static int rt1653_update_mtp(struct rt1653_info *ri)
{
	int ret = 0;
	int retry = ri->mtp_retry_cnt;
	bool latest_mtp = false;
	u8 orig_data[MTP_DATA_SIZE];
	u8 ext_data[MTP_DATA_SIZE] = {0};
	u8 ext_mask[MTP_DATA_SIZE] = {0};

	ret = rt1653_extract_raw_data(ri, ext_data, ext_mask);
	if (ret < 0) {
		dev_err(ri->dev, "%s: ext raw data fail(%d)\n",
				__func__, ret);
		goto out;
	}

	while (retry--) {
		ret = rt1653_enable_prog_mode(ri, true);
		if (ret < 0) {
			dev_err(ri->dev, "%s: en prog fail(%d)\n",
					 __func__, ret);
			if (retry > 0)
				continue;
			else
				return ret;
		}

		ret = rt1653_check_mtp_ver(ri, ext_data, &latest_mtp);
		if (ret < 0) {
			dev_err(ri->dev, "%s: check version fail(%d)\n",
				__func__, ret);
			if (retry > 0)
				continue;
			else
				goto out;
		} else if (latest_mtp)
			goto out;

		ret = __rt1653_read_mtp(ri, orig_data);
		if (ret < 0) {
			dev_err(ri->dev, "%s: read mtp fail(%d)\n",
					 __func__, ret);
			if (retry > 0)
				continue;
			else
				goto out;
		}

		ret = rt1653_check_mtp_crc(ri, orig_data);
		if (ret < 0) {
			dev_err(ri->dev, "%s: check crc fail(%d)\n",
					 __func__, ret);
			if (retry > 0)
				continue;
			else
				goto out;
		} else
			dev_info(ri->dev, "%s: chip mtp is available\n",
				__func__);

		rt1653_apply_raw_data(ri, orig_data, ext_data, ext_mask);
		break;
	}

	retry = ri->mtp_retry_cnt * 2;
	while (retry--) {

		ret = __rt1653_write_mtp(ri, orig_data, ext_data);
		if (ret < 0) {
			dev_err(ri->dev, "%s: write mtp fail(%d)\n",
					 __func__, ret);
			if (retry > 0)
				continue;
			else
				goto out;
		}

		ret = rt1653_update_mtp_ver(ri, ext_data);
		if (ret < 0) {
			dev_err(ri->dev, "%s: update mtp version fail(%d)\n",
				__func__, ret);
			if (retry > 0)
				continue;
			else
				goto out;
		}

		ret = rt1653_check_mtp_crc(ri, ext_data);
		if (ret < 0) {
			dev_err(ri->dev,
				"%s: check crc fail after write mtp(%d)\n",
				__func__, ret);
			if (retry > 0)
				continue;
			else
				goto out;
		}

		dev_info(ri->dev, "%s: update bin done\n", __func__);
		goto out;
	}
out:
	rt1653_enable_prog_mode(ri, false);
	return ret;
}

static int rt1653_trigger_action(struct rt1653_info *ri, int trig_num)
{
	int ret = 0;
	u8 data[MTP_DATA_SIZE];

	switch (trig_num) {
	case RT_TRIGGER_MTP_UPDATE:
		if (!ri->raw_data)
			return -EINVAL;
		ri->mtp_success = false;
		ri->mtp_retry_cnt = 3;
		ret = rt1653_update_mtp(ri);
		if (ret < 0)
			dev_err(ri->dev, "%s, update mtp fail(%d)\n",
					 __func__, ret);
		else
			ri->mtp_success = true;
		break;
	case RT_TRIGGER_MTP_READ:
		ret = rt1653_read_mtp(ri, data);
		break;
	case RT_TRIGGER_MTP_FORCE_WRITE:
		ret = rt1653_force_write_mtp(ri);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int rt1653_trigger_process(struct rt1653_info *ri, int trig_num)
{
	int ret = 0;

	switch (trig_num) {
	case RT_TRIGGER_START:
#ifdef REGULATOR_CTRL_CHG_PUMP /* use external chg pump 5V */
		/* Enable charger pump */
		ret = regulator_enable(ri->chg_pump);
		if (ret < 0) {
			dev_err(ri->dev, "%s: enable chg_pump fail(%d)\n",
					 __func__, ret);
			return ret;
		}

		msleep(30);
#endif /* REGULATOR_CTRL_CHG_PUMP */
		ri->mtp_start = true;
		break;
	case RT_TRIGGER_MTP_UPDATE ... RT_TRIGGER_MTP_FORCE_WRITE:
		ret = rt1653_trigger_action(ri, trig_num);
		break;
	case RT_TRIGGER_STOP:
		ri->mtp_start = false;
		if (ri->raw_data) {
			devm_kfree(ri->dev, ri->raw_data);
			ri->raw_data = NULL;
		}
#ifdef REGULATOR_CTRL_CHG_PUMP /* use external chg pump 5V */
		/* Disable charger pump */
		ret = regulator_disable(ri->chg_pump);
		if (ret < 0)
			dev_err(ri->dev, "%s: disable chg_pump fail(%d)\n",
				__func__, ret);
#endif /* REGULATOR_CTRL_CHG_PUMP */
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static inline int rt1653_store_raw_data(struct rt1653_info *ri,
					const char *buf, size_t count)
{
	if (!ri->mtp_start || count < 7)
		return -EINVAL;
	if (ri->raw_data) {
		devm_kfree(ri->dev, ri->raw_data);
		ri->raw_data = NULL;
	}
	ri->raw_data = devm_kzalloc(ri->dev, count, GFP_KERNEL);
	if (!ri->raw_data)
		return -ENOMEM;
	memcpy(ri->raw_data, buf, count);
	ri->raw_data_cnt = count;
	return 0;
}

static ssize_t rt1653_attr_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	int ret = 0;
	struct rt1653_info *ri = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - rt1653_dev_attrs;
	unsigned long val;

	mutex_lock(&ri->attr_lock);
	dev_dbg(ri->dev, "%s\n", __func__);

	switch (offset) {
	case RT_DESC_RAW_DATA:
		ret = rt1653_store_raw_data(ri, buf, count);
		if (ret < 0)
			goto out;
		break;
	case RT_DESC_TRIGGER:
		ret = kstrtoul(buf, 10, &val);
		if (ret < 0)
			goto out;
		val = val - RT_MAGIC_NUM;
		if (val < RT_TRIGGER_START || val > RT_TRIGGER_STOP) {
			ret = -EINVAL;
			goto out;
		}
		ret = rt1653_trigger_process(ri, val);
		if (ret < 0)
			goto out;
		break;
	default:
		ret = -EINVAL;
		goto out;
	}
	ret = count;
out:
	mutex_unlock(&ri->attr_lock);

	return ret;
}

static void fwload_load_cb(const struct firmware *fw, void *context)
{
	struct rt1653_info *ri = (struct rt1653_info *)context;
	struct rt1653_platform_data *pdata = dev_get_platdata(ri->dev);
	const size_t count = 0;
	int val = 0, ret = 0;

	if (fw == NULL) {
		dev_err(ri->dev, "%s, request fw failed\n", __func__);
		return;
	}

	dev_info(ri->dev, "%s, fw_size = %zu\n", __func__, count);
	if (count < 7) {
		dev_err(ri->dev, "%s, raw data shorter than minimum\n",
			__func__);
		goto out;
	}
	if (ri->raw_data) {
		devm_kfree(ri->dev, ri->raw_data);
		ri->raw_data = NULL;
	}
	ri->raw_data = devm_kzalloc(ri->dev, count, GFP_KERNEL);
	if (!ri->raw_data) {
		dev_err(ri->dev, "%s, ENOMEM\n", __func__);
		goto out;
	}
	memcpy(ri->raw_data, fw->data, count);
	ri->raw_data_cnt = count;

	/* Auto update MTP data */
	val = gpio_get_value(pdata->gpios[RT1653_WLESS_GPIO].gpio);
	dev_info(ri->dev, "%s, wless gpio = %d\n", __func__, val);
	if (!val)
		goto out;

	/* Enable Charge Pump */
#ifdef REGULATOR_CTRL_CHG_PUMP /* use external chg pump 5V */
	ret = regulator_enable(ri->chg_pump);
	if (ret < 0) {
		dev_err(ri->dev, "%s: enable chg_pump fail(%d)\n",
				__func__, ret);
		goto out;
	}
	msleep(30);
#endif

	ret = rt1653_update_mtp(ri);
	if (ret < 0)
		dev_err(ri->dev, "%s, update mtp fail(%d)\n", __func__, ret);
	dev_info(ri->dev, "%s: done\n", __func__);

	/* Disable Charge Pump */
#ifdef REGULATOR_CTRL_CHG_PUMP /* use external chg pump 5V */
	ret = regulator_disable(ri->chg_pump);
	if (ret < 0) {
		dev_err(ri->dev, "%s: disable chg_pump fail(%d)\n",
				__func__, ret);
		goto out;
	}
#endif
out:
	release_firmware(fw);
}
#endif /* CONFIG_MTP_PROGRAM */

static inline void rt1653_psy_changed(struct rt1653_info *ri)
{
	int ret = 0;
	union power_supply_propval propval;

	dev_info(ri->dev, "%s: wless_attached = %d\n",
			  __func__, ri->wless_attached);

	if (!ri->psy)
		ri->psy = power_supply_get_by_name("charger");
	if (!ri->psy) {
		dev_err(ri->dev, "%s: get chg psy fail\n", __func__);
		return;
	}

	if (!ri->wless_attached) {
		ret = power_supply_get_property(ri->psy,
						POWER_SUPPLY_PROP_CHARGE_TYPE,
						&propval);
		if (ret < 0)
			dev_err(ri->dev, "%s: get psy type fail(%d)\n",
					 __func__, ret);
		if (propval.intval != WIRELESS_CHARGER) {
			dev_err(ri->dev,
				"%s: current charger type is %d, skip inform\n",
				__func__, propval.intval);
			return;
		}
	}

	propval.intval = ri->wless_attached;
	ret = power_supply_set_property(ri->psy, POWER_SUPPLY_PROP_ONLINE,
					&propval);
	if (ret < 0)
		dev_err(ri->dev, "%s: psy online fail(%d)\n", __func__, ret);

	propval.intval = (ri->wless_attached) ? WIRELESS_CHARGER :
						CHARGER_UNKNOWN;
	ret = power_supply_set_property(ri->psy, POWER_SUPPLY_PROP_CHARGE_TYPE,
					&propval);
	if (ret < 0)
		dev_err(ri->dev, "%s: psy type fail(%d)\n", __func__, ret);
}

static int rt1653_get_status(struct charger_device *chg_dev,
			     enum chg_stat status, u32 *val)
{
	int ret = 0;
	struct rt1653_info *ri = charger_get_data(chg_dev);

	if (status >= CHG_STAT_MAX) {
		dev_err(ri->dev, "%s: status(%d) >= max(%d)",
				 __func__, status, CHG_STAT_MAX);
		return -EINVAL;
	}

	switch (status) {
	case WLESS_CHG_STAT_EPP:
	case WLESS_CHG_STAT_LP_EPP:
		ret = rt1653_i2c_read_byte(ri, stat_tbl[status].reg);
		if (ret < 0)
			return ret;
		*val = (ret & stat_tbl[status].mask) >> stat_tbl[status].shift;
		dev_info(ri->dev, "%s: %s = %d, reg[0x%02X] = 0x%02X\n",
				  __func__, stat_name[status], *val,
				  stat_tbl[status].reg, ret);
		break;
	default:
		dev_err(ri->dev, "%s: no such status(%d) defined\n",
				 __func__, status);
		return -ENOTSUPP;
	}
	return 0;
}

static inline u8 rt1653_closest_reg(u32 min, u32 max, u32 step, u32 target)
{
	if (target < min)
		return 0;

	if (target > max)
		target = max;

	return (target - min) / step;
}

static inline u32 rt1653_closest_value(u32 min, u32 max, u32 step, u8 regval)
{
	u32 val = 0;

	val = min + regval * step;
	if (val > max)
		val = max;

	return val;
}

static int rt1653_enable(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct rt1653_info *ri = charger_get_data(chg_dev);
	struct rt1653_platform_data *pdata = dev_get_platdata(ri->dev);

	dev_info(ri->dev, "%s: en = %d\n", __func__, en);
	/* Control mode0 of RT1653 */
	if (en)
		ri->en = en;
	gpio_set_value(pdata->gpios[RT1653_MODE0_GPIO].gpio, !en);
	if (!en) {
		ret = wait_event_timeout(ri->wait_queue, ri->wless_gpio,
					 msecs_to_jiffies(200));
		if (ret <= 0) {
			dev_err(ri->dev,
				"%s: wait GPIO0 pull-high time out", __func__);
			ret = -EIO;
		}
		ri->en = en;
	}
	return ret;
}

static int rt1653_enable_power_path(struct charger_device *chg_dev, bool en)
{
	struct rt1653_info *ri = charger_get_data(chg_dev);
	struct rt1653_platform_data *pdata = dev_get_platdata(ri->dev);

	dev_info(ri->dev, "%s: en = %d\n", __func__, en);
	/* Control NCP3902 */
	gpio_set_value(pdata->gpios[RT1653_MODE1_GPIO].gpio, !en);
	return 0;
}

static int __rt1653_get_vldo(struct rt1653_info *ri, u32 *uV)
{
	int ret = 0;
	u8 regval = 0;

	ret = rt1653_i2c_read_byte(ri, RT1653_REG_LDO_CONTROL);
	if (ret < 0)
		return ret;

	regval = (ret & RT1653_VLDO_MASK) >> RT1653_VLDO_SHIFT;
	*uV = rt1653_closest_value(RT1653_VLDO_MIN, RT1653_VLDO_MAX,
				   RT1653_VLDO_STEP, regval);

	return ret;
}

static int rt1653_get_vldo(struct charger_device *chg_dev, u32 *uV)
{
	struct rt1653_info *ri = charger_get_data(chg_dev);

	return __rt1653_get_vldo(ri, uV);
}

static int __rt1653_get_vout_adc(struct rt1653_info *ri, u32 *val)
{
	int ret = 0, i = 0;
	u8 adc_result[2] = {0}, regval = 0;
	const int max_retry_cnt = 35;

	regval = RT1653_ADC_START_MASK |
		 ((VOUT_CHANNEL << RT1653_ADC_CHANNEL_SHIFT) &
		 RT1653_ADC_CHANNEL_MASK);

	ret = rt1653_i2c_write_byte(ri, RT1653_REG_ADC_CONTROL, regval);
	if (ret < 0)
		goto out;

	for (i = 0; i < max_retry_cnt; i++) {
		ret = rt1653_i2c_read_byte(ri, RT1653_REG_ADC_CONTROL);
		if (ret >= 0 && !(ret & RT1653_ADC_START_MASK))
			break;
		/* 10ms */
		udelay(10000);
	}
	if (i == max_retry_cnt) {
		dev_err(ri->dev, "%s: waiting ADC timeout\n", __func__);
		ret = -ETIMEDOUT;
		goto out;
	}

	ret = rt1653_i2c_block_read(ri, RT1653_REG_ADC_DATA1,
				    sizeof(adc_result), adc_result);
	if (ret < 0)
		goto out;
	/* mV --> uV */
	*val = ((adc_result[1] << 8) | adc_result[0]) * 1000;
out:
	dev_info(ri->dev, "%s: val = %u, ret = %d\n", __func__, *val, ret);
	rt1653_clr_bit(ri, RT1653_REG_ADC_CONTROL, RT1653_ADC_START_MASK);
	return ret;
}

static int rt1653_set_vldo(struct charger_device *chg_dev, u32 uV)
{
	int ret = 0, i = 0;
	struct rt1653_info *ri = charger_get_data(chg_dev);
	u8 regval = 0;
	const int max_retry_cnt = 3;
	u32 vout = 0, diff = 0;

	regval = rt1653_closest_reg(RT1653_VLDO_MIN, RT1653_VLDO_MAX,
				    RT1653_VLDO_STEP, uV);

	dev_info(ri->dev, "%s: target = %d(0x%02X)\n", __func__, uV, regval);

	ret = (uV ? rt1653_set_bit : rt1653_clr_bit)
		(ri, RT1653_REG_LDO_ILIM_EN, RT1653_ENI2CLDO_MASK);
	if (ret < 0 || !uV)
		goto out;

	ret = rt1653_i2c_update_bits(ri, RT1653_REG_LDO_CONTROL,
				     RT1653_VLDO_MASK,
				     regval << RT1653_VLDO_SHIFT);
	if (ret < 0)
		goto out;

	for (i = 0; i < max_retry_cnt; i++) {
		ret = __rt1653_get_vout_adc(ri, &vout);
		if (ret < 0)
			continue;

		dev_info(ri->dev, "%s: target = %u, adc = %u\n",
				  __func__, uV, vout);
		diff = (uV > vout) ? uV - vout : vout - uV;
		/* 1V */
		if (diff <= 1000000)
			break;
	}
	if (i == max_retry_cnt) {
		dev_err(ri->dev, "%s: waiting VOUT's change timeout\n",
				 __func__);
		ret = -ETIMEDOUT;
	}
out:
	return ret;
}

static int rt1653_get_vbus_adc(struct charger_device *chg_dev, u32 *vbus)
{
	struct rt1653_info *ri = charger_get_data(chg_dev);

	return __rt1653_get_vout_adc(ri, vbus);
}

static int rt1653_dump_registers(struct charger_device *chg_dev)
{
	struct rt1653_info *ri = charger_get_data(chg_dev);
	u32 vldo = 0, vout = 0;

	__rt1653_get_vldo(ri, &vldo);
	__rt1653_get_vout_adc(ri, &vout);

	dev_info(ri->dev, "%s VLDO = %dmV, VOUT = %dmV\n",
		 __func__, vldo / 1000, vout / 1000);

	return 0;
}

static struct charger_ops rt1653_chg_ops = {
	.enable = rt1653_enable,
	.enable_powerpath = rt1653_enable_power_path,
	.get_constant_voltage = rt1653_get_vldo,
	.set_constant_voltage = rt1653_set_vldo,
	.get_vbus_adc = rt1653_get_vbus_adc,
	.get_status = rt1653_get_status,
	.dump_registers = rt1653_dump_registers,
};

static int rt1653_enable_hidden_mode(struct rt1653_info *ri)
{
	int ret = 0;

	dev_info(ri->dev, "%s\n", __func__);

	ret = rt1653_i2c_block_write(ri, 0xF0,
				     ARRAY_SIZE(rt1653_val_en_hidden_mode),
				     rt1653_val_en_hidden_mode);
	if (ret < 0)
		dev_err(ri->dev, "%s fail(%d)\n", __func__, ret);

	return ret;
}

static inline struct rt1653_info *work_to_rt1653(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);

	return container_of(dwork, struct rt1653_info, dwork);
}

static void rt1653_irq_delayed_work(struct work_struct *work)
{
	struct rt1653_info *ri = work_to_rt1653(work);

	dev_info(ri->dev, "%s: wless_gpio = %d, en = %d\n",
			  __func__, ri->wless_gpio, ri->en);

	if (ri->wless_gpio && ri->wless_attached) {
		dev_info(ri->dev, "%s: detached\n", __func__);
		ri->wless_attached = false;
		rt1653_psy_changed(ri);
	} else if (!ri->wless_gpio && !ri->wless_attached && ri->en) {
		dev_info(ri->dev, "%s: attached\n", __func__);
		ri->wless_attached = true;
		rt1653_psy_changed(ri);
	}
}

static void __rt1653_irq_handler(struct rt1653_info *ri, unsigned int delay)
{
	struct rt1653_platform_data *pdata = dev_get_platdata(ri->dev);

	ri->wless_gpio = gpio_get_value(pdata->gpios[RT1653_WLESS_GPIO].gpio);
	dev_info(ri->dev, "%s: wless_gpio = %d, en = %d\n",
			  __func__, ri->wless_gpio, ri->en);
	if (!ri->en)
		return;
	delay = ri->wless_gpio ? 1000 : delay;
	mod_delayed_work(ri->wqs, &ri->dwork, msecs_to_jiffies(delay));
	pm_wakeup_event(ri->dev, delay);
	if (ri->wless_gpio)
		wake_up(&ri->wait_queue);
	else {
		rt1653_enable_hidden_mode(ri);
		rt1653_set_bit(ri, RT1653_REG_SYSTEM_STATUS,
				   RT1653_AP_READY_MASK);
	}
}

static irqreturn_t rt1653_irq_handler(int irq, void *priv)
{
	__rt1653_irq_handler(priv, 100);
	return IRQ_HANDLED;
}

static int rt1653_init_gpio(struct rt1653_info *ri)
{
	int ret = 0;
	struct rt1653_platform_data *pdata = dev_get_platdata(ri->dev);

	ret = gpio_request_array(pdata->gpios, ARRAY_SIZE(pdata->gpios));
	if (ret < 0)
		dev_err(ri->dev, "%s: request gpio array fail(%d)\n",
				 __func__, ret);
	return ret;
}

static int rt1653_irq_register(struct rt1653_info *ri)
{
	int ret = 0;
	struct rt1653_platform_data *pdata = dev_get_platdata(ri->dev);

	ret = gpio_to_irq(pdata->gpios[RT1653_WLESS_GPIO].gpio);
	if (ret <= 0) {
		dev_err(ri->dev, "%s: gpio_to_irq fail(%d)\n", __func__, ret);
		return ret;
	}
	ri->irq = ret;

	ret = devm_request_threaded_irq(ri->dev, ri->irq, NULL,
					rt1653_irq_handler,
					IRQF_TRIGGER_FALLING |
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"rt1653_irq", ri);
	if (ret < 0)
		return ret;
	device_init_wakeup(ri->dev, true);
	return 0;
}

static void rt1653_irq_unregister(struct rt1653_info *ri)
{
	device_init_wakeup(ri->dev, false);
	devm_free_irq(ri->dev, ri->irq, ri);
}

static int rt1653_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	int ret = 0;
	struct rt1653_info *ri = NULL;
	struct rt1653_platform_data *pdata = dev_get_platdata(&i2c->dev);
	bool use_dt = i2c->dev.of_node;
#ifdef CONFIG_MTP_PROGRAM
	int i = 0;
#endif

	dev_info(&i2c->dev, "%s: (%s)\n", __func__, RT1653_DRV_VERSION);

	if (use_dt) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = rt1653_parse_dt(&i2c->dev, pdata);
		if (ret < 0) {
			dev_err(&i2c->dev, "%s: parse dts fail(%d)\n",
					   __func__, ret);
			devm_kfree(&i2c->dev, pdata);
			return ret;
		}
		i2c->dev.platform_data = pdata;
	} else {
		if (!pdata)
			return -EINVAL;
	}
	ri = devm_kzalloc(&i2c->dev, sizeof(*ri), GFP_KERNEL);
	if (!ri)
		goto out_alloc_ri;
	ri->dev = &i2c->dev;
	ri->i2c = i2c;
	i2c_set_clientdata(i2c, ri);
	ri->wless_gpio = 1;
	INIT_DELAYED_WORK(&ri->dwork, rt1653_irq_delayed_work);
	ri->wqs = create_workqueue("rt1653");
	if (!ri->wqs)
		goto out_create_wq;
	mutex_init(&ri->attr_lock);
	mutex_init(&ri->io_lock);
	ri->wless_attached = false;
	ri->en = true;
	/* Init wait queue head */
	init_waitqueue_head(&ri->wait_queue);

#ifdef CONFIG_MTP_PROGRAM
#ifdef REGULATOR_CTRL_CHG_PUMP /* use external chg pump 5V */
	ri->chg_pump = regulator_get(&i2c->dev, "wless_chgpump");
	if (IS_ERR(ri->chg_pump)) {
		ret = PTR_ERR(ri->chg_pump);
		dev_err(ri->dev, "%s: regulator get fail(%d)\n", __func__, ret);
		goto out_regulator;
	}
#endif /* REGULATOR_CTRL_CHG_PUMP */

	ri->mtp_retry_cnt = 3;
	ret = request_firmware_nowait(THIS_MODULE, false, "mtp_update.bin",
				      ri->dev, GFP_KERNEL, ri, fwload_load_cb);
	if (ret < 0)
		dev_err(ri->dev, "%s: request firmware fail(%d)\n",
				 __func__, ret);
#endif /* CONFIG_MTP_PROGRAM */

	ret = rt1653_init_gpio(ri);
	if (ret < 0)
		goto out_gpio;
	/* Check wireless plug-in */
	__rt1653_irq_handler(ri, 5000);
	ret = rt1653_irq_register(ri);
	if (ret < 0)
		goto out_irq;

#ifdef CONFIG_MTP_PROGRAM
	for (i = 0; i < ARRAY_SIZE(rt1653_dev_attrs); i++) {
		ret = device_create_file(ri->dev, rt1653_dev_attrs + i);
		if (ret < 0) {
			dev_err(ri->dev,
				"%s: create file fail(%d) %d\n",
				__func__, ret, i);
			goto out_create_file;
		}
	}
#endif /* CONFIG_MTP_PROGRAM */

	ri->chg_props.alias_name = "rt1653_chg";
	ri->chg_dev = charger_device_register("wless_chg", ri->dev, ri,
					      &rt1653_chg_ops,
					      &ri->chg_props);
	if (IS_ERR_OR_NULL(ri->chg_dev)) {
		ret = PTR_ERR(ri->chg_dev);
		goto err_register_chg_dev;
	}

#ifdef CONFIG_RT_REGMAP
	ri->rd = rt_regmap_device_register(&rt1653_regmap_props,
			&rt1653_regmap_fops, ri->dev, ri->i2c, ri);
	if (!ri->rd) {
		ret = -EINVAL;
		goto err_register_rt_regmap;
	}
#endif

	dev_info(ri->dev, "%s: Successfully\n", __func__);
	return 0;
#ifdef CONFIG_RT_REGMAP
err_register_rt_regmap:
#endif
	charger_device_unregister(ri->chg_dev);
err_register_chg_dev:
#ifdef CONFIG_MTP_PROGRAM
out_create_file:
	while (--i >= 0)
		device_remove_file(ri->dev, rt1653_dev_attrs + i);
#endif /* CONFIG_MTP_PROGRAM */
	rt1653_irq_unregister(ri);
out_irq:
	gpio_free_array(pdata->gpios, ARRAY_SIZE(pdata->gpios));
out_gpio:
#ifdef CONFIG_MTP_PROGRAM
#ifdef REGULATOR_CTRL_CHG_PUMP /* use external chg pump 5V */
	regulator_put(ri->chg_pump);
out_regulator:
#endif /* REGULATOR_CTRL_CHG_PUMP */
#endif /* CONFIG_MTP_PROGRAM */
	destroy_workqueue(ri->wqs);
	mutex_destroy(&ri->io_lock);
	mutex_destroy(&ri->attr_lock);
out_create_wq:
	devm_kfree(&i2c->dev, ri);
out_alloc_ri:
	if (use_dt)
		devm_kfree(&i2c->dev, pdata);
	return ret;
}

static int rt1653_i2c_remove(struct i2c_client *i2c)
{
	struct rt1653_info *ri = i2c_get_clientdata(i2c);
	struct rt1653_platform_data *pdata = dev_get_platdata(&i2c->dev);
#ifdef CONFIG_MTP_PROGRAM
	int i = 0;
#endif

	if (ri) {
		disable_irq(ri->irq);
		charger_device_unregister(ri->chg_dev);
#ifdef CONFIG_MTP_PROGRAM
		for (i = 0; i < ARRAY_SIZE(rt1653_dev_attrs); i++)
			device_remove_file(ri->dev, rt1653_dev_attrs + i);
#endif
		rt1653_irq_unregister(ri);
#ifdef CONFIG_MTP_PROGRAM
#ifdef REGULATOR_CTRL_CHG_PUMP /* use external chg pump 5V */
		regulator_put(ri->chg_pump);
#endif /* REGULATOR_CTRL_CHG_PUMP */
#endif /* CONFIG_MTP_PROGRAM */
		destroy_workqueue(ri->wqs);
		mutex_destroy(&ri->io_lock);
		mutex_destroy(&ri->attr_lock);
	}
	if (pdata) {
		gpio_free_array(pdata->gpios, ARRAY_SIZE(pdata->gpios));
	}

	dev_info(&i2c->dev, "%s: Successfully\n", __func__);
	return 0;
}

static int __maybe_unused rt1653_i2c_pm_suspend(struct device *dev)
{
	struct rt1653_info *ri = dev_get_drvdata(dev);

	dev_dbg(dev, "%s: ++\n", __func__);
	if (device_may_wakeup(dev))
		enable_irq_wake(ri->irq);
	disable_irq(ri->irq);
	dev_dbg(dev, "%s: --\n", __func__);
	return 0;
}

static int __maybe_unused rt1653_i2c_pm_resume(struct device *dev)
{
	struct rt1653_info *ri = dev_get_drvdata(dev);

	dev_dbg(dev, "%s: ++\n", __func__);
	enable_irq(ri->irq);
	if (device_may_wakeup(dev))
		disable_irq_wake(ri->irq);
	dev_dbg(dev, "%s: --\n", __func__);
	return 0;
}

static SIMPLE_DEV_PM_OPS(rt1653_i2c_pm_ops,
			 rt1653_i2c_pm_suspend, rt1653_i2c_pm_resume);

static const struct of_device_id __maybe_unused rt1653_ofid_table[] = {
	{ .compatible = "richtek,rt1653",},
	{ },
};
MODULE_DEVICE_TABLE(of, rt1653_ofid_table);

static const struct i2c_device_id rt1653_id_table[] = {
	{ "rt1653", 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, rt1653_id_table);

static struct i2c_driver rt1653_i2c_drv = {
	.driver = {
		.name = "rt1653",
		.owner = THIS_MODULE,
		.of_match_table = rt1653_ofid_table,
		.pm = &rt1653_i2c_pm_ops,
	},
	.probe = rt1653_i2c_probe,
	.remove = rt1653_i2c_remove,
	.id_table = rt1653_id_table,
};
module_i2c_driver(rt1653_i2c_drv);

MODULE_LICENSE("GPL v2");
MODULE_VERSION(RT1653_DRV_VERSION);
MODULE_AUTHOR("gene_chen shufan_lee <@richtek.com>");
MODULE_DESCRIPTION("RT1653 Wireless Rx");

/*
 * LGE Changes
 * - CONFIG_MTK_WIRELESS_CHARGER_SUPPORT deleted
 */

/*
 * Release Note
 * 1.0.8
 * (1) Use ADC to wait VOUT's change for rt1653_set_vldo()
 * (2) Add charger_ops: get_constant_voltage, get_vbus_adc, and dump_registers
 *
 * 1.0.7
 * (1) Use non-interruptible wait_event_timeout()
 * (2) Defer getting chg psy to rt1653_psy_changed()
 *
 * 1.0.6
 * (1) Use wait queue to wait GPIO0 pull-high in rt1653_enable(), timeout: 200ms
 * (2) disable_irq()/enable_irq() in suspend()/resume()
 *
 * 1.0.5
 * (1) Fix rt1653_set_vldo()
 * (2) Add rt-regmap support for debugging
 * (3) Use wait queue to wait GPIO0 pull-high in rt1653_enable(), timeout: 500ms
 *
 * 1.0.4
 * (1) Disable MTP programming by default
 * (2) Use wait queue to wait GPIO0 pull-high in rt1653_enable()
 * (3) Change GPIO0 debounce time, high: 1s, low: 100ms
 * (4) Change GPIO0 debounce time in rt1653_i2c_probe(), high: 1s, low: 5s
 * (5) Remove unused GPIOs, hv_dwork, and code
 * (6) Enable/Disable irq_wake at pm_suspend() and pm_resume()
 * (7) Revise conditional compilation flags
 * (8) Do not inform CHARGER_UNKNOWN if
 *     current charger type is not WIRELESS_CHARGER
 * (9) Revise debugging messages
 * (10) Add io_lock for I2C operations
 * (11) Add rt1653_set_vldo()
 *
 * 1.0.3
 * (1) Register charger class for exporting ops
 * (2) Free resources in rt1653_i2c_remove()
 *
 * 1.0.2
 * (1) Add i2c NACK retry 3 times and whole retry strategy
 * (2) Check whether wireless TX is connected before MTP auto update
 * (3) Replace checking MTP version by checking MTP CRC
 *
 * 1.0.1
 * (1) Fix rt1653_wait_firmware_done() to return correct error codes
 * (2) Change RT1653_WLESS_GPIO debounce time, high: 1s, low: 0s (immediately)
 *
 * 1.0.0
 * Initial Release
 */
