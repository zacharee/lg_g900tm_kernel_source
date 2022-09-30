/*iDTP9222 Wireless Power Receiver driver
 *
 * Copyright (C) 2016 LG Electronics, Inc
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt) "P9222: %s: " fmt, __func__

#define pr_idt(reason, fmt, ...)            \
	do {                                        \
		if (p9222_debug & (reason))      \
			pr_err(fmt, ##__VA_ARGS__);     \
		else                                \
			pr_debug(fmt, ##__VA_ARGS__);   \
	} while (0)

#define pr_assert(exp)                      \
	do {                                \
		if ((p9222_debug & IDT_ASSERT) && !(exp)) {      \
			pr_idt(IDT_ASSERT, "Assertion failed\n");   \
		}                           \
	} while (0)


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
#include "tcpm.h"

// Constants
#define P9222_NAME_COMPATIBLE    "idt,p9222"
#define P9222_NAME_DRIVER        "p9222"

/* Mask/Bit helpers */
#define _IDT_MASK(BITS, POS) \
	((unsigned char)(((1 << (BITS)) - 1) << (POS)))
#define IDT_MASK(LEFT_BIT_POS, RIGHT_BIT_POS) \
	_IDT_MASK((LEFT_BIT_POS) - (RIGHT_BIT_POS) + 1, \
		(RIGHT_BIT_POS))

// Register addresses
#define REG_ADDR_FIRMWARE       0x06
#define REG_ADDR_STATUS_L       0x34
#define REG_ADDR_STATUS_H       0x35
#define REG_ADDR_INT_L          0x36
#define REG_ADDR_INT_H          0x37
#define REG_ADDR_OPMODE         0x3F
#define REG_ADDR_VRECT_L        0x54
#define REG_ADDR_VRECT_M        0x55
#define REG_ADDR_CHGSTAT        0x4E
#define REG_ADDR_EPT            0x4F
#define REG_ADDR_VADC_L         0x50
#define REG_ADDR_VADC_M         0x51
#define REG_ADDR_VOUT           0x52
#define REG_ADDR_IADC_L         0x58
#define REG_ADDR_IADC_M         0x59
#define REG_ADDR_COM_L          0x62
#define REG_ADDR_COM_H          0x63
#define REG_ADDR_GRTPWR         0xB4
#define REG_ADDR_POTPWR         0xB5
#define REG_ADDR_MPREQNP        0xBD
#define REG_ADDR_MPREQMP        0xBE
#define REG_ADDR_MPVRCALM1_L    0xE4
#define REG_ADDR_MPVRCALM1_H    0xE5
#define REG_ADDR_SPECREV        0x105
#define REG_ADDR_TXID           0x106
#define REG_ADDR_GLOBALGAIN     0xCF

// For VOUT register
#define VOUT_V5P5       0x14
#define VOUT_V9P0       0x37
#define VOUT_V10P0      0x41
#define VOUT_V12P0      0x55
// For Guaranteed Power Register
#define POWER_10W    0x14
#define POWER_15W    0x1E
// For EPT register
#define EPT_BY_EOC      1
#define EPT_BY_OVERTEMP     3
#define EPT_BY_NORESPONSE   8
#define EPT_BY_RESTART_POWER_TRANSFER   11
// For Operation mode register
#define OPMODE_MASK         IDT_MASK(7, 5)
#define OPMODE_SHIFT        5
#define OPMODE_AC_MISSING   0x0
#define OPMODE_WPC_BPP      0x1
#define OPMODE_WPC_EPP      0x2
#define OPMODE_UNKNOWN      0x7
// For Status register
#define STAT_VOUT_SHIFT     7
#define EXTENDED_MODE       BIT(4)
// For command register
#define SEND_CHGSTAT    BIT(4)
#define SEND_EPT        BIT(3)
#define MCU_RESET       BIT(2)

enum p9222_print {
	IDT_ASSERT  = BIT(0),
	IDT_ERROR   = BIT(1),
	IDT_INTERRUPT   = BIT(2),
	IDT_MONITOR = BIT(3),
	IDT_REGISTER    = BIT(4),
	IDT_RETURN  = BIT(5),
	IDT_UPDATE  = BIT(6),
	IDT_VERBOSE = BIT(7),
};

enum p9222_opmode {
	AC_MISSING = 0,
	WPC_BPP,
	WPC_EPP,
	UNKNOWN = 7,
};

struct p9222_fod {
	char addr;
	char value;
};

struct p9222_struct {
	/* p9222 descripters */
	struct device*      dev;
	struct i2c_client*  i2c;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct wakeup_source    wless_wakelock;
	struct power_supply *psy;
	/* p9222 work struct */
	struct delayed_work wless_det_work;
	struct delayed_work onpad_work;
	struct delayed_work polling_adc;
	struct delayed_work polling_log;
	struct delayed_work voltage_step_work;
	int         target_voltage;
	int         capacity;
	int         temperature;
	/* shadow status */
	bool        status_onpad;       // opposite to gpio_detached
	/* onpad flags */
	enum p9222_opmode    opmode_type;       // EPP or BPP
	bool        opmode_epp;                 // 10W,12W or 5W
	bool        enabled;
	bool        detached;
	bool        vrect_valid;
	bool        wless_attached;
	bool        overheat;
	bool        max_power_15w;
	/* for controling GPIOs */
	int         gpio_stat;          // DIR_IN(interrupt)
	int         gpio_detached;      // DIR_IN(interrupt)
	int         gpio_vrect;         // DIR_IN(interrupt)
	int         gpio_wpc;           // DIR_OUT(command) vbus switch
	int         gpio_enabled;       // DIR_OUT(command)
	/* FOD parameters */
	struct p9222_fod *fod_bpp;
	struct p9222_fod *fod_epp;
	int         size_fodbpp;
	int         size_fodepp;
	bool        configure_sysfs;    // making sysfs or not (for debug)
};

static int p9222_regs [] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x1E, 0x1F, // identification
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3F,       // status and interrupt
	0x4E, 0x4F,     //battery and power transfer
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, // operation parameters
	0x60, 0xB4, 0xB5, 0x105, 0x106,
	0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, // fod
	0xD2, // Q factor
	0x62, 0x63, 0x64, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0xA1, // command and communication
	0x7E, 0x7F, // HW control and monitor
};

static inline const char* p9222_modename(enum p9222_opmode modetype) {
	switch (modetype) {
		case AC_MISSING :
			return "AC_MISSING";
		case WPC_BPP :
			return "WPC_BPP";
		case WPC_EPP :
			return "WPC_EPP";
		case UNKNOWN :
		default :
			return "UNKNOWN";
	}
}

static int p9222_debug = IDT_ASSERT | IDT_ERROR | IDT_INTERRUPT | IDT_MONITOR | IDT_REGISTER | IDT_RETURN | IDT_UPDATE;

static bool p9222_is_onpad(struct p9222_struct* p9222);
static int p9222_set_voltage(struct p9222_struct* p9222, u32 uV);
static ssize_t sysfs_i2c_show(struct device* dev,
		struct device_attribute* attr, char* buffer);
static ssize_t sysfs_i2c_store(struct device* dev,
		struct device_attribute* attr, const char* buf, size_t size);

static DEVICE_ATTR(register, S_IWUSR|S_IRUGO, sysfs_i2c_show, sysfs_i2c_store);

static struct attribute* p9222_sysfs_attrs [] = {
	&dev_attr_register.attr,
	NULL
};

static const struct attribute_group p9222_sysfs_files = {
	.attrs  = p9222_sysfs_attrs,
};

#define I2C_RETRY_COUNT 5
#define I2C_RETRY_DELAY 10
static inline bool p9222_reg_check(struct p9222_struct* p9222) {
	u8 address [] = {
		REG_ADDR_FIRMWARE >> 8,
		REG_ADDR_FIRMWARE & 0xff
	};

	u8 value = 0;
	bool success = false;

	struct i2c_msg message [] = {
		{	.addr   = p9222->i2c->addr,
			.flags  = 0,
			.buf    = address,
			.len    = 2
		},
		{	.addr   = p9222->i2c->addr,
			.flags  = I2C_M_RD,
			.buf    = &value,
			.len    = 1
		}
	};

	if (!p9222->overheat
		&& p9222->enabled
		&& i2c_transfer(p9222->i2c->adapter, message, 2) == 2)
		success = true;

	pr_idt(IDT_VERBOSE, "I2C check is %s\n", success ? "successed" : "failed");

	return success;
}

static inline bool p9222_reg_read(struct i2c_client* client, u16 reg, u8* value) {
	u8 address [] = {
		reg >> 8,
		reg & 0xff
	};

	struct i2c_msg message [] = {
		{	.addr   = client->addr,
			.flags  = 0,
			.buf    = address,
			.len    = 2
		},
		{	.addr   = client->addr,
			.flags  = I2C_M_RD,
			.buf    = value,
			.len    = 1
		}
	};

	bool success = false;
	int retry = I2C_RETRY_COUNT;

	while (--retry && !success) {
		if (i2c_transfer(client->adapter, message, 2) == 2) {
			pr_idt(IDT_VERBOSE, "I2C read : %d tried\n", I2C_RETRY_COUNT-retry);
			success = true;
		}
		else {
			mdelay(I2C_RETRY_DELAY);
		}
	}

	if(!success)
		pr_idt(IDT_ERROR, "I2C failed to read 0x%02x\n", reg);

	return success;
}

static inline bool p9222_reg_write(struct i2c_client* client, u16 reg, u8 val) {
	u8 address [] = {
		reg >> 8,
		reg & 0xff,
		val
	};

	struct i2c_msg message = {
		.addr   = client->addr,
		.flags  = 0,
		.buf    = address,
		.len    = 3
	};

	bool success = false;
	int retry = I2C_RETRY_COUNT;

	while (--retry && !success) {
		if (i2c_transfer(client->adapter, &message, 1) == 1) {
			pr_idt(IDT_VERBOSE, "I2C write : %d tried\n", I2C_RETRY_COUNT-retry);
			success = true;
		}
		else {
			mdelay(I2C_RETRY_DELAY);
		}
	}

	if(!success)
		pr_idt(IDT_ERROR, "I2C failed to write 0x%02x:0x%02x\n", reg, val);

	return success;
}

static void p9222_reg_dump(struct p9222_struct* p9222) {
	if (p9222_is_onpad(p9222)) {
		u8 val;
		int i;

		for (i=0; i<sizeof(p9222_regs)/sizeof(p9222_regs[0]); i++) {
			val = -1;
			p9222_reg_read(p9222->i2c, p9222_regs[i], &val);
			pr_idt(IDT_REGISTER, "%02x : %02x\n", p9222_regs[i], val);
		}
	}
	else
		pr_idt(IDT_VERBOSE, "p9222 is off\n");
}

static unsigned int sysfs_i2c_register = -1;
static ssize_t sysfs_i2c_store(struct device* dev,
		struct device_attribute* attr, const char* buf, size_t size) {
	struct p9222_struct* p9222 = dev->platform_data;

	u8 value = -1;
	if (sscanf(buf, "0x%04x-0x%02x", &sysfs_i2c_register, (unsigned int*)&value) == 2) {
		if (p9222_reg_write(p9222->i2c, sysfs_i2c_register, value))
			pr_idt(IDT_ERROR, "I2C write fail for 0x%04x\n", sysfs_i2c_register);
	}
	else if (sscanf(buf, "0x%04x", &sysfs_i2c_register) == 1) {
		pr_idt(IDT_ERROR, "I2C address 0x%04x is stored\n", sysfs_i2c_register);
	}
	else {
		pr_idt(IDT_ERROR, "Usage : echo 0x%%04x-0x%%02x\n > register");
	}

	return size;
}
static ssize_t sysfs_i2c_show(struct device* dev,
		struct device_attribute* attr, char* buffer) {
	struct p9222_struct* p9222 = dev->platform_data;

	u8 value = -1;
	if (sysfs_i2c_register != -1) {
		if (p9222_reg_read(p9222->i2c, sysfs_i2c_register, &value))
			return snprintf(buffer, PAGE_SIZE, "0x%03x", value);
		else
			return snprintf(buffer, PAGE_SIZE, "I2C read fail for 0x%04x\n", sysfs_i2c_register);
	}
	else
		return snprintf(buffer, PAGE_SIZE, "Address should be set befor reading\n");
}

static bool p9222_wakelock_acquire(struct wakeup_source* wakelock) {
	if (!wakelock->active) {
		pr_idt(IDT_INTERRUPT, "Success!\n");
		__pm_stay_awake(wakelock);

		return true;
	}
	return false;
}

static bool p9222_wakelock_release(struct wakeup_source* wakelock) {
	if (wakelock->active) {
		pr_idt(IDT_INTERRUPT, "Success!\n");
		__pm_relax(wakelock);

		return true;
	}
	return false;
}

static bool p9222_set_fod(struct p9222_struct* p9222) {
	const struct p9222_fod* parameters = p9222->opmode_epp
		? p9222->fod_epp : p9222->fod_bpp;
	const int size = p9222->opmode_epp
		? p9222->size_fodepp : p9222->size_fodbpp;
	u16 i;

	if (!size) {
		pr_idt(IDT_VERBOSE, "Skip to set %s fod (do not need)\n",
			p9222->opmode_epp ? "EPP" : "BPP");
		return true;
	}

	for (i = 0; i < size; ++i)
		p9222_reg_write(p9222->i2c,
				parameters[i].addr, parameters[i].value);

	// Returning for further purpose
	return true;
}

#define CONF_MIN_VOL_RANGE 3500000
static int p9222_set_voltage(struct p9222_struct* p9222, u32 uV)
{
	u8 value;

	p9222_reg_write(p9222->i2c, REG_ADDR_VOUT, (uV - CONF_MIN_VOL_RANGE)/100000);
	p9222_reg_read(p9222->i2c, REG_ADDR_VOUT, &value);
	pr_idt(IDT_REGISTER, "set voltage %duV(0x%02x)\n", uV, value);

	return 0;
}

static bool p9222_is_onpad(struct p9222_struct* p9222) {
	/* Refer to shadow here,
	 * And be sure that real GPIO may indicate diffrent value of onpad.
	 */
	return p9222->status_onpad;
}

static int p9222_get_opmode(struct p9222_struct* p9222)
{
	u8 value = 0;

	// Update system's operating mode {EPP or BPP}
	p9222_reg_read(p9222->i2c, REG_ADDR_OPMODE, &value);

	return (value & OPMODE_MASK) >> OPMODE_SHIFT;
}

static bool p9222_set_onpad(struct p9222_struct* p9222, bool onpad) {
	if (onpad) {
		u8 value = 0;

		p9222->opmode_type = p9222_get_opmode(p9222);
		if (p9222->opmode_type == OPMODE_WPC_EPP)
			p9222->opmode_epp = true;
		pr_idt(IDT_REGISTER, "opmode_type = 0x%02x = %s%s\n", value,
			p9222_modename(p9222->opmode_type),
			p9222->opmode_type == UNKNOWN ? "" :
			(p9222->opmode_epp ? "(12W)" : "(5W)"));
		p9222->overheat = false;

		// Start others
		schedule_delayed_work(&p9222->onpad_work, 0);
	}
	else {
		/*!!!!!!!!!!!! need to check!!!!!!!!!!!!!!!*/
		/* Off pad conditions
		 * 1: p9222->gpio_detached HIGH (means device is far from pad) or
		 * 2: p9222->gpio_enabled HIGH (means USB inserted) or
		 * 3: p9222->gpio_vrect HIGH (means vout is disabled) or
		 * 4: wireless_attached is false
		 */
		pr_assert(!p9222->wless_attached
			|| !!gpio_get_value(p9222->gpio_detached)
			|| !!gpio_get_value(p9222->gpio_enabled)
			|| !!gpio_get_value(p9222->gpio_vrect));

		p9222->opmode_type = UNKNOWN;
		p9222->opmode_epp = false;
		p9222->max_power_15w = false;
		cancel_delayed_work(&p9222->onpad_work);
		cancel_delayed_work(&p9222->voltage_step_work);
	}

	if (p9222->status_onpad != onpad) {
		p9222->status_onpad = onpad;

		pr_idt(IDT_UPDATE, "%s onpad %d\n", P9222_NAME_DRIVER,
			p9222_is_onpad(p9222));
		return true;
	}
	else
		return false;
}

static inline void p9222_psy_changed(struct p9222_struct* p9222) {
	union power_supply_propval propval;
	int ret = 0;

	pr_idt(IDT_VERBOSE, "wless_attached = %d\n", p9222->wless_attached);
	if (!p9222->psy)
		p9222->psy = power_supply_get_by_name("charger");
	if (!p9222->psy) {
		pr_idt(IDT_ERROR, "get chg psy fail\n");
		return;
	}

	if (!p9222->wless_attached) {
		ret = power_supply_get_property(p9222->psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE,
			&propval);
		if (ret < 0)
			pr_idt(IDT_ERROR, "get chg psy fail(%d)\n", ret);

		if (propval.intval != WIRELESS_CHARGER) {
			pr_idt(IDT_ERROR, "current charger type is %d, skip inform\n",
				propval.intval);
			p9222_set_onpad(p9222, false);
			power_supply_put(p9222->psy);
			return;
		}
	}

	propval.intval = p9222->wless_attached;
	ret = power_supply_set_property(p9222->psy, POWER_SUPPLY_PROP_ONLINE,
			&propval);
	if (ret < 0)
		pr_idt(IDT_ERROR, "psy online fail(%d)\n", ret);

	propval.intval = (p9222->wless_attached) ? WIRELESS_CHARGER :
		CHARGER_UNKNOWN;
	ret = power_supply_set_property(p9222->psy, POWER_SUPPLY_PROP_CHARGE_TYPE,
		&propval);
	if (ret < 0)
		pr_idt(IDT_ERROR, "psy type fail(%d)\n", ret);
	power_supply_put(p9222->psy);
}

/*
 * P9222 Work structs
 */
#define MAX_DETACH_TRY_CNT  5
static void p9222_wless_det_work(struct work_struct* work) {
	struct p9222_struct* p9222 = container_of(work,
				struct p9222_struct, wless_det_work.work);
	static int detach_try_cnt = 0;

	pr_idt(IDT_UPDATE, "detached = %d, vrect_valid = %d, enabled = %d, wless_attached = %d, overheat = %d\n",
		p9222->detached, p9222->vrect_valid, p9222->enabled, p9222->wless_attached, p9222->overheat);

	if (!p9222->vrect_valid && p9222->wless_attached) {
		if (p9222->detached) {
			pr_idt(IDT_UPDATE, "tx pad detached\n");
		} else if (!p9222->detached && p9222->overheat) {
			pr_idt(IDT_UPDATE, "overheat status.... keep current status...\n");
			return;
		} else if (!p9222->detached && (detach_try_cnt < MAX_DETACH_TRY_CNT) && !p9222->overheat) {
			schedule_delayed_work(&p9222->wless_det_work, msecs_to_jiffies(1000));
			detach_try_cnt++;
			pr_idt(IDT_UPDATE, "vrect is unstable... recheck wireless status for %d/5 sec... \n", detach_try_cnt);
			return;
		}
		pr_idt(IDT_UPDATE, "wless detached\n");
		p9222->wless_attached = false;
//		gpiod_set_value(gpio_to_desc(p9222->gpio_wpc), p9222->wless_attached);
		p9222_psy_changed(p9222);
		p9222_set_onpad(p9222, false);
		detach_try_cnt = 0;
		if (delayed_work_pending(&p9222->wless_det_work))
			cancel_delayed_work(&p9222->wless_det_work);
		p9222_wakelock_release(&p9222->wless_wakelock);
	} else if (p9222->vrect_valid && !p9222->wless_attached && p9222->enabled) {
		p9222_wakelock_acquire(&p9222->wless_wakelock);
		pr_idt(IDT_UPDATE, "wless attached\n");
		p9222->wless_attached = true;
		/* Control NCP3902 */
//		gpiod_set_value(gpio_to_desc(p9222->gpio_wpc), p9222->wless_attached);
		p9222_psy_changed(p9222);
		p9222_set_onpad(p9222, true);
	}
}

static void p9222_onpad_work(struct work_struct* work) {
	struct p9222_struct* p9222 = container_of(work,
			struct p9222_struct, onpad_work.work);
	u8 value = -1;

	p9222_reg_read(p9222->i2c, REG_ADDR_STATUS_L, &value);
	value = value >> STAT_VOUT_SHIFT;
	pr_idt(IDT_REGISTER, "REG_ADDR_STATUS_L STAT_VOUT: 0x%02x\n", value);
	if (!value) {
		schedule_delayed_work(&p9222->onpad_work, msecs_to_jiffies(1000));
		return;
	}

	// 1. Check firmware version (may be >= 0x12)
	p9222_reg_read(p9222->i2c, REG_ADDR_FIRMWARE, &value);
	pr_idt(IDT_REGISTER, "REG_ADDR_FIRMWARE : 0x%02x\n", value);

	// 2. Set Foreign Object Register
	p9222_set_fod(p9222);

	// 3. Check TxID and Finalized information
	p9222_reg_read(p9222->i2c, REG_ADDR_TXID, &value);
	pr_idt(IDT_REGISTER, "Finally, TX id = 0x%02x = %s%s\n", value,
			p9222_modename(p9222->opmode_type),
			p9222->opmode_type == UNKNOWN ? "" :
			(p9222->opmode_epp ? "(12W)" : "(5W)"));
	if (p9222->opmode_epp) {
		p9222_reg_read(p9222->i2c, REG_ADDR_SPECREV, &value);
		pr_idt(IDT_REGISTER, "TX SPEC REVISION = 0x%02x\n", value);
		p9222_reg_read(p9222->i2c, REG_ADDR_GRTPWR, &value);
		pr_idt(IDT_REGISTER, "TX Guaranteed Power = 0x%02x\n", value);
		if (value == POWER_15W)
			p9222->max_power_15w = true;
		p9222_reg_read(p9222->i2c, REG_ADDR_POTPWR, &value);
		pr_idt(IDT_REGISTER, "TX Potential Power = 0x%02x\n", value);
	}
}

static void p9222_voltage_step_work(struct work_struct* work) {
	struct p9222_struct* p9222 = container_of(work,
			struct p9222_struct, voltage_step_work.work);

	p9222_set_voltage(p9222, p9222->target_voltage);
	cancel_delayed_work(&p9222->voltage_step_work);
}

static void p9222_polling_adc(struct work_struct* work) {
	struct p9222_struct* p9222 = container_of(work,
			struct p9222_struct, polling_adc.work);
	int vrect = 0;
	int vadc = 0;
	int iadc = 0;
	u8 vrect_m, vrect_l;
	u8 vadc_m, vadc_l;
	u8 iadc_m, iadc_l;

	if (p9222_reg_check(p9222)) {
		p9222_reg_read(p9222->i2c, REG_ADDR_VRECT_M, &vrect_m);
		p9222_reg_read(p9222->i2c, REG_ADDR_VRECT_L, &vrect_l);
		vrect = vrect_m << 8 | vrect_l;
		pr_idt(IDT_UPDATE, "Vrect : %dmV\n", vrect);

		p9222_reg_read(p9222->i2c, REG_ADDR_VADC_M, &vadc_m);
		p9222_reg_read(p9222->i2c, REG_ADDR_VADC_L, &vadc_l);
		vadc = vadc_m << 8 | vadc_l;
		pr_idt(IDT_UPDATE, "Vadc : %dmV\n", vadc);

		p9222_reg_read(p9222->i2c, REG_ADDR_IADC_M, &iadc_m);
		p9222_reg_read(p9222->i2c, REG_ADDR_IADC_L, &iadc_l);
		iadc = iadc_m << 8 | iadc_l;
		pr_idt(IDT_UPDATE, "Iadc : %dmA\n", iadc);
	}

	if (!gpio_get_value(p9222->gpio_detached))
		schedule_delayed_work(&p9222->polling_adc,
			round_jiffies_relative(msecs_to_jiffies(1000)));
}

static void p9222_polling_log(struct work_struct* work) {
	struct p9222_struct* p9222 = container_of(work,
			struct p9222_struct, polling_log.work);

	// Monitor GPIOs
	int stat = gpio_get_value(p9222->gpio_stat);
	int detached = gpio_get_value(p9222->gpio_detached);
	int vrect = !gpio_get_value(p9222->gpio_vrect);
	int wpc = gpio_get_value(p9222->gpio_wpc);
	int enabled = !gpio_get_value(p9222->gpio_enabled);
	pr_err("AP_GPIO%d(<-STAT_N, stat):%d, "
			"AP_GPIO%d(<-PDT_N, detached):%d, "
			"AP_GPIO%d(<-VRECT_N, vrect):%d, "
			"AP_GPIO%d(->NCP_EN_N, wpc):%d, "
			"AP_GPIO%d(->EN_N, enabled):%d\n",
			p9222->gpio_stat, stat,
			p9222->gpio_detached, detached,
			p9222->gpio_vrect, !vrect,
			p9222->gpio_wpc, wpc,
			p9222->gpio_enabled, !enabled);

	if (false) {
		/* for debugging */
		p9222_reg_dump(p9222);
	}

	schedule_delayed_work(&p9222->polling_log, round_jiffies_relative
		(msecs_to_jiffies(1000*30)));
}

/*
 *  P9222 Charger ops
 */
static int p9222_enable(struct charger_device *chg_dev, bool enabled) {
	struct p9222_struct* p9222 = charger_get_data(chg_dev);
	int ret = 0;
	u8 txid = -1;

	if (p9222_is_onpad(p9222)
			&& !enabled
			&& p9222_reg_read(p9222->i2c, REG_ADDR_TXID, &txid)
			&& txid == 0x63/*innotek tx pad W/A*/) {
		pr_idt(IDT_MONITOR, "Send EPT_BY_NORESPONSE for normal stop\n");
		p9222_reg_write(p9222->i2c, REG_ADDR_EPT, EPT_BY_NORESPONSE);
		p9222_reg_write(p9222->i2c, REG_ADDR_COM_L, SEND_EPT);
	}

	pr_idt(IDT_VERBOSE, "p9222_enable = %d\n", enabled);

	if (enabled)
		p9222->enabled = enabled;

	gpiod_set_value(gpio_to_desc(p9222->gpio_enabled), !enabled);

	if (!enabled) {
#if 0
		ret = wait_event_timeout(p9222->wait_queue, p9222->gpio_vrect,
				msecs_to_jiffies(200));
		if (ret < 0) {
			pr_idt(IDT_ERROR, "%s: wait GPIO0 pull-high time out", __func__);
			ret = -EIO;
		}
#endif
		p9222->enabled = enabled;
//		p9222_set_onpad(p9222, false);
	}

	msleep(20);

	pr_assert(!gpio_get_value(p9222->gpio_enabled)==enabled);
	pr_idt(IDT_INTERRUPT, "p9222_enabled is written %s(%d)\n",
		(!gpio_get_value(p9222->gpio_enabled) == enabled)
		? "success!" : "fail!", enabled);
	return ret;
}

static int p9222_enable_power_path(struct charger_device *chg_dev, bool enabled) {
	struct p9222_struct* p9222 = charger_get_data(chg_dev);

	pr_idt(IDT_VERBOSE, "p9222_enable_power_path = %d\n", enabled);
	/* Control NCP3902 */
	gpiod_set_value(gpio_to_desc(p9222->gpio_wpc), !enabled);

	pr_idt(IDT_INTERRUPT, "p9222_enabled is written %s(%d)\n",
			(!gpio_get_value(p9222->gpio_wpc) == enabled)
			? "success!" : "fail!", enabled);
	return 0;
}

#define EPP_STEP_VOLTAGE 9000000
#define EPP_STEP_DELAY 5000
static int p9222_set_constant_voltage(struct charger_device *chg_dev, u32 uV) {
	struct p9222_struct* p9222 = charger_get_data(chg_dev);

	if (uV < 0)
		return 0;

	if (p9222_is_onpad(p9222)) {
		if(delayed_work_pending(&p9222->voltage_step_work))
			cancel_delayed_work(&p9222->voltage_step_work);

		if (uV <= EPP_STEP_VOLTAGE) {
			p9222_set_voltage(p9222, uV);
		} else {
			p9222->target_voltage = uV;

			p9222_set_voltage(p9222, EPP_STEP_VOLTAGE);
			schedule_delayed_work(&p9222->voltage_step_work,
					msecs_to_jiffies(EPP_STEP_DELAY));
		}
	} else
		return -ENOTSUPP;

	return 0;
}

static int p9222_get_constant_voltage(struct charger_device *chg_dev, u32 *uV) {
	struct p9222_struct* p9222 = charger_get_data(chg_dev);

	if (*uV < 0)
		return 0;

	if (p9222_is_onpad(p9222)) {
		u8 value = -1;

		p9222_reg_read(p9222->i2c, REG_ADDR_VOUT, &value);
		pr_idt(IDT_REGISTER, "get voltage %duV(0x%02x)\n", *uV, value);
		*uV = value;
	}

	return 0;
}

static int p9222_get_vbus_adc(struct charger_device *chg_dev, u32 *vbus) {
	struct p9222_struct* p9222 = charger_get_data(chg_dev);

	u8 vadc_m, vadc_l;

	if (!p9222_reg_check(p9222))
		return 0;

	p9222_reg_read(p9222->i2c, REG_ADDR_VADC_M, &vadc_m);
	p9222_reg_read(p9222->i2c, REG_ADDR_VADC_L, &vadc_l);

	*vbus = vadc_m << 8 | vadc_l;

	return 0;
}

static int p9222_get_guaranteed_power(struct charger_device *chg_dev, enum epp_pwr *pwr) {
	struct p9222_struct* p9222 = charger_get_data(chg_dev);

	if (p9222_is_onpad(p9222)) {
		if (p9222->opmode_epp) {
			if (p9222->max_power_15w)
				*pwr = WLESS_EPP_PWR_15W;
			else
				*pwr = WLESS_EPP_PWR_10W;
			pr_idt(IDT_VERBOSE, "WLESS_EPP_PWR = %d\n", *pwr);
		}
	}

	return 0;
}

static int p9222_get_status(struct charger_device *chg_dev, enum chg_stat status, u32 *val) {
	struct p9222_struct* p9222 = charger_get_data(chg_dev);

	if (status >= CHG_STAT_MAX) {
		pr_idt(IDT_ERROR, "status(%d) >= max(%d)\n", status, CHG_STAT_MAX);
		return -EINVAL;
	}

	switch (status) {
		case WLESS_CHG_STAT_EPP:
			if (p9222_get_opmode(p9222) == OPMODE_WPC_EPP)
				*val = true;
			else
				*val = false;
			pr_idt(IDT_VERBOSE, "STAT_EPP = %d\n", *val);
			break;
		case WLESS_CHG_STAT_LP_EPP:
			*val = false;
			pr_idt(IDT_VERBOSE, "STAT_LP_EPP = %d\n", *val);
			break;
		case WLESS_CHG_STAT_EPT:
			*val = p9222->overheat;
			break;
		default:
			pr_idt(IDT_ERROR, "no such status(%d) defined\n", status);
			return -ENOTSUPP;

	}
	return 0;
}

static int p9222_set_ept_status(struct charger_device *chg_dev, enum ept_stat status) {
	struct p9222_struct* p9222 = charger_get_data(chg_dev);

	if (status >= EPT_STAT_MAX) {
		pr_idt(IDT_ERROR, "status(%d) >= max(%d)\n", status, EPT_STAT_MAX);
		return -EINVAL;
	}

	if (!p9222_is_onpad(p9222) || p9222->overheat)
		return -1;

	switch (status) {
		case WLESS_EPT_UNKNOWN:
		case WLESS_EPT_EOC:
		case WLESS_EPT_INTERNAL_FAULT:
			pr_idt(IDT_VERBOSE, "nothing to do currently\n");
			break;
		case WLESS_EPT_OVER_TEMPERATURE:
			if (p9222_reg_write(p9222->i2c, REG_ADDR_EPT, EPT_BY_OVERTEMP) &&
				p9222_reg_write(p9222->i2c, REG_ADDR_COM_L, SEND_EPT)) {
				p9222->overheat = true;
				pr_idt(IDT_VERBOSE, "The device is overheat, Send EPT_BY_OVERTEMP\n");
			} else {
				pr_idt(IDT_ERROR, "Failed to turning off by EPT_BY_OVERTEMP\n");
			}
			break;
		case WLESS_EPT_OVER_VOLTAGE:
		case WLESS_EPT_OVER_CURRENT:
		case WLESS_EPT_BATTERY_FAIL:
		case WLESS_EPT_RECONFIGURATION:
		case WLESS_EPT_NO_RESPONSE:
			pr_idt(IDT_VERBOSE, "nothing to do currently\n");
			break;
		default:
			pr_idt(IDT_ERROR, "no such status(%d) defined\n", status);
			return -ENOTSUPP;
	}
	return 0;
}

#define FULL_CAPACITY   100
static int p9222_update_capacity(struct charger_device *chg_dev, u32 capacity) {
	struct p9222_struct* p9222 = charger_get_data(chg_dev);

	if (capacity < 0 || capacity > FULL_CAPACITY)
		return 0;

	if (p9222->capacity == capacity)
		return 0;

	p9222->capacity = capacity;

	if (p9222_is_onpad(p9222)) {
		if (p9222->capacity <= FULL_CAPACITY && p9222->capacity >= 0) {
			if ((p9222->opmode_type == WPC_BPP || p9222->opmode_type == WPC_EPP)
				&& p9222_reg_check(p9222)) {
				pr_idt(IDT_UPDATE, "capacity = %d\n", p9222->capacity);
				p9222_reg_write(p9222->i2c, REG_ADDR_CHGSTAT, p9222->capacity);
				p9222_reg_write(p9222->i2c, REG_ADDR_COM_L, SEND_CHGSTAT);
			}
		}
	}
	return 0;
}

static int p9222_dump_registers(struct charger_device *chg_dev) {
    struct p9222_struct* p9222 = charger_get_data(chg_dev);

	p9222_reg_dump(p9222);

	return 0;
}

/*
 * P9222 Interrupt Handler
 */

static irqreturn_t p9222_stat_handle_irq(int irq, void* data) {
	/* This ISR will be triggered on below unrecoverable exceptions :
	 * Over temperature, Over current, or Over voltage detected by IDTP922X chip.
	 * P9222 turns off after notifying it to host, so there's nothing to handle
	 * except logging here.
	 */
	struct p9222_struct* p9222 = data;
	u8 value_l;
	u8 value_h;

	if (!p9222->enabled || p9222->overheat)
		return IRQ_HANDLED;

	if (p9222_is_onpad(p9222)) {
		p9222_reg_read(p9222->i2c, REG_ADDR_INT_L, &value_l);
		p9222_reg_read(p9222->i2c, REG_ADDR_INT_H, &value_h);
#if 0
		if (value_h & EXTENDED_MODE)
			pr_idt(IDT_INTERRUPT, "p9222_stat_handle_irq is triggered : EPP Calibration Mode 2 end\n");
#endif
	}

	pr_idt(IDT_INTERRUPT, "p9222_stat_handle_irq is triggered : %d(0x%02x 0x%02x)\n",
		!gpio_get_value(p9222->gpio_stat), value_l, value_h);

	return IRQ_HANDLED;
}

static irqreturn_t p9222_detach_handle_irq(int irq, void* data) {
	struct p9222_struct* p9222 = data;

	p9222->detached = gpio_get_value(p9222->gpio_detached);

	if (!p9222->enabled)
		return IRQ_HANDLED;

	pr_idt(IDT_INTERRUPT, "p9222_detach_handle_irq is triggered : %d\n",
			p9222->detached);

	if (p9222->detached) {
		if(delayed_work_pending(&p9222->wless_det_work))
			cancel_delayed_work(&p9222->wless_det_work);
		schedule_delayed_work(&p9222->wless_det_work, 0);
	}

	return IRQ_HANDLED;
}

static void p9222_restart_power_transfer(struct p9222_struct *p9222)
{
	pr_idt(IDT_REGISTER, "p9222_restart_power_transfer\n");
	p9222_reg_write(p9222->i2c, REG_ADDR_EPT, EPT_BY_RESTART_POWER_TRANSFER);
	p9222_reg_write(p9222->i2c, REG_ADDR_COM_L, SEND_EPT);
}

#define FOD_REGISTER_SIZE   12
#define TRANSFER_EPP_POWER   10000000
static void p9222_transfer_epp_power(struct p9222_struct *p9222)
{
	const struct p9222_fod parameters[FOD_REGISTER_SIZE] = {{0x84, 0xC0}, {0x85, 0x00},
					{0x86, 0x80}, {0x87, 0x00},
					{0x88, 0x80}, {0x89, 0x00},
					{0x8A, 0x80}, {0x8B, 0x00},
					{0x8C, 0x80}, {0x8D, 0x00},
					{0x8E, 0x80}, {0x8F, 0x00}};
	u16 i;
	u8 value = -1;

	p9222_reg_write(p9222->i2c, REG_ADDR_MPREQNP, POWER_10W);
	p9222_reg_write(p9222->i2c, REG_ADDR_MPREQMP, POWER_10W);
	p9222_reg_write(p9222->i2c, REG_ADDR_VOUT, (TRANSFER_EPP_POWER - CONF_MIN_VOL_RANGE)/100000);
	p9222_reg_write(p9222->i2c, REG_ADDR_MPVRCALM1_L, 0x00);
	p9222_reg_write(p9222->i2c, REG_ADDR_MPVRCALM1_H, 0x08);
	for (i = 0; i < FOD_REGISTER_SIZE; ++i)
		p9222_reg_write(p9222->i2c,
			parameters[i].addr, parameters[i].value);

	p9222_reg_read(p9222->i2c, REG_ADDR_MPREQNP, &value);
	pr_idt(IDT_REGISTER, "reqnp 0x%02x", value);
	p9222_reg_read(p9222->i2c, REG_ADDR_MPREQMP, &value);
	pr_idt(IDT_REGISTER, "reqmp 0x%02x", value);
	p9222_reg_read(p9222->i2c, REG_ADDR_VOUT, &value);
	pr_idt(IDT_REGISTER, "set voltage (0x%02x)\n", value);
	p9222_reg_read(p9222->i2c, REG_ADDR_MPVRCALM1_L, &value);
	pr_idt(IDT_REGISTER, "calm1_l 0x%02x", value);
	p9222_reg_read(p9222->i2c, REG_ADDR_MPVRCALM1_H, &value);
	pr_idt(IDT_REGISTER, "calm1_h 0x%02x", value);
}

#define INITIAL_BOOT_DELAY    5000
static void __p9222_vrect_handle_irq(struct p9222_struct *p9222, unsigned int delay) {
	p9222->vrect_valid = !gpio_get_value(p9222->gpio_vrect);

	if (!p9222->enabled)
		return;

	pr_idt(IDT_INTERRUPT, "p9222_vrect_handle_irq is triggered : %d\n",
		p9222->vrect_valid);

	if (p9222->vrect_valid && p9222->overheat)
		p9222->overheat = false;

	delay = p9222->vrect_valid ? delay : 0;

	if (p9222->vrect_valid) {
		mdelay(100); //to read epp type

		if (p9222_get_opmode(p9222) == OPMODE_WPC_EPP) {
			if (delay == INITIAL_BOOT_DELAY && (!p9222->wless_attached && p9222->enabled)) {
				pr_idt(IDT_INTERRUPT, "initial boot\n");
				p9222_restart_power_transfer(p9222);
				return;
			}

			p9222_transfer_epp_power(p9222);
		}
	}

	schedule_delayed_work(&p9222->wless_det_work, msecs_to_jiffies(delay));

	if (false) { // for debugging
		if (p9222->vrect_valid)
			schedule_delayed_work(&p9222->polling_adc,
				round_jiffies_relative(msecs_to_jiffies(1000)));
	}
}

static irqreturn_t p9222_vrect_handle_irq(int irq, void *data) {
	__p9222_vrect_handle_irq(data, 100);
	return IRQ_HANDLED;
}

/*
 * P9222 Probes
 */

static bool p9222_probe_devicetree(struct device_node* dnode,
		struct p9222_struct* p9222) {
	const char* arr = NULL;
	int i, buf = -1;

	if (!dnode) {
		pr_idt(IDT_ERROR, "dnode is null\n");
		return false;
	}

	/* Parse GPIOs */
	p9222->gpio_stat = of_get_named_gpio(dnode, "idt,gpio-stat", 0);
	if (p9222->gpio_stat < 0) {
		pr_idt(IDT_ERROR, "Fail to get gpio-stat\n");
		return false;
	}

	p9222->gpio_detached = of_get_named_gpio(dnode, "idt,gpio-detached", 0);
	if (p9222->gpio_detached < 0) {
		pr_idt(IDT_ERROR, "Fail to get gpio-detached\n");
		return false;
	}

	p9222->gpio_vrect = of_get_named_gpio(dnode, "idt,gpio-vrect", 0);
	if (p9222->gpio_vrect < 0) {
		pr_idt(IDT_ERROR, "Fail to get gpio-vrect\n");
		return false;
	}

	p9222->gpio_wpc = of_get_named_gpio(dnode, "idt,gpio-wpc", 0);
	if (p9222->gpio_wpc < 0) {
		pr_idt(IDT_ERROR, "Fail to get gpio-wpc\n");
	}

	p9222->gpio_enabled = of_get_named_gpio(dnode, "idt,gpio-enabled", 0);
	if (p9222->gpio_enabled < 0) {
		pr_idt(IDT_ERROR, "Fail to get gpio-enabled\n");
		return false;
	}

	/* Parse FOD parameters */
	arr = of_get_property(dnode, "idt,fod-bpp", &buf);
	if (!arr) {
		pr_idt(IDT_VERBOSE, "Use default fod status - Basic\n");
		p9222->fod_bpp = NULL;
	}
	else {
		p9222->fod_bpp = (struct p9222_fod*)kmalloc(
				sizeof(struct p9222_fod) * (buf / 2), GFP_KERNEL);
		if (p9222->fod_bpp == NULL) {
			pr_idt(IDT_ERROR, "Fail to kmalloc fod_bpp space\n");
			return -ENOMEM;
		}

		memset(p9222->fod_bpp, 0, sizeof(*p9222->fod_bpp));

		for (i = 0; i < (buf / 2); i++) {
			p9222->fod_bpp[i].addr = arr[i * 2];
			p9222->fod_bpp[i].value = arr[i * 2 + 1];
		}
		p9222->size_fodbpp = buf / 2;
	}

	arr = of_get_property(dnode, "idt,fod-epp", &buf);
	if (!arr) {
		pr_idt(IDT_VERBOSE, "Use default fod status - Extended\n");
		p9222->fod_epp = NULL;
	}
	else {
		p9222->fod_epp = (struct p9222_fod*)kmalloc(
			sizeof(struct p9222_fod) * (buf / 2), GFP_KERNEL);
		if (p9222->fod_epp == NULL) {
			pr_idt(IDT_ERROR, "Fail to kmalloc fod_epp space\n");
			return -ENOMEM;
		}

		memset(p9222->fod_epp, 0, sizeof(*p9222->fod_epp));

		for (i = 0; i < (buf / 2); i++) {
			p9222->fod_epp[i].addr = arr[i * 2];
			p9222->fod_epp[i].value = arr[i * 2 + 1];
		}
		p9222->size_fodepp = buf / 2;
	}
	p9222->configure_sysfs = of_property_read_bool(dnode, "idt,configure-sysfs");
	return true;
}

static bool p9222_probe_gpios(struct p9222_struct* p9222) {
	int ret;
	// Set direction
	ret = gpio_request_one(p9222->gpio_stat, GPIOF_IN, "gpio_stat");
	if (ret < 0) {
		pr_idt(IDT_ERROR, "Fail to request gpio_stat %d\n", ret);
		return false;
	}

	ret = gpio_request_one(p9222->gpio_detached, GPIOF_IN, "gpio_detached");
	if (ret < 0) {
		pr_idt(IDT_ERROR, "Fail to request gpio_detached, %d\n", ret);
		return false;
	}

	ret = gpio_request_one(p9222->gpio_vrect, GPIOF_IN, "gpio_vrect");
	if (ret < 0) {
		pr_idt(IDT_ERROR, "Fail to request gpio_vrect, %d\n", ret);
		return false;
	}

	ret = gpio_request_one(p9222->gpio_wpc, GPIOF_DIR_OUT, "gpio_wpc");
	if (ret < 0) {
		pr_idt(IDT_ERROR, "Fail to request gpio_wpc %d\n", ret);
		return false;
	}

	ret = gpio_request_one(p9222->gpio_enabled, GPIOF_OUT_INIT_LOW, "gpio_enabled");
	if (ret < 0) {
		pr_idt(IDT_ERROR, "Fail to request gpio_enabled %d\n", ret);
		return false;
	}
	return true;
}

static bool p9222_probe_irqs(struct p9222_struct* p9222) {
	int ret = 0;

	/* GPIO IDTFault */
	ret = request_threaded_irq(gpio_to_irq(p9222->gpio_stat),
			NULL, p9222_stat_handle_irq, IRQF_ONESHOT|IRQF_TRIGGER_FALLING,
			"wlc-stat", p9222);
	if (ret) {
		pr_idt(IDT_ERROR, "Cannot request irq %d (%d)\n",
				gpio_to_irq(p9222->gpio_stat), ret);
		return false;
	}
	else
		enable_irq_wake(gpio_to_irq(p9222->gpio_stat));

	/* GPIO Detached */
	ret = request_threaded_irq(gpio_to_irq(p9222->gpio_detached),
			NULL, p9222_detach_handle_irq, IRQF_ONESHOT|IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,
			"wlc-detached", p9222);
	if (ret) {
		pr_idt(IDT_ERROR, "Cannot request irq %d (%d)\n",
				gpio_to_irq(p9222->gpio_detached), ret);
		return false;
	}
	else
		enable_irq_wake(gpio_to_irq(p9222->gpio_detached));

	/* GPIO Vrect */
	ret = request_threaded_irq(gpio_to_irq(p9222->gpio_vrect),
			NULL, p9222_vrect_handle_irq, IRQF_ONESHOT|IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,
			"wlc-vrect", p9222);
	if (ret) {
		pr_idt(IDT_ERROR, "Cannot request irq %d (%d)\n",
				gpio_to_irq(p9222->gpio_vrect), ret);
		return false;
	}
	else
		enable_irq_wake(gpio_to_irq(p9222->gpio_vrect));

	return true;
}

static int p9222_remove(struct i2c_client* client) {
	struct p9222_struct* p9222 = i2c_get_clientdata(client);
	pr_idt(IDT_VERBOSE, "p9222 is about to be removed from system\n");

	if (p9222) {
		/* Clear descripters */
		if (delayed_work_pending(&p9222->wless_det_work))
			cancel_delayed_work_sync(&p9222->wless_det_work);
		if (delayed_work_pending(&p9222->onpad_work))
			cancel_delayed_work_sync(&p9222->onpad_work);
		if (delayed_work_pending(&p9222->voltage_step_work))
			cancel_delayed_work_sync(&p9222->voltage_step_work);
		if (delayed_work_pending(&p9222->polling_adc))
			cancel_delayed_work(&p9222->polling_adc);
		if (delayed_work_pending(&p9222->polling_log))
			cancel_delayed_work_sync(&p9222->polling_log);
		/* Clear gpios */
		if (p9222->gpio_stat)
			gpio_free(p9222->gpio_stat);
		if (p9222->gpio_detached)
			gpio_free(p9222->gpio_detached);
		if (p9222->gpio_vrect)
			gpio_free(p9222->gpio_vrect);
		if (p9222->gpio_wpc)
			gpio_free(p9222->gpio_wpc);
		if (p9222->gpio_enabled)
			gpio_free(p9222->gpio_enabled);
		/* Finally, make me free */
		kfree(p9222);
		return 0;
	}
	else
		return -EINVAL;
}

static void p9222_shutdown(struct i2c_client* client) {
	struct p9222_struct* p9222 = i2c_get_clientdata(client);
	pr_idt(IDT_VERBOSE, "p9222 shutdown\n");

	if (p9222) {
		if (p9222->opmode_epp)
			p9222_restart_power_transfer(p9222);
	}
}

static struct charger_ops p9222_chg_ops = {
	.enable = p9222_enable,
	.enable_powerpath = p9222_enable_power_path,
	.get_constant_voltage = p9222_get_constant_voltage,
	.set_constant_voltage = p9222_set_constant_voltage,
	.get_vbus_adc = p9222_get_vbus_adc,
	.get_max_power = p9222_get_guaranteed_power,
	.get_status = p9222_get_status,
	.set_ept_status = p9222_set_ept_status,
	.update_capacity = p9222_update_capacity,
	.dump_registers = p9222_dump_registers,
};

static int p9222_probe(struct i2c_client* client, const struct i2c_device_id* id) {
	struct p9222_struct* p9222 = kzalloc(sizeof(struct p9222_struct), GFP_KERNEL);
	int ret;

	pr_idt(IDT_VERBOSE, "Start\n");

	if (!p9222) {
		pr_idt(IDT_ERROR, "Failed to alloc memory\n");
		goto error;
	}
	else {
		// Store the platform_data to drv_data
		i2c_set_clientdata(client, p9222);
	}

	// For client and device
	p9222->i2c = client;
	p9222->dev = &client->dev;
	p9222->dev->platform_data = p9222;

	p9222->enabled = true;
	p9222->status_onpad = false;
	p9222->wless_attached = false;
	p9222->detached = false;
	p9222->vrect_valid = true;
	p9222->max_power_15w = false;
	p9222->overheat = false;

	// For remained preset
	if (!p9222_probe_devicetree(p9222->dev->of_node, p9222)) {
		pr_idt(IDT_ERROR, "Fail to read parse_dt\n");
		goto error;
	}

	wakeup_source_init(&p9222->wless_wakelock, "P9222: wakelock");

	INIT_DELAYED_WORK(&p9222->wless_det_work, p9222_wless_det_work);
	INIT_DELAYED_WORK(&p9222->onpad_work, p9222_onpad_work);
	INIT_DELAYED_WORK(&p9222->voltage_step_work, p9222_voltage_step_work);
	INIT_DELAYED_WORK(&p9222->polling_adc, p9222_polling_adc);
	INIT_DELAYED_WORK(&p9222->polling_log, p9222_polling_log);

	schedule_delayed_work(&p9222->polling_log, 0);

	// For GPIOs
	if (!p9222_probe_gpios(p9222)) {
		pr_idt(IDT_ERROR, "Fail to request gpio at probe\n");
		goto error;
	}

	// Check wireless plug-in
	__p9222_vrect_handle_irq(p9222, INITIAL_BOOT_DELAY);

	// Request irqs
	if (!p9222_probe_irqs(p9222)) {
		pr_idt(IDT_ERROR, "Fail to request irqs at probe\n");
		goto error;
	}

	p9222->chg_props.alias_name = "p9222_chg";
	p9222->chg_dev = charger_device_register("wless_chg", p9222->dev, p9222,
			&p9222_chg_ops,
			&p9222->chg_props);
	if (IS_ERR_OR_NULL(p9222->chg_dev)) {
		ret = PTR_ERR(p9222->chg_dev);
		goto error;
	}

	// Create sysfs if it is configured
	if (p9222->configure_sysfs
			&& sysfs_create_group(&p9222->dev->kobj, &p9222_sysfs_files) < 0) {
		pr_idt(IDT_ERROR, "unable to create sysfs\n");
		goto error;
	}

	pr_idt(IDT_VERBOSE, "Complete probing P9222\n");
	return 0;

error:
	p9222_remove(client);
	return -EPROBE_DEFER;
}
//Compatible node must be matched to dts
static struct of_device_id  __maybe_unused p9222_of_id_table[] = {
	{ .compatible = P9222_NAME_COMPATIBLE, },
	{ },
};
MODULE_DEVICE_TABLE(of, p9222_of_id_table);

//I2C slave id supported by driver
static const struct i2c_device_id p9222_id_table [] = {
	{ P9222_NAME_DRIVER, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, p9222_id_table);

//I2C Driver Info
static struct i2c_driver p9222_driver = {
	.driver = {
		.name = P9222_NAME_DRIVER,
		.owner = THIS_MODULE,
		.of_match_table = p9222_of_id_table,
	},

	.probe = p9222_probe,
	.remove = p9222_remove,
	.shutdown = p9222_shutdown,
	.id_table = p9222_id_table,
};

//Easy wrapper to do driver init
module_i2c_driver(p9222_driver);

MODULE_DESCRIPTION(P9222_NAME_DRIVER);
MODULE_LICENSE("GPL v2");

