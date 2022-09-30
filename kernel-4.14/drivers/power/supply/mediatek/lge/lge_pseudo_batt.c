/*
 * lge_pseudo_batt.c
 *
 * Copyright (C) 2014 LG Electronics. Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "[PSEUDO_BATT] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/power/lge_pseudo_batt.h>
#include "lge_pseudo_power_supply.h"

#define PSEUDO_BATT_RW_PERM (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

struct pseudo_batt_info_type {
	int mode;	/* enable / disable */
	int id;		/* battery id */
	int therm;
	int temp;	/* degree */
	int volt;	/* micro volt */
	int capacity;	/* percent */
	int charging;
};

static struct pseudo_batt_info_type pseudo_batt = {
	.mode = 0,
	.id = 1,
	.therm = 100,
	.temp = 40,
	.volt = 4100000,
	.capacity = 80,
	.charging = 1,
};

static void pseudo_batt_notify(void)
{
	pseudo_power_supply_update("battery");

#ifdef CONFIG_LGE_PM_CHARGER_CONTROLLER
	chgctrl_set_pseudo_mode(PSEUDO_BATTERY, pseudo_batt.mode);
#endif
}


static bool pseudo_batt_changed(struct pseudo_batt_info_type info)
{
	if (pseudo_batt.mode != info.mode)
		return true;
	if (pseudo_batt.id != info.id)
		return true;
	if (pseudo_batt.therm != info.therm)
		return true;
	if (pseudo_batt.temp != info.temp)
		return true;
	if (pseudo_batt.volt != info.volt)
		return true;
	if (pseudo_batt.capacity != info.capacity)
		return true;
	if (pseudo_batt.charging != info.charging)
		return true;

	return false;
}

static int param_set_pseudo_batt(const char *val,
				 const struct kernel_param *kp)
{
	struct pseudo_batt_info_type info;
	int ret;

	ret = sscanf(val, "%d %d %d %d %d %d %d",
			&info.mode, &info.id,
			&info.therm, &info.temp,
			&info.volt, &info.capacity,
			&info.charging);
	if (ret != 7)
		return -EINVAL;

	if (!pseudo_batt_changed(info))
		return 0;

	pseudo_batt = info;

	pr_info("fake battery %s\n", pseudo_batt.mode ? "enabled" : "disabled");

	pseudo_batt_notify();

	return 0;
}

static int param_get_pseudo_batt(char *buffer,
				 const struct kernel_param *kp)
{
	return scnprintf(buffer, PAGE_SIZE, "%s",
			(pseudo_batt.mode ? "PSEUDO" : "NORMAL"));
}

static struct kernel_param_ops pseudo_batt_ops = {
	.set = param_set_pseudo_batt,
	.get = param_get_pseudo_batt,
};
module_param_cb(pseudo_batt, &pseudo_batt_ops, NULL, PSEUDO_BATT_RW_PERM);

static int param_set_pseudo_batt_mode(const char *val,
				      const struct kernel_param *kp)
{
	struct pseudo_batt_info_type info = pseudo_batt;
	int ret;

	ret = sscanf(val, "%d", &info.mode);
	if (ret != 1)
		return -EINVAL;

	if (!pseudo_batt_changed(info))
		return 0;

	pseudo_batt = info;

	pr_info("fake battery %s\n", pseudo_batt.mode ? "enabled" : "disabled");

	pseudo_batt_notify();

	return 0;
}
module_param_call(battery_mode, param_set_pseudo_batt_mode, param_get_int,
		&pseudo_batt.mode, PSEUDO_BATT_RW_PERM);
module_param_named(battery_id, pseudo_batt.id, int, PSEUDO_BATT_RW_PERM);
module_param_named(battery_therm, pseudo_batt.therm, int, PSEUDO_BATT_RW_PERM);
module_param_named(battery_temp, pseudo_batt.temp, int, PSEUDO_BATT_RW_PERM);
module_param_named(battery_volt, pseudo_batt.volt, int, PSEUDO_BATT_RW_PERM);
module_param_named(battery_capacity, pseudo_batt.capacity, int, PSEUDO_BATT_RW_PERM);
module_param_named(battery_charging, pseudo_batt.charging, int, PSEUDO_BATT_RW_PERM);

int get_pseudo_batt_info(int type)
{
	switch (type) {
	case PSEUDO_BATT_MODE:
		return pseudo_batt.mode;
	case PSEUDO_BATT_ID:
		return pseudo_batt.id;
	case PSEUDO_BATT_THERM:
		return pseudo_batt.therm;
	case PSEUDO_BATT_TEMP:
		return pseudo_batt.temp;
	case PSEUDO_BATT_VOLT:
		return pseudo_batt.volt;
	case PSEUDO_BATT_CAPACITY:
		return pseudo_batt.capacity;
	case PSEUDO_BATT_CHARGING:
		return pseudo_batt.charging;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(get_pseudo_batt_info);

void pseudo_batt_property_override(enum power_supply_property psp,
				   union power_supply_propval *val)
{
	if (!pseudo_batt.mode)
		return;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = pseudo_batt.capacity;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = pseudo_batt.volt;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = pseudo_batt.temp * 10;
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(pseudo_batt_property_override);

static int __init pseudo_batt_init(char *param)
{
	if (!strcmp(param, "enable")) {
		pr_info("enable fake battery by default\n");
		pseudo_batt.mode = 1;
	}

	return 1;
}
__setup("fakebattery=", pseudo_batt_init);
