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

#define pr_fmt(fmt) "[PSEUDO_HVDCP] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include "lge_pseudo_power_supply.h"

#define PSEUDO_HVDCP_RW_PERM (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

struct pseudo_hvdcp_info {
	int mode;
	int threshold;
};

static struct pseudo_hvdcp_info pseudo_hvdcp = {
	.mode = 0,
	.threshold = 7000,
};

extern int pmic_get_vbus(void);
static int pseudo_hvdcp_get_vbus(void)
{
	return pmic_get_vbus();
}

static void pseudo_hvdcp_notify(void)
{
	pseudo_power_supply_update("usb");

#ifdef CONFIG_LGE_PM_CHARGER_CONTROLLER
	chgctrl_set_pseudo_mode(PSEUDO_HVDCP, pseudo_hvdcp.mode);
#endif
}

static int param_set_pseudo_hvdcp(const char *val,
				  const struct kernel_param *kp)
{
	struct pseudo_hvdcp_info info;
	int ret;

	ret = sscanf(val, "%d %d", &info.mode, &info.threshold);
	if (ret != 2)
		return -EINVAL;

	pseudo_hvdcp = info;

	pr_info("fake hvdcp %s\n",
			(pseudo_hvdcp.mode ? "enabled" : "disabled"));

	pseudo_hvdcp_notify();

	return 0;
}

static int param_get_pseudo_hvdcp(char *buffer,
				  const struct kernel_param *kp)
{
	return scnprintf(buffer, PAGE_SIZE, "%s",
			(pseudo_hvdcp.mode ? "pseudo" : "normal"));
}

static struct kernel_param_ops pseudo_hvdcp_ops = {
	.set = param_set_pseudo_hvdcp,
	.get = param_get_pseudo_hvdcp,
};
module_param_cb(pseudo_hvdcp, &pseudo_hvdcp_ops, NULL, PSEUDO_HVDCP_RW_PERM);

static int param_set_pseudo_hvdcp_mode(const char *val,
				       const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret)
		return ret;

	pr_info("fake hvdcp %s\n",
			(pseudo_hvdcp.mode ? "enabled" : "disabled"));

	pseudo_hvdcp_notify();

	return 0;
}
module_param_call(hvdcp_mode, param_set_pseudo_hvdcp_mode, param_get_int,
		&pseudo_hvdcp.mode, PSEUDO_HVDCP_RW_PERM);
module_param_named(hvdcp_threshold, pseudo_hvdcp.threshold, int,
		PSEUDO_HVDCP_RW_PERM);

int pseudo_hvdcp_is_hvdcp(void)
{
	int vchr = 0;

	if (!pseudo_hvdcp.mode)
		return 0;

	vchr = pseudo_hvdcp_get_vbus();
	if (vchr > pseudo_hvdcp.threshold)
		return 1;

	return 0;
}
EXPORT_SYMBOL(pseudo_hvdcp_is_hvdcp);

int pseudo_hvdcp_is_enabled(void)
{
	return pseudo_hvdcp.mode;
}
EXPORT_SYMBOL(pseudo_hvdcp_is_enabled);
