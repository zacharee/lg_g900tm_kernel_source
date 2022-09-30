/*
 * IDT P9222 Wireless Power Receiver driver
 *
 * Copyright (C) 2016 LG Electronics, Inc
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
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
#include <linux/debugfs.h>

#include "charger_class.h"
#include <mt-plat/mtk_charger.h>
#include <mtk_charger_intf.h>

// Register addresses
#define P9222_REG_CHIP_ID		(0x0000)	/* word */
#define P9222_REG_CHIP_REV		(0x0002)
#define P9222_REG_CUSTOMER_ID		(0x0003)
#define P9222_REG_FW_MAJOR_REV		(0x0004)	/* word */
#define P9222_REG_FW_MINOR_REV		(0x0006)	/* word */
#define P9222_REG_PRMC_ID		(0x001E)	/* word */
#define P9222_REG_STATUS		(0x0034)	/* word */
#define P9222_REG_INT			(0x0036)	/* word */
#define P9222_REG_INT_ENABLE		(0x0038)	/* word */
#define P9222_REG_INT_CLEAR		(0x003A)	/* word */
#define P9222_REG_SYS_OP_MODE		(0x003F)
#define P9222_REG_CHG_STATUS		(0x004E)
#define P9222_REG_EPT			(0x004F)
#define P9222_REG_VOUT			(0x0050)	/* word */
#define P9222_REG_VOUT_SET		(0x0052)
#define P9222_REG_VRECT_ADJ		(0x0053)
#define P9222_REG_VRECT			(0x0054)	/* word */
#define P9222_REG_RX_IOUT		(0x0058)	/* word */
#define P9222_REG_ADC_DIE_TEMP		(0x005A)	/* word */
#define P9222_REG_OP_FREQ		(0x005C)	/* word */
#define P9222_REG_PING_FREQ		(0x005E)	/* word */
#define P9222_REG_ILIM_SET		(0x0060)
#define P9222_REG_COM			(0x0062)	/* word */
#define P9222_REG_ADT_TIMEOUT_PKT	(0x0070)
#define P9222_REG_ADT_TIMEOUT_STR	(0x0071)
#define P9222_REG_OVP_CLAMP		(0x007E)
#define P9222_REG_CMFET			(0x007F)
#define P9222_REG_FOD_COEF_GAIN_0	(0x0084)
#define P9222_REG_FOD_COEF_OFFSET_0	(0x0085)
#define P9222_REG_FOD_COEF_GAIN_1	(0x0086)
#define P9222_REG_FOD_COEF_OFFSET_1	(0x0087)
#define P9222_REG_FOD_COEF_GAIN_2	(0x0088)
#define P9222_REG_FOD_COEF_OFFSET_2	(0x0089)
#define P9222_REG_FOD_COEF_GAIN_3	(0x008A)
#define P9222_REG_FOD_COEF_OFFSET_3	(0x008B)
#define P9222_REG_FOD_COEF_GAIN_4	(0x008C)
#define P9222_REG_FOD_COEF_OFFSET_4	(0x008D)
#define P9222_REG_FOD_COEF_GAIN_5	(0x008E)
#define P9222_REG_FOD_COEF_OFFSET_5	(0x008F)
#define P9222_REG_FOD_COEF_GAIN_6	(0x0090)
#define P9222_REG_FOD_COEF_OFFSET_6	(0x0091)
#define P9222_REG_FOD_COEF_GAIN_7	(0x0092)
#define P9222_REG_FOD_COEF_OFFSET_7	(0x0093)
#define P9222_REG_ADT_ERROR_CODE	(0x00A1)
#define P9222_REG_TX_GUARANTEED_PWR	(0x00B4)
#define P9222_REG_TX_POTENTIAL_PWR	(0x00B5)
#define P9222_REG_MP_REQ_NP		(0x00BD)
#define P9222_REG_MP_REQ_MP		(0x00BE)
#define P9222_REG_FW_DATE_CODE		(0x00C4)	/* block (size : 13) */
#define P9222_REG_QF			(0x00D2)
#define P9222_REG_FW_TIME_CODE		(0x00D4)	/* block (size : 8) */
#define P9222_REG_MP_VR_CALIB_M1	(0x00E4)	/* word */
#define P9222_REG_SPEC_REV		(0x0105)
#define P9222_REG_TX_MANU_CODE		(0x0106)

/* P9222_REG_COM */
#define P9222_COM_SEND_ADT		BIT(9)
#define P9222_COM_CLEAR_INT		BIT(5)
#define P9222_COM_SEND_CHG_STATUS	BIT(4)
#define P9222_COM_SEND_EOP		BIT(3)
#define P9222_COM_MCU_RESET		BIT(2)
#define P9222_COM_LDO_TOGGLE		BIT(1)
#define P9222_COM_SEND_DATA		BIT(0)

/* P9222_REG_FOD_COEF_XXX_n */
#define P9222_FOD_COEF_START		(P9222_REG_FOD_COEF_GAIN_0)
#define P9222_FOD_COEF_SIZE		(16)

#define P9222_POWER_DEFAULT		(5000000)
#define P9222_VOUT_DEFAULT		(5500000)
#define P9222_VRECT_DEFAULT		(6000000)

#define P9222_POWER_INVALID_EPP		(10000000)

/* TX Manufacturers */
#define TX_MANUFACTURER_LG_INNOTEK	(0x63)

enum p9222_rx_mode {
	RX_MODE_AC_MISSING		= 0,
	RX_MODE_WPC_BPP			= 1,
	RX_MODE_WPC_EPP			= 2,
	RX_MODE_UNKNOWN			= 7,
};

enum p9222_irqstat {
	IRQSTAT_OVER_TEMPERATURE	= 2,	/* Byte 0 */
	IRQSTAT_OVER_CURRENT		= 3,
	IRQSTAT_OVER_VOLTAGE		= 4,	/* Byte 1 */
	IRQSTAT_OPERATION_MODE		= 5,
	IRQSTAT_STAT_VRECT		= 6,
	IRQSTAT_STAT_VOUT		= 7,
	IRQSTAT_ADT_SENT		= 8,	/* Byte 2 */
	IRQSTAT_ADT_RECEIVED		= 9,
	IRQSTAT_AC_MISSING_DETECT	= 10,
	IRQSTAT_EXTENDED_MODE		= 12,	/* Byte 3 */
	IRQSTAT_IDT_DEBUG		= 14,
	IRQSTAT_DATA_RECEIVED		= 15,
};

enum p9222_ept {
	EPT_UNKNOWN			= 0,
	EPT_EOC				= 1,
	EPT_INTERNAL_FAULT		= 2,
	EPT_OVER_TEMPERATURE		= 3,
	EPT_OVER_VOLTAGE		= 4,
	EPT_OVER_CURRENT		= 5,
	EPT_BATTERY_FAILURE		= 6,
	EPT_RECONFIGURATION		= 7,
	EPT_NO_RESPONSE			= 8,
	EPT_NEGOTIATION_FAILURE		= 10,
	EPT_RESTART_POWER_TRANSFER	= 11,
};

enum p9222_gpio {
	P9222_GPIO_VRECT,
	P9222_GPIO_PDETB,
	P9222_GPIO_IRQ,
	P9222_GPIO_EN,
	P9222_GPIO_MAX,
};

struct p9222_tx_info {
	u8 manufacturer;
	u8 spec_rev;
	int guaranteed_power;
	int potential_power;
};

struct p9222_param {
	u32 power;
	u32 vout;
	u32 vrect;
	const u8 *fod;
};

struct p9222 {
	struct device *dev;
	struct i2c_client *client;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct power_supply *chg_psy;

	struct mutex io_lock;
	struct mutex ept_lock;
	struct mutex cs_lock;
	struct mutex vout_lock;

	u32 fw_rev;
	bool enabled;
	bool online;
	enum p9222_rx_mode rx_mode;
	struct p9222_param *req_param;
	bool mode2;
	bool overheat;
	bool ept_sent;

	struct mutex tx_detect_lock;
	struct work_struct online_work;
	struct wakeup_source online_ws;
	struct delayed_work offline_work;
	struct wakeup_source offline_ws;
	unsigned int offline_timeout_ms;

	atomic_t vrect_cnt;
	struct completion vrect_fall;

	struct work_struct tx_info_work;
	struct p9222_tx_info tx;

	struct work_struct chg_status_work;
	struct notifier_block nb;
	int capacity;

	struct work_struct post_boot_work;
	bool boot_tx_epp;
	unsigned int boot_tx_epp_rst_soc;

	struct gpio gpios[P9222_GPIO_MAX];

	struct p9222_param bpp_param;
	struct p9222_param *epp_param;
	int num_epp_param;

	u32 vout_set_step;

	struct dentry *debugfs;
	u32 debug_addr;
};

static const char *p9222_dt_gpio_name[] = {
	[P9222_GPIO_VRECT] = "vrect-gpio",
	[P9222_GPIO_PDETB] = "pdetb-gpio",
	[P9222_GPIO_IRQ] = "irq-gpio",
	[P9222_GPIO_EN] = "en-gpio",
};

static const struct gpio p9222_gpio_default[P9222_GPIO_MAX] = {
	[P9222_GPIO_VRECT] = {
		.flags = GPIOF_IN,
		.label = "p9222_vrect"
	},
	[P9222_GPIO_PDETB] = {
		.flags = GPIOF_IN,
		.label = "p9222_pdetb"
	},
	[P9222_GPIO_IRQ] = {
		.flags = GPIOF_IN,
		.label = "p9222_irq"
	},
	[P9222_GPIO_EN] = {
		.flags = GPIOF_OUT_INIT_LOW,
		.label = "p9222_en"
	},
};

static const struct p9222_tx_info tx_info_default = {
	.manufacturer = 0,
	.spec_rev = 0,
	.guaranteed_power = -1,
	.potential_power = -1,
};

static const struct p9222_tx_info tx_info_bpp = {
	.manufacturer = 0,
	.spec_rev = 0,
	.guaranteed_power = P9222_POWER_DEFAULT,
	.potential_power = P9222_POWER_DEFAULT,
};

#define I2C_RETRY_COUNT 5
#define I2C_RETRY_DELAY 10
static inline int p9222_read(struct p9222 *chip, u16 reg, u8 *val)
{
	u8 address[] = {
		reg >> 8,
		reg & 0xff
	};
	struct i2c_msg msgs[] = {
		{
			.addr   = chip->client->addr,
			.flags  = 0,
			.buf    = address,
			.len    = 2,
		},
		{
			.addr   = chip->client->addr,
			.flags  = I2C_M_RD,
			.buf    = val,
			.len    = 1,
		}
	};
	int retry, ret = 0;

	mutex_lock(&chip->io_lock);

	for (retry = 0; retry <= I2C_RETRY_COUNT; retry++) {
		if (retry)
			mdelay(I2C_RETRY_DELAY);

		ret = i2c_transfer(chip->client->adapter, msgs, 2);
		if (ret == 2) {
			mutex_unlock(&chip->io_lock);

			return 0;
		}
	}

	mutex_unlock(&chip->io_lock);

	dev_err(chip->dev, "failed to read 0x%04x\n", reg);

	return ret < 0 ? ret : -EIO;
}

static inline int p9222_write(struct p9222 *chip, u16 reg, u8 val)
{
	u8 buf[] = {
		reg >> 8,
		reg & 0xff,
		val
	};
	struct i2c_msg msgs[] = {
		{
			.addr   = chip->client->addr,
			.flags  = 0,
			.buf    = buf,
			.len    = 3,
		},
	};
	int retry, ret = 0;

	mutex_lock(&chip->io_lock);

	for (retry = 0; retry <= I2C_RETRY_COUNT; retry++) {
		if (retry)
			mdelay(I2C_RETRY_DELAY);

		ret = i2c_transfer(chip->client->adapter, msgs, 1);
		if (ret == 1) {
			mutex_unlock(&chip->io_lock);

			return 0;
		}
	}

	mutex_unlock(&chip->io_lock);

	dev_err(chip->dev, "failed to write 0x%02x to 0x%04x\n", val, reg);

	return ret < 0 ? ret : -EIO;
}

static inline int p9222_read_word(struct p9222 *chip, u16 reg, u16 *val)
{
	u8 address[] = {
		reg >> 8,
		reg & 0xff
	};
	u8 buf[2] = { 0, 0 };
	struct i2c_msg msgs[] = {
		{
			.addr   = chip->client->addr,
			.flags  = 0,
			.buf    = address,
			.len    = 2
		},
		{
			.addr   = chip->client->addr,
			.flags  = I2C_M_RD,
			.buf    = buf,
			.len    = 2,
		}
	};
	int retry, ret = 0;

	mutex_lock(&chip->io_lock);

	for (retry = 0; retry <= I2C_RETRY_COUNT; retry++) {
		if (retry)
			mdelay(I2C_RETRY_DELAY);

		ret = i2c_transfer(chip->client->adapter, msgs, 2);
		if (ret == 2) {
			mutex_unlock(&chip->io_lock);

			*val = buf[0] | (buf[1] << 8);

			return 0;
		}
	}

	mutex_unlock(&chip->io_lock);

	dev_err(chip->dev, "failed to read 0x%04x\n", reg);

	return ret < 0 ? ret : -EIO;
}

static inline int p9222_write_word(struct p9222 *chip, u16 reg, u16 val)
{
	u8 buf[] = {
		reg >> 8,
		reg & 0xFF,
		val & 0xFF,
		val >> 8
	};
	struct i2c_msg msgs[] = {
		{
			.addr   = chip->client->addr,
			.flags  = 0,
			.buf    = buf,
			.len    = 4,
		},
	};
	int retry, ret = 0;

	mutex_lock(&chip->io_lock);

	for (retry = 0; retry <= I2C_RETRY_COUNT; retry++) {
		if (retry)
			mdelay(I2C_RETRY_DELAY);

		ret = i2c_transfer(chip->client->adapter, msgs, 1);
		if (ret == 1) {
			mutex_unlock(&chip->io_lock);

			return 0;
		}
	}

	mutex_unlock(&chip->io_lock);

	dev_err(chip->dev, "failed to write 0x%04x to 0x%04x\n", val, reg);

	return ret < 0 ? ret : -EIO;
}

static inline int p9222_read_block(struct p9222 *chip, u16 reg, u8 *buf,
				   unsigned int len)
{
	u8 address[] = {
		reg >> 8,
		reg & 0xff
	};
	struct i2c_msg msgs[] = {
		{
			.addr   = chip->client->addr,
			.flags  = 0,
			.buf    = address,
			.len    = 2,
		},
		{
			.addr   = chip->client->addr,
			.flags  = I2C_M_RD,
			.buf    = buf,
			.len    = len,
		}
	};
	int retry, ret = 0;

	mutex_lock(&chip->io_lock);

	for (retry = 0; retry <= I2C_RETRY_COUNT; retry++) {
		if (retry)
			mdelay(I2C_RETRY_DELAY);

		ret = i2c_transfer(chip->client->adapter, msgs, 2);
		if (ret == 2) {
			mutex_unlock(&chip->io_lock);

			return 0;
		}
	}

	mutex_unlock(&chip->io_lock);

	dev_err(chip->dev, "failed to write 0x%04x~0x%04x\n", reg, reg + len);

	return ret < 0 ? ret : -EIO;
}

static inline int p9222_write_block(struct p9222 *chip, u16 reg,
				    const u8 *val, unsigned int len)
{
	u8 buf[len + 2];
	struct i2c_msg msgs[] = {
		{
			.addr   = chip->client->addr,
			.flags  = 0,
			.buf    = buf,
			.len    = len + 2,
		},
	};
	int retry, ret = 0;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xFF;
	memcpy(buf + 2, val, len);

	mutex_lock(&chip->io_lock);

	for (retry = 0; retry <= I2C_RETRY_COUNT; retry++) {
		if (retry)
			mdelay(I2C_RETRY_DELAY);

		ret = i2c_transfer(chip->client->adapter, msgs, 1);
		if (ret == 1) {
			mutex_unlock(&chip->io_lock);

			return 0;
		}
	}

	mutex_unlock(&chip->io_lock);

	dev_err(chip->dev, "failed to write 0x%04x~0x%04x\n", reg, reg + len);

	return ret < 0 ? ret : -EIO;
}

static bool p9222_is_pad_detected(struct p9222 *chip)
{
	unsigned int gpio = chip->gpios[P9222_GPIO_PDETB].gpio;

	return gpio_get_value(gpio) ? false : true;
}

static bool p9222_is_vrect_valid(struct p9222 *chip)
{
	unsigned int gpio = chip->gpios[P9222_GPIO_VRECT].gpio;

	return gpio_get_value(gpio) ? false : true;
}

static bool p9222_is_vout_valid(struct p9222 *chip)
{
	u16 status;
	int ret;

	if (!p9222_is_pad_detected(chip))
		return false;
	if (!p9222_is_vrect_valid(chip))
		return false;

	ret = p9222_read_word(chip, P9222_REG_STATUS, &status);
	if (ret)
		return false;

	if (status & BIT(IRQSTAT_STAT_VOUT))
		return true;

	return false;
}

static int p9222_check_firmware(struct p9222 *chip)
{
	u16 major, minor;
	u8 date[14] = { 0 };
	u8 time[9] = { 0 };
	int ret;

	ret = p9222_read_word(chip, P9222_REG_FW_MAJOR_REV, &major);
	if (ret)
		return ret;

	ret = p9222_read_word(chip, P9222_REG_FW_MINOR_REV, &minor);
	if (ret)
		return ret;

	ret = p9222_read_block(chip, P9222_REG_FW_DATE_CODE, date,
			ARRAY_SIZE(date) - 1);
	if (ret)
		return ret;

	ret = p9222_read_block(chip, P9222_REG_FW_TIME_CODE, time,
			ARRAY_SIZE(time) - 1);
	if (ret)
		return ret;

	chip->fw_rev = major << 16 | minor;
	dev_info(chip->dev, "firmware version: 0x%x\n", chip->fw_rev);
	dev_info(chip->dev, "firmware date time: %s %s\n", date, time);

	return 0;
}

static void p9222_dump_rx_mode(struct p9222 *chip, enum p9222_rx_mode rx_mode)
{
	const char *rx_mode_str = NULL;

	switch (rx_mode) {
	case RX_MODE_AC_MISSING:
		rx_mode_str = "ac_missing";
		break;
	case RX_MODE_WPC_BPP:
		rx_mode_str = "wpc_bpp";
		break;
	case RX_MODE_WPC_EPP:
		rx_mode_str = "wpc_epp";
		break;
	case RX_MODE_UNKNOWN:
		rx_mode_str = "unknown";
		break;
	default:
		rx_mode_str = "ERROR";
		break;
	}

	dev_info(chip->dev, "rx mode: %s\n", rx_mode_str);
}

static enum p9222_rx_mode p9222_get_rx_mode(struct p9222 *chip)
{
	u8 value = 0;
	int ret;

	ret = p9222_read(chip, P9222_REG_SYS_OP_MODE, &value);
	if (ret)
		return RX_MODE_UNKNOWN;

	return (value & 0xE0) >> 5;
}

static int p9222_get_adc_vout(struct p9222 *chip, u32 *uV)
{
	u16 value;
	int ret;

	ret = p9222_read_word(chip, P9222_REG_VOUT, &value);
	if (ret) {
		*uV = 0;
		return ret;
	}

	*uV = value * 1000;

	return 0;
}

static int p9222_get_adc_vrect(struct p9222 *chip, u32 *uV)
{
	u16 value;
	int ret;

	ret = p9222_read_word(chip, P9222_REG_VRECT, &value);
	if (ret) {
		*uV = 0;
		return ret;
	}

	*uV = value * 1000;

	return 0;
}

static int p9222_get_adc_iout(struct p9222 *chip, u32 *uA)
{
	u16 value;
	int ret;

	ret = p9222_read_word(chip, P9222_REG_RX_IOUT, &value);
	if (ret) {
		*uA = 0;
		return ret;
	}

	*uA = value * 1000;

	return 0;
}

static int p9222_get_adc_die_temp(struct p9222 *chip, int *mdeg)
{
	u16 value;
	int ret;

	ret = p9222_read_word(chip, P9222_REG_ADC_DIE_TEMP, &value);
	if (ret) {
		*mdeg = -127000;
		return ret;
	}

	if (chip->fw_rev < 0x510) {
		*mdeg = (value * 898 - 2290000) / 10;
		return 0;
	}

	*mdeg = value * 1000;

	return 0;
}

static int p9222_set_negotiated_power(struct p9222 *chip, u32 uW)
{
	u8 value;

	if (uW > 15000000)
		uW = 15000000;

	value = uW / 500000;

	return p9222_write(chip, P9222_REG_MP_REQ_NP, value);
}

static int p9222_set_maximum_power(struct p9222 *chip, u32 uW)
{
	u8 value;

	if (uW > 15000000)
		uW = 15000000;

	value = uW / 500000;

	return p9222_write(chip, P9222_REG_MP_REQ_MP, value);
}

static int p9222_get_vout_set(struct p9222 *chip, u32 *uV)
{
	u8 value;
	int ret;

	ret = p9222_read(chip, P9222_REG_VOUT_SET, &value);
	if (ret)
		return ret;

	*uV = 3500000 + (value * 100000);

	return 0;
}

static int p9222_set_vout_set(struct p9222 *chip, u32 uV)
{
	u8 value;

	if (uV < 3500000)
		uV = 3500000;
	if (uV > 12500000)
		uV = 12500000;

	value = (uV - 3500000) / 100000;

	return p9222_write(chip, P9222_REG_VOUT_SET, value);
}

static int p9222_set_vrect_target(struct p9222 *chip, u32 uV)
{
	u16 value = ((uV / 21) << 12) / 1000000;

	return p9222_write_word(chip, P9222_REG_MP_VR_CALIB_M1, value);
}

static int p9222_enable_irqstat(struct p9222 *chip, enum p9222_irqstat irqstat)
{
	u16 int_enable = 0;
	int ret;

	if (irqstat > IRQSTAT_DATA_RECEIVED)
		return -EINVAL;

	ret = p9222_read_word(chip, P9222_REG_INT_ENABLE, &int_enable);
	if (ret)
		return ret;

	int_enable |= BIT(irqstat);

	return p9222_write_word(chip, P9222_REG_INT_ENABLE, int_enable);
}

static int p9222_set_fod(struct p9222 *chip, const u8 *fod)
{
	return p9222_write_block(chip, P9222_FOD_COEF_START, fod,
			P9222_FOD_COEF_SIZE);
}

static int p9222_send_ept(struct p9222 *chip, enum p9222_ept ept)
{
	const char *ept_str[] = {
		[EPT_UNKNOWN] = "unknown",
		[EPT_EOC] = "eoc",
		[EPT_INTERNAL_FAULT] = "internal fault",
		[EPT_OVER_TEMPERATURE] = "over temperature",
		[EPT_OVER_VOLTAGE] = "over voltage",
		[EPT_OVER_CURRENT]  = "over current",
		[EPT_BATTERY_FAILURE] = "battery failure",
		[EPT_RECONFIGURATION] = "reconfiguration",
		[EPT_NO_RESPONSE] = "no response",
		[EPT_NEGOTIATION_FAILURE] = "negotiation failure",
		[EPT_RESTART_POWER_TRANSFER] = "restart power transfer",
	};
	int ret;

	if (!ept_str[ept])
		return -EINVAL;

	mutex_lock(&chip->ept_lock);

	ret = p9222_write(chip, P9222_REG_EPT, ept);
	if (ret)
		goto out;

	ret = p9222_write_word(chip, P9222_REG_COM, P9222_COM_SEND_EOP);

out:
	mutex_unlock(&chip->ept_lock);
	if (ret) {
		dev_err(chip->dev, "failed to send ept %s\n", ept_str[ept]);
		return ret;
	}

	chip->ept_sent = true;
	dev_info(chip->dev, "ept %s sent\n", ept_str[ept]);

	return 0;
}

static int p9222_send_chg_status(struct p9222 *chip, int capacity)
{
	int ret;

	if (capacity <= 0 || capacity > 100)
		return -EINVAL;

	mutex_lock(&chip->cs_lock);

	ret = p9222_write(chip, P9222_REG_CHG_STATUS, capacity);
	if (ret)
		goto out;

	ret = p9222_write_word(chip, P9222_REG_COM, P9222_COM_SEND_CHG_STATUS);

out:
	mutex_unlock(&chip->cs_lock);
	if (ret) {
		dev_err(chip->dev, "failed to send charge status\n");
		return ret;
	}

	if (capacity == 100)
		dev_info(chip->dev, "charge status: full\n");

	return 0;
}

static inline void p9222_dump_tx_info(struct p9222 *chip,
				      struct p9222_tx_info tx)
{
	dev_info(chip->dev, "tx manufacturer: 0x%02x\n", tx.manufacturer);
	dev_info(chip->dev, "tx spec revision: 0x%02x\n", tx.spec_rev);
	dev_info(chip->dev, "tx guaranteed power: %dmW\n",
			tx.guaranteed_power / 1000);
	dev_info(chip->dev, "tx potential power: %dmW\n",
			tx.potential_power / 1000);
}

static bool p9222_is_valid_tx(struct p9222 *chip)
{
	struct p9222_tx_info tx = chip->tx;

	if (!tx.guaranteed_power && !tx.potential_power)
		return false;

	return true;
}

static void p9222_tx_info_work(struct work_struct* work)
{
	struct p9222 *chip = container_of(work, struct p9222, tx_info_work);
	struct p9222_tx_info info = tx_info_default;
	u8 value = 0;
	int ret;

	ret = p9222_read(chip, P9222_REG_TX_MANU_CODE, &value);
	if (ret) {
		dev_err(chip->dev, "failed to read tx manufacturer\n");
		return;
	}
	info.manufacturer = value;

	ret = p9222_read(chip, P9222_REG_SPEC_REV, &value);
	if (ret) {
		dev_err(chip->dev, "failed to read tx spec revision\n");
		return;
	}
	info.spec_rev = value;

	ret = p9222_read(chip, P9222_REG_TX_GUARANTEED_PWR, &value);
	if (ret) {
		dev_err(chip->dev, "failed to read tx guaranteed power\n");
		return;
	}
	info.guaranteed_power = value * 500000;

	ret = p9222_read(chip, P9222_REG_TX_POTENTIAL_PWR, &value);
	if (ret) {
		dev_err(chip->dev, "failed to read tx potential power\n");
		return;
	}
	info.potential_power = value * 500000;

	chip->tx = info;

	if (!p9222_is_valid_tx(chip)) {
		dev_warn(chip->dev, "invalid tx\n");
		if (chip->mode2) {
			chip->mode2 = false;
			charger_dev_notify(chip->chg_dev,
					CHARGER_DEV_NOTIFY_MODE_CHANGE);
		}
		schedule_work(&chip->chg_status_work);

		return;
	}

	p9222_dump_tx_info(chip, chip->tx);

	return;
}

static void p9222_set_online(struct p9222 *chip, bool online)
{
	union power_supply_propval propval;
	int ret = 0;

	if (chip->online == online)
		return;

	chip->online = online;
	dev_info(chip->dev, "p9222 %s\n", online ? "online" : "offline");

	if (!online) {
		ret = power_supply_get_property(chip->chg_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
		if (ret < 0)
			dev_err(chip->dev, "get chg psy fail(%d)\n", ret);

		if (propval.intval != WIRELESS_CHARGER) {
			dev_err(chip->dev, "current charger type is %d, "
					"skip inform\n", propval.intval);
			return;
		}
	}

	propval.intval = online ? 1 : 0;
	ret = power_supply_set_property(chip->chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &propval);
	if (ret < 0)
		dev_err(chip->dev, "psy online fail(%d)\n", ret);

	propval.intval = (online) ? WIRELESS_CHARGER : CHARGER_UNKNOWN;
	ret = power_supply_set_property(chip->chg_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
	if (ret < 0)
		dev_err(chip->dev, "psy type fail(%d)\n", ret);
}

static void p9222_offline_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct p9222 *chip = container_of(dwork, struct p9222, offline_work);
	unsigned int recheck = chip->offline_timeout_ms;
	static int vrect_cnt = 0;

	__pm_stay_awake(&chip->offline_ws);
	mutex_lock(&chip->tx_detect_lock);

	if (!chip->enabled || !p9222_is_pad_detected(chip))
		goto offline;

	if (!chip->online || p9222_is_vrect_valid(chip))
		goto out;

	if (chip->ept_sent) {
		dev_info(chip->dev, "ignore offline (ept sent)\n");
		goto out;
	}

	/* offline work scheduled without vrect. set offline */
	if (vrect_cnt == atomic_read(&chip->vrect_cnt))
		goto offline;

	if (!recheck)
		goto offline;

	vrect_cnt = atomic_read(&chip->vrect_cnt);
	dev_info(chip->dev, "check again after %dms\n", recheck);
	schedule_delayed_work(dwork, msecs_to_jiffies(recheck));

	mutex_unlock(&chip->tx_detect_lock);

	return;

offline:
	p9222_set_online(chip, false);
	chip->tx = tx_info_default;
	chip->req_param = NULL;
out:
	chip->ept_sent = false;
	mutex_unlock(&chip->tx_detect_lock);
	__pm_relax(&chip->offline_ws);
}

static int p9222_set_param(struct p9222 *chip, struct p9222_param *param)
{
	int ret = 0;

	if (chip->boot_tx_epp) {
		dev_info(chip->dev, "ignore set parameters. boot tx epp\n");
		return 0;
	}

	if (chip->rx_mode == RX_MODE_WPC_EPP) {
		dev_info(chip->dev, "request power: %dmW\n",
				param->power / 1000);
		ret = p9222_set_negotiated_power(chip, param->power);
		if (ret)
			dev_err(chip->dev, "failed to set negotiated power\n");

		ret = p9222_set_maximum_power(chip, param->power);
		if (ret)
			dev_err(chip->dev, "failed to set maximum power\n");

		ret = p9222_set_vout_set(chip, param->vout);
		if (ret)
			dev_err(chip->dev, "failed to set vout\n");

		ret = p9222_set_vrect_target(chip, param->vrect);
		if (ret)
			dev_err(chip->dev, "failed to set vrect target\n");
	}

	ret = p9222_set_fod(chip, param->fod);
	if (ret)
		dev_err(chip->dev, "failed to set fod\n");

	dev_info(chip->dev, "set parameters done\n");

	return ret;
}

static struct p9222_param *p9222_get_param(struct p9222 *chip)
{
	int maxidx = chip->num_epp_param;
	int idx;

	if (chip->rx_mode == RX_MODE_WPC_BPP)
		return &chip->bpp_param;

	if (!chip->epp_param)
		return &chip->bpp_param;

	for (idx = 0; idx < maxidx; idx++) {
		if (chip->epp_param[idx].power <= chip->tx.guaranteed_power)
			return &chip->epp_param[idx];
	}

	return &chip->epp_param[0];
}

static int p9222_wait_vrect_stable(struct p9222 *chip, unsigned int msec)
{
	int time_left;

	reinit_completion(&chip->vrect_fall);

	if (!p9222_is_vrect_valid(chip))
		return -ENODEV;

	time_left = wait_for_completion_timeout(&chip->vrect_fall,
			msecs_to_jiffies(msec));
	if (time_left)
		return -ENODEV;

	if (!p9222_is_vrect_valid(chip))
		return -ENODEV;

	return 0;
}

static void p9222_online_work(struct work_struct *work)
{
	struct p9222 *chip = container_of(work, struct p9222, online_work);
	int ret;

	__pm_stay_awake(&chip->online_ws);
	mutex_lock(&chip->tx_detect_lock);

	ret = p9222_wait_vrect_stable(chip, 100);
	if (ret) {
		dev_err(chip->dev, "vrect unstable\n");
		goto out;
	}

	chip->ept_sent = false;
	chip->rx_mode = p9222_get_rx_mode(chip);
	if (chip->rx_mode == RX_MODE_WPC_BPP)
		chip->tx = tx_info_bpp;
	p9222_dump_rx_mode(chip, chip->rx_mode);

	chip->req_param = p9222_get_param(chip);
	ret = p9222_set_param(chip, chip->req_param);
	if (ret)
		dev_err(chip->dev, "failed to set parameters\n");

	ret = p9222_check_firmware(chip);
	if (ret)
		dev_err(chip->dev, "failed to check firmware\n");

	/* from fw509, stat_vout will be enabled as default */
	if (chip->fw_rev < 0x509)
		p9222_enable_irqstat(chip, IRQSTAT_STAT_VOUT);

	p9222_set_online(chip, true);

out:
	mutex_unlock(&chip->tx_detect_lock);
	__pm_relax(&chip->online_ws);
}

static void p9222_chg_status_work(struct work_struct *work)
{
	struct p9222 *chip = container_of(work, struct p9222, chg_status_work);
	int capacity = chip->capacity;

	if (!p9222_is_vrect_valid(chip))
		return;

	if (chip->boot_tx_epp) {
		dev_info(chip->dev, "ingore charge status. boot tx epp\n");

		/* if battery charged enough, send ept to re-negotiate power */
		if (capacity >= chip->boot_tx_epp_rst_soc)
			p9222_send_ept(chip, EPT_RESTART_POWER_TRANSFER);

		return;
	}

	if (!p9222_is_vout_valid(chip))
		return;

	/* do not send charge status in mode 2 */
	if (chip->rx_mode == RX_MODE_WPC_EPP && chip->mode2)
		return;

	p9222_send_chg_status(chip, chip->capacity);
}

static int p9222_psy_notifier_call(struct notifier_block *nb,
				   unsigned long event, void *v)
{
	struct p9222 *chip = container_of(nb, struct p9222, nb);
	struct power_supply *psy = (struct power_supply*)v;
	union power_supply_propval val;
	int ret;

	if (strcmp(psy->desc->name, "battery"))
		return NOTIFY_DONE;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (ret)
		return NOTIFY_DONE;

	if (val.intval <= 0 || val.intval > 100)
		return NOTIFY_DONE;

	if (val.intval == chip->capacity)
		return NOTIFY_DONE;

	chip->capacity = val.intval;

	if (!chip->online)
		return NOTIFY_DONE;

	schedule_work(&chip->chg_status_work);

	return NOTIFY_DONE;
}

static void p9222_post_boot_work(struct work_struct *work)
{
	struct p9222 *chip = container_of(work, struct p9222, post_boot_work);

	if (!p9222_is_vrect_valid(chip))
		return;

	if (chip->online)
		return;

	if (p9222_get_rx_mode(chip) == RX_MODE_WPC_EPP)
		chip->boot_tx_epp = true;

	dev_info(chip->dev, "boot tx is %s\n",
			chip->boot_tx_epp ? "epp" : "bpp");

	schedule_work(&chip->online_work);
	if (p9222_is_vout_valid(chip))
		schedule_work(&chip->tx_info_work);
}

static irqreturn_t p9222_over_temperature_irq_handler(struct p9222 *chip,
						      u16 status)
{
	if (!(status & BIT(IRQSTAT_OVER_TEMPERATURE)))
		return IRQ_HANDLED;

	dev_info(chip->dev, "over temperature\n");

	return IRQ_HANDLED;
}

static irqreturn_t p9222_over_current_irq_handler(struct p9222 *chip,
						  u16 status)
{
	if (!(status & BIT(IRQSTAT_OVER_CURRENT)))
		return IRQ_HANDLED;

	dev_info(chip->dev, "over current\n");

	return IRQ_HANDLED;
}

static irqreturn_t p9222_over_voltage_irq_handler(struct p9222 *chip,
						  u16 status)
{
	if (!(status & BIT(IRQSTAT_OVER_VOLTAGE)))
		return IRQ_HANDLED;

	dev_info(chip->dev, "over voltage\n");

	return IRQ_HANDLED;
}

static irqreturn_t p9222_operation_mode_irq_handler(struct p9222 *chip,
						    u16 status)
{
	enum p9222_rx_mode rx_mode;

	if (!(status & BIT(IRQSTAT_OPERATION_MODE)))
		return IRQ_HANDLED;

	dev_info(chip->dev, "operation mode\n");

	rx_mode = p9222_get_rx_mode(chip);
	if (rx_mode == chip->rx_mode)
		return IRQ_HANDLED;

	if (rx_mode == RX_MODE_WPC_BPP || rx_mode == RX_MODE_WPC_EPP)
		schedule_work(&chip->online_work);

	return IRQ_HANDLED;
}

static irqreturn_t p9222_stat_vout_irq_handler(struct p9222 *chip, u16 status)
{
	bool active = false;

	if (status & BIT(IRQSTAT_STAT_VOUT))
		active = true;

	if (status & BIT(IRQSTAT_AC_MISSING_DETECT))
		active = false;

	dev_info(chip->dev, "stat vout %s\n", active ? "active" : "inactive");

	if (active && chip->rx_mode == RX_MODE_WPC_EPP) {
		chip->mode2 = true;
		schedule_work(&chip->tx_info_work);
	}

	if (active && chip->rx_mode == RX_MODE_WPC_BPP)
		schedule_work(&chip->chg_status_work);

	if (IS_ERR_OR_NULL(chip->chg_dev))
		return IRQ_HANDLED;

	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_MODE_CHANGE);

	return IRQ_HANDLED;
}

static irqreturn_t p9222_extended_mode_irq_handler(struct p9222 *chip,
						   u16 status)
{
	if (!(status & BIT(IRQSTAT_EXTENDED_MODE)))
		return IRQ_HANDLED;

	chip->mode2 = false;

	dev_info(chip->dev, "epp calibration mode 2 end\n");

	if (chip->req_param != p9222_get_param(chip)) {
		dev_info(chip->dev, "start re-negotiation\n");
		p9222_send_ept(chip, EPT_RESTART_POWER_TRANSFER);

		return IRQ_HANDLED;
	}

	schedule_work(&chip->chg_status_work);

	if (IS_ERR_OR_NULL(chip->chg_dev))
		return IRQ_HANDLED;

	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_MODE_CHANGE);

	return IRQ_HANDLED;
}

static irqreturn_t (*p9222_irq_handlers[])(struct p9222 *, u16) = {
	[IRQSTAT_OVER_TEMPERATURE] = p9222_over_temperature_irq_handler,
	[IRQSTAT_OVER_CURRENT] = p9222_over_current_irq_handler,
	[IRQSTAT_OVER_VOLTAGE] = p9222_over_voltage_irq_handler,
	[IRQSTAT_OPERATION_MODE] = p9222_operation_mode_irq_handler,
	[IRQSTAT_STAT_VRECT] = NULL,
	[IRQSTAT_STAT_VOUT] = p9222_stat_vout_irq_handler,
	[IRQSTAT_ADT_SENT] = NULL,
	[IRQSTAT_ADT_RECEIVED] = NULL,
	[IRQSTAT_AC_MISSING_DETECT] = NULL,
	[IRQSTAT_EXTENDED_MODE] = p9222_extended_mode_irq_handler,
	[IRQSTAT_IDT_DEBUG] = NULL,
	[IRQSTAT_DATA_RECEIVED] = NULL,
};

static irqreturn_t p9222_threaded_irq_handler(int irq, void* data)
{
	struct p9222 *chip = (struct p9222 *)data;
	u16 interrupt = 0, status = 0;
	unsigned int i;
	int ret;

	ret = p9222_read_word(chip, P9222_REG_INT, &interrupt);
	if (ret) {
		dev_err(chip->dev, "failed to read interrupt\n");
		return IRQ_HANDLED;
	}

	ret = p9222_read_word(chip, P9222_REG_STATUS, &status);
	if (ret) {
		dev_err(chip->dev, "failed to read status\n");
		return IRQ_HANDLED;
	}

	ret = p9222_write_word(chip, P9222_REG_INT_CLEAR, interrupt);
	if (ret)
		dev_err(chip->dev, "failed to set int clear\n");

	ret = p9222_write_word(chip, P9222_REG_COM, P9222_COM_CLEAR_INT);
	if (ret)
		dev_err(chip->dev, "failed to clear interrupt\n");

	dev_info(chip->dev, "int: 0x%04x, stat: 0x%04x\n", interrupt, status);

	for (i = 0; i < ARRAY_SIZE(p9222_irq_handlers); i++) {
		if (!(interrupt & BIT(i)))
			continue;
		if (!p9222_irq_handlers[i])
			continue;

		p9222_irq_handlers[i](chip, status);
	}

	return IRQ_HANDLED;
}

static irqreturn_t p9222_irq_handler(int irq, void* data)
{
	struct p9222 *chip = (struct p9222 *)data;

	if (p9222_is_vrect_valid(chip))
		return IRQ_WAKE_THREAD;

	return IRQ_HANDLED;
}

static irqreturn_t p9222_vrect_irq_handler(int irq, void *data)
{
	struct p9222 *chip = (struct p9222 *)data;
	bool vrect_valid = p9222_is_vrect_valid(chip);

	atomic_inc(&chip->vrect_cnt);
	dev_info(chip->dev, "vrect %s\n", vrect_valid ? "valid" : "invalid");

	chip->mode2 = false;
	chip->boot_tx_epp = false;

	if (!vrect_valid) {
		complete(&chip->vrect_fall);
		schedule_delayed_work(&chip->offline_work, 0);

		return IRQ_HANDLED;
	}

	if (!chip->enabled)
		return IRQ_HANDLED;

	schedule_work(&chip->online_work);

	return IRQ_HANDLED;
}

static irqreturn_t p9222_pdetb_irq_handler(int irq, void* data)
{
	struct p9222 *chip = (struct p9222 *)data;

	if (p9222_is_pad_detected(chip))
		return IRQ_HANDLED;

	if (!chip->online)
		return IRQ_HANDLED;

	chip->rx_mode = RX_MODE_AC_MISSING;
	dev_info(chip->dev, "pad detached\n");

	cancel_delayed_work(&chip->offline_work);
	schedule_delayed_work(&chip->offline_work, 0);

	return IRQ_HANDLED;
}

static int p9222_enable(struct charger_device *chg_dev, bool en)
{
	struct p9222 *chip = charger_get_data(chg_dev);
	int en_gpio = chip->gpios[P9222_GPIO_EN].gpio;
	int ret = 0;

	if (en == chip->enabled)
		return 0;

	/* Innotek TX Pad Workaround */
	if (p9222_is_vrect_valid(chip) && !en) {
		if (chip->tx.manufacturer == TX_MANUFACTURER_LG_INNOTEK)
			p9222_send_ept(chip, EPT_NO_RESPONSE);
	}

	if (en)
		chip->enabled = en;

	gpio_set_value(en_gpio, en ? 0 : 1);

	if (!en)
		chip->enabled = en;

	return ret;
}

static bool p9222_is_vout_settled(struct p9222 *chip, u32 uV)
{
	u32 vout;
	int i;
	int ret;

	for (i = 0; i < 5; i++) {
		if (!p9222_is_vrect_valid(chip))
			return -ENODEV;

		msleep(100);

		if (!p9222_is_vrect_valid(chip))
			return -ENODEV;

		ret = p9222_get_adc_vout(chip, &vout);
		if (ret) {
			dev_err(chip->dev, "failed to get vout\n");
			return ret;
		}

		if (abs(uV - vout) < 500000)
			return true;
	}

	dev_err(chip->dev, "vout not settled yet. requested: %uuV "
			   "actual: %uuV\n", uV, vout);

	return false;
}

static int p9222_set_constant_voltage(struct charger_device *chg_dev, u32 uV)
{
	struct p9222 *chip = charger_get_data(chg_dev);
	u32 vout = 0;
	int ret = 0;

	if (!chip->online)
		return -ENODEV;

	mutex_lock(&chip->vout_lock);

	ret = p9222_get_vout_set(chip, &vout);
	if (!ret && vout == uV)
		goto out;

	dev_info(chip->dev, "set vout_set: %uuV", uV);

	if (!chip->vout_set_step)
		goto no_step;
	if (vout < uV)
		goto no_step;

	/* step down vout for reduce ic temperature */
	while (vout - uV > chip->vout_set_step) {
		vout = vout - chip->vout_set_step;
		dev_info(chip->dev, "step down vout_set : %uuV", vout);

		ret = p9222_set_vout_set(chip, vout);
		if (ret)
			dev_err(chip->dev, "failed to step down vout_set\n");

		p9222_is_vout_settled(chip, vout);
	}

	dev_info(chip->dev, "step down vout_set : %uuV", uV);
no_step:
	ret = p9222_set_vout_set(chip, uV);
	if (ret) {
		dev_err(chip->dev, "failed to set vout_set\n");
		goto out;
	}

	if (!p9222_is_vout_settled(chip, uV))
		ret = -EAGAIN;

out:
	mutex_unlock(&chip->vout_lock);

	return 0;
}

static int p9222_get_constant_voltage(struct charger_device *chg_dev, u32 *uV)
{
	struct p9222 *chip = charger_get_data(chg_dev);

	if (!p9222_is_vrect_valid(chip)) {
		*uV = 0;
		return 0;
	}

	return p9222_get_vout_set(chip, uV);
}

static int p9222_get_vbus_adc(struct charger_device *chg_dev, u32 *vbus)
{
	struct p9222 *chip = charger_get_data(chg_dev);
	u16 vout = 0;
	int ret;

	if (!p9222_is_vrect_valid(chip)) {
		*vbus = 0;
		return 0;
	}

	ret = p9222_read_word(chip, P9222_REG_VOUT, &vout);
	if (ret)
		return ret;

	*vbus = vout * 1000;

	return 0;
}

static int p9222_get_power(struct charger_device *chg_dev, int *uW)
{
	struct p9222 *chip = charger_get_data(chg_dev);

	if (chip->rx_mode == RX_MODE_WPC_BPP) {
		*uW = P9222_POWER_DEFAULT;
		return 0;
	}

	if (chip->boot_tx_epp) {
		*uW = P9222_POWER_DEFAULT;
		return 0;
	}

	if (!p9222_is_valid_tx(chip)) {
		*uW = P9222_POWER_INVALID_EPP;
		return 0;
	}

	*uW = chip->tx.guaranteed_power;

	return 0;
}

static int p9222_get_status(struct charger_device *chg_dev,
			    enum chg_stat status, u32 *val)
{
	struct p9222 *chip = charger_get_data(chg_dev);

	if (status >= CHG_STAT_MAX)
		return -EINVAL;

	switch (status) {
	case WLESS_CHG_STAT_EPP:
		*val = (chip->rx_mode == RX_MODE_WPC_EPP) ? 1 : 0;
		break;
	case WLESS_CHG_STAT_LP_EPP:
		*val = 0;
		break;
	case WLESS_CHG_STAT_CALI:
		*val = chip->mode2 ? 1 : 0;
		break;
	case WLESS_CHG_STAT_EPT:
		*val = chip->overheat;
		break;
	default:
		return -ENOTSUPP;
	}

	return 0;
}

static int p9222_set_status(struct charger_device *chg_dev,
			    enum chg_stat status, u32 val)
{
	struct p9222 *chip = charger_get_data(chg_dev);

	if (status != WLESS_CHG_STAT_EPT)
		return -EINVAL;

	if (!p9222_is_vrect_valid(chip))
		return -EPERM;

	switch (val) {
	case WLESS_EPT_UNKNOWN:
	case WLESS_EPT_EOC:
	case WLESS_EPT_INTERNAL_FAULT:
		break;
	case WLESS_EPT_OVER_TEMPERATURE:
		p9222_send_ept(chip, EPT_OVER_TEMPERATURE);
		chip->overheat = true;
		break;
	case WLESS_EPT_OVER_VOLTAGE:
	case WLESS_EPT_OVER_CURRENT:
	case WLESS_EPT_BATTERY_FAIL:
	case WLESS_EPT_RECONFIGURATION:
	case WLESS_EPT_NO_RESPONSE:
		break;
	default:
		dev_err(chip->dev, "no such status(%d) defined\n", status);
		return -ENOTSUPP;
	}

	return 0;
}

static int p9222_dump_registers(struct charger_device *chg_dev)
{
	struct p9222 *chip = charger_get_data(chg_dev);
	u32 vout, vrect, iout;
	int temp;

	if (!chip->enabled)
		return 0;

	if (!p9222_is_vrect_valid(chip))
		return -EPERM;

	p9222_get_adc_vrect(chip, &vrect);
	p9222_get_adc_vout(chip, &vout);
	p9222_get_adc_iout(chip, &iout);
	p9222_get_adc_die_temp(chip, &temp);
	temp /= 100;

	dev_info(chip->dev, "Vrect: %dmV, Vout: %dmV, Iout: %dmA, "
			    "Temp: %s%d.%dDeg\n",
			vrect / 1000, vout / 1000, iout / 1000,
			temp < 0 ? "-" : "", abs(temp / 10), abs(temp % 10));

	return 0;
}

static struct charger_ops p9222_chg_ops = {
	.enable = p9222_enable,
	.get_constant_voltage = p9222_get_constant_voltage,
	.set_constant_voltage = p9222_set_constant_voltage,
	.get_vbus_adc = p9222_get_vbus_adc,
	.get_power = p9222_get_power,
	.get_status = p9222_get_status,
	.set_status = p9222_set_status,
	.dump_registers = p9222_dump_registers,
};

static struct p9222_reg {
	u16 addr;
	u32 size;
} p9222_regs_to_dump[] = {
	/* Identification */
	{ .addr = P9222_REG_CHIP_ID, .size = 1 },
	{ .addr = P9222_REG_CHIP_REV, .size = 1 },
	{ .addr = P9222_REG_CUSTOMER_ID, .size = 1 },
	{ .addr = P9222_REG_FW_MAJOR_REV, .size = 2 },
	{ .addr = P9222_REG_FW_MINOR_REV, .size = 2 },
	{ .addr = P9222_REG_PRMC_ID, .size = 2 },
	/* Status & Interrupt */
	{ .addr = P9222_REG_STATUS, .size = 2 },
	{ .addr = P9222_REG_INT_ENABLE, .size = 2 },
	{ .addr = P9222_REG_SYS_OP_MODE, .size = 1 },
	/* Battery & Power Transfer */
	{ .addr = P9222_REG_CHG_STATUS, .size = 1 },
	{ .addr = P9222_REG_EPT, .size = 1 },
	/* Operation Parameters */
	{ .addr = P9222_REG_VOUT, .size = 2 },
	{ .addr = P9222_REG_VOUT_SET, .size = 1 },
	{ .addr = P9222_REG_VRECT_ADJ, .size = 1 },
	{ .addr = P9222_REG_VRECT, .size = 2 },
	{ .addr = P9222_REG_RX_IOUT, .size = 2 },
	{ .addr = P9222_REG_ADC_DIE_TEMP, .size = 2 },
	{ .addr = P9222_REG_OP_FREQ, .size = 2 },
	{ .addr = P9222_REG_PING_FREQ, .size = 2 },
	{ .addr = P9222_REG_ILIM_SET, .size = 1 },
	/* Foreign Ojbect Detection */
	{ .addr = P9222_REG_FOD_COEF_GAIN_0, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_OFFSET_0, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_GAIN_1, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_OFFSET_1, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_GAIN_2, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_OFFSET_2, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_GAIN_3, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_OFFSET_3, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_GAIN_4, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_OFFSET_4, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_GAIN_5, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_OFFSET_5, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_GAIN_6, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_OFFSET_6, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_GAIN_7, .size = 1 },
	{ .addr = P9222_REG_FOD_COEF_OFFSET_7, .size = 1 },
	/* RX Mode */
	{ .addr = P9222_REG_TX_GUARANTEED_PWR, .size = 1 },
	{ .addr = P9222_REG_TX_POTENTIAL_PWR, .size = 1 },
	{ .addr = P9222_REG_MP_REQ_NP, .size = 1 },
	{ .addr = P9222_REG_MP_REQ_MP, .size = 1 },
	{ .addr = P9222_REG_QF, .size = 1 },
	{ .addr = P9222_REG_MP_VR_CALIB_M1, .size = 2 },
	{ .addr = P9222_REG_SPEC_REV, .size = 1 },
	{ .addr = P9222_REG_TX_MANU_CODE, .size = 1 },
	/* HW Control & Monitor */
	{ .addr = P9222_REG_OVP_CLAMP, .size = 1 },
	{ .addr = P9222_REG_CMFET, .size = 1 },
};

static int debugfs_get_data(void *data, u64 *val)
{
	struct p9222 *chip = data;
	int ret;
	u8 temp;

	if (!p9222_is_vrect_valid(chip))
		return -EPERM;

	ret = p9222_read(chip, chip->debug_addr, &temp);
	if (ret)
		return -EIO;

	*val = temp;

	return 0;
}

static int debugfs_set_data(void *data, u64 val)
{
	struct p9222 *chip = data;
	int ret;
	u8 temp;

	if (!p9222_is_vrect_valid(chip))
		return -EPERM;

	temp = (u8)val;
	ret = p9222_write(chip, chip->debug_addr, temp);
	if (ret)
		return -EIO;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(data_debugfs_ops,
	debugfs_get_data, debugfs_set_data, "0x%02llx\n");

static void dump_debugfs_u8_show(struct seq_file *m, unsigned int idx)
{
	struct p9222 *chip = m->private;
	u8 data;
	int ret;

	ret = p9222_read(chip, p9222_regs_to_dump[idx].addr, &data);
	if (ret) {
		seq_printf(m, "0x%04x=error\n", p9222_regs_to_dump[idx].addr);
		return;
	}

	seq_printf(m, "0x%04x=0x%02x\n", p9222_regs_to_dump[idx].addr, data);
}

static void dump_debugfs_u16_show(struct seq_file *m, unsigned int idx)
{
	struct p9222 *chip = m->private;
	u16 data;
	int ret;

	ret = p9222_read_word(chip, p9222_regs_to_dump[idx].addr, &data);
	if (ret) {
		seq_printf(m, "0x%02x=error\n", p9222_regs_to_dump[idx].addr);
		return;
	}

	seq_printf(m, "0x%04x=0x%04x\n", p9222_regs_to_dump[idx].addr, data);
}

static int dump_debugfs_show(struct seq_file *m, void *start)
{
	struct p9222 *chip = m->private;
	unsigned int i;

	if (!p9222_is_vrect_valid(chip))
		return -EPERM;

	for (i = 0; i < ARRAY_SIZE(p9222_regs_to_dump); i++) {
		switch (p9222_regs_to_dump[i].size) {
		case 1:
			dump_debugfs_u8_show(m, i);
			break;
		case 2:
			dump_debugfs_u16_show(m, i);
			break;
		default:
			break;
		}
	}

	return 0;
}

static int dump_debugfs_open(struct inode *inode, struct file *file)
{
	struct p9222 *chip = inode->i_private;

	return single_open(file, dump_debugfs_show, chip);
}

static const struct file_operations dump_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= dump_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int create_debugfs_entries(struct p9222 *chip)
{
	struct dentry *ent;

	chip->debugfs = debugfs_create_dir("p9222", NULL);
	if (!chip->debugfs) {
		dev_err(chip->dev, "failed to create debugfs\n");
		return -ENODEV;
	}

	ent = debugfs_create_x32("addr", S_IFREG | S_IWUSR | S_IRUGO,
		chip->debugfs, &chip->debug_addr);
	if (!ent)
		dev_err(chip->dev, "failed to create addr debugfs\n");

	ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
		chip->debugfs, chip, &data_debugfs_ops);
	if (!ent)
		dev_err(chip->dev, "failed to create data debugfs\n");

	ent = debugfs_create_file("dump", S_IFREG | S_IRUGO,
		chip->debugfs, chip, &dump_debugfs_ops);
	if (!ent)
		dev_err(chip->dev, "failed to create dump debugfs\n");

	return 0;
}

static int p9222_init_irq(struct p9222 *chip)
{
	int irq;
	int ret = 0;

	irq = gpio_to_irq(chip->gpios[P9222_GPIO_VRECT].gpio);
	if (irq < 0)
		return -EINVAL;

	ret = devm_request_threaded_irq(chip->dev, irq, NULL,
			p9222_vrect_irq_handler, IRQF_ONESHOT |
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"p9222-vrect", chip);
	if (ret) {
		dev_err(chip->dev, "failed to request irq %d (%d)\n",
				irq, ret);
		return ret;
	}
	enable_irq_wake(irq);

	irq = gpio_to_irq(chip->gpios[P9222_GPIO_PDETB].gpio);
	if (irq < 0)
		return -EINVAL;

	ret = devm_request_threaded_irq(chip->dev, irq, NULL,
			p9222_pdetb_irq_handler, IRQF_ONESHOT |
			IRQF_TRIGGER_RISING, "p9222-pdetb", chip);
	if (ret) {
		dev_err(chip->dev, "failed to request irq %d (%d)\n",
				irq, ret);
		return ret;
	}
	enable_irq_wake(irq);

	irq = gpio_to_irq(chip->gpios[P9222_GPIO_IRQ].gpio);
	if (irq < 0)
		return -EINVAL;

	ret = devm_request_threaded_irq(chip->dev, irq, p9222_irq_handler,
			p9222_threaded_irq_handler, IRQF_ONESHOT |
			IRQF_TRIGGER_FALLING, "p9222-irq", chip);
	if (ret) {
		dev_err(chip->dev, "failed to request irq %d (%d)\n",
				irq, ret);
		return ret;
	}
	enable_irq_wake(irq);

	return 0;
}

static int p9222_init_gpio(struct p9222 *chip)
{
	int ret;

	ret = gpio_request_array(chip->gpios, ARRAY_SIZE(chip->gpios));
	if (ret < 0)
		dev_err(chip->dev, "failed to request gpios (%d)\n", ret);

	return ret;
}

static void p9222_parse_dt_param(struct p9222 *chip, struct device_node *np,
				 struct p9222_param *param)
{
	int len = 0;
	int ret;

	ret = of_property_read_u32(np, "power", &param->power);
	if (ret < 0)
		param->power = P9222_POWER_DEFAULT;

	ret = of_property_read_u32(np, "vout", &param->vout);
	if (ret < 0)
		param->vout = P9222_VOUT_DEFAULT;

	ret = of_property_read_u32(np, "vrect", &param->vrect);
	if (ret < 0)
		param->vrect = P9222_VRECT_DEFAULT;

	param->fod = (u8 *)of_get_property(np, "fod", &len);
	if (!param->fod || len != P9222_FOD_COEF_SIZE)
		param->fod = NULL;
}

static int p9222_parse_dt_epp(struct p9222 *chip)
{
	struct device_node *np = chip->dev->of_node;
	struct device_node *epp;
	struct p9222_param *param;
	const char *name = NULL;
	int idx, maxidx;
	int ret;

	maxidx = of_property_count_strings(np, "epp-settings");
	if (maxidx <= 0)
		return -EINVAL;

	param = devm_kzalloc(chip->dev, sizeof(*param) * maxidx, GFP_KERNEL);
	if (!param)
		return -ENOMEM;

	for (idx = 0; idx < maxidx; idx++) {
		ret = of_property_read_string_index(np, "epp-settings", idx, &name);
		if (ret)
			goto not_found;

		epp = of_get_child_by_name(np, name);
		if (!epp)
			goto not_found;

		p9222_parse_dt_param(chip, epp, &param[idx]);

		of_node_put(epp);
	}

	chip->epp_param = param;
	chip->num_epp_param = maxidx;

	return 0;

not_found:
	devm_kfree(chip->dev, param);
	return -EINVAL;
}

static int p9222_parse_dt_bpp(struct p9222 *chip)
{
	struct device_node *np = chip->dev->of_node;
	struct device_node *bpp;

	bpp = of_get_child_by_name(np, "bpp");
	if (!bpp)
		return -EINVAL;

	p9222_parse_dt_param(chip, bpp, &chip->bpp_param);

	of_node_put(bpp);

	return 0;
}

static int p9222_parse_dt_gpio(struct p9222 *chip)
{
	struct device_node *np = chip->dev->of_node;
	int gpio;
	int i;

	memcpy(chip->gpios, p9222_gpio_default, sizeof(chip->gpios));
	for (i = 0; i < P9222_GPIO_MAX; i++) {
		gpio = of_get_named_gpio(np, p9222_dt_gpio_name[i], 0);
		if (gpio < 0)
			return -EINVAL;
		chip->gpios[i].gpio = gpio;
	}

	return 0;
}

static int p9222_parse_dt(struct p9222 *chip)
{
	struct device_node *np = chip->dev->of_node;
	int ret;

	if (!np)
		return -ENODEV;

	ret = p9222_parse_dt_gpio(chip);
	if (ret) {
		dev_err(chip->dev, "failed to parse gpio\n");
		return ret;
	}

	p9222_parse_dt_bpp(chip);
	p9222_parse_dt_epp(chip);

	ret = of_property_read_u32(np, "boot-tx-epp-rst-soc",
			&chip->boot_tx_epp_rst_soc);
	if (ret)
		chip->boot_tx_epp_rst_soc = 5;

	ret = of_property_read_u32(np, "offline-timeout-ms",
			&chip->offline_timeout_ms);
	if (ret)
		chip->offline_timeout_ms = 1000;

	ret = of_property_read_u32(np, "vout-set-step", &chip->vout_set_step);
	if (ret)
		chip->vout_set_step = 0;

	return 0;
}

static int p9222_probe(struct i2c_client* client,
		       const struct i2c_device_id* id)
{
	struct p9222 *chip;
	int ret = 0;
	struct power_supply *chg_psy;

	chg_psy = power_supply_get_by_name("charger");
	if (!chg_psy)
		return -EPROBE_DEFER;

	chip = devm_kzalloc(&client->dev, sizeof(struct p9222), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "failed to alloc memory\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, chip);
	chip->client = client;
	chip->dev = &client->dev;
	chip->chg_psy = chg_psy;

	chip->enabled = true;
	chip->mode2 = false;
	chip->ept_sent = false;
	chip->req_param = NULL;
	chip->tx = tx_info_default;
	chip->boot_tx_epp = false;

	mutex_init(&chip->io_lock);
	mutex_init(&chip->ept_lock);
	mutex_init(&chip->cs_lock);
	mutex_init(&chip->vout_lock);
	mutex_init(&chip->tx_detect_lock);

	init_completion(&chip->vrect_fall);
	wakeup_source_init(&chip->online_ws, "p9222_online");
	INIT_WORK(&chip->online_work, p9222_online_work);

	wakeup_source_init(&chip->offline_ws, "p9222_offline");
	INIT_DELAYED_WORK(&chip->offline_work, p9222_offline_work);

	INIT_WORK(&chip->tx_info_work, p9222_tx_info_work);
	INIT_WORK(&chip->chg_status_work, p9222_chg_status_work);
	INIT_WORK(&chip->post_boot_work, p9222_post_boot_work);

	ret = p9222_parse_dt(chip);
	if (ret) {
		dev_err(chip->dev, "failed to parse dt\n");
		return ret;
	}

	ret = p9222_init_gpio(chip);
	if (ret) {
		dev_err(chip->dev, "failed to request gpio at probe\n");
		return ret;
	}

	ret = p9222_init_irq(chip);
	if (ret) {
		dev_err(chip->dev, "failed to request irqs at probe\n");
		return ret;
	}

	chip->nb.notifier_call = p9222_psy_notifier_call;
	ret = power_supply_reg_notifier(&chip->nb);
	if (ret) {
		dev_err(chip->dev, "failed to register notifier\n");
		return ret;
	}

	chip->chg_props.alias_name = "p9222";
	chip->chg_dev = charger_device_register("wless_chg", chip->dev, chip,
			&p9222_chg_ops, &chip->chg_props);
	if (IS_ERR_OR_NULL(chip->chg_dev)) {
		ret = PTR_ERR(chip->chg_dev);
		goto err_chg_dev;
	}

	create_debugfs_entries(chip);

	if (p9222_is_vrect_valid(chip))
		schedule_work(&chip->post_boot_work);

	return 0;

err_chg_dev:
	power_supply_unreg_notifier(&chip->nb);
	return ret;
}

static void p9222_shutdown(struct i2c_client* client)
{
	struct p9222 *chip = i2c_get_clientdata(client);

	if (p9222_is_vrect_valid(chip) && chip->rx_mode == RX_MODE_WPC_EPP)
		p9222_send_ept(chip, EPT_RESTART_POWER_TRANSFER);
}

static int p9222_remove(struct i2c_client* client)
{
	struct p9222 *chip = i2c_get_clientdata(client);

	charger_device_unregister(chip->chg_dev);
	power_supply_unreg_notifier(&chip->nb);

	return 0;
}

static struct of_device_id p9222_of_id_table[] = {
	{ .compatible = "idt,p9222" },
	{ },
};
MODULE_DEVICE_TABLE(of, p9222_of_id_table);

static const struct i2c_device_id p9222_id_table [] = {
	{ .name = "p9222" },
	{ },
};
MODULE_DEVICE_TABLE(i2c, p9222_id_table);

static struct i2c_driver p9222_driver = {
	.driver = {
		.name = "p9222",
		.owner = THIS_MODULE,
		.of_match_table = p9222_of_id_table,
	},
	.probe = p9222_probe,
	.remove = p9222_remove,
	.shutdown = p9222_shutdown,
	.id_table = p9222_id_table,
};
module_i2c_driver(p9222_driver);

MODULE_DESCRIPTION("IDT P9222 Wireless Power Receiver IC");
MODULE_LICENSE("GPL v2");
