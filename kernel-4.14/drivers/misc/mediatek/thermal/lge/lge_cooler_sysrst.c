/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#if defined(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif

/*=============================================================
 *Local variable definition
 *=============================================================
 */
static unsigned int cl_dev_sysrst_state_quiet;
static unsigned int cl_dev_sysrst_state_skin;
static unsigned int cl_dev_sysrst_state_battery;
static struct thermal_cooling_device *cl_dev_sysrst_quiet;
static struct thermal_cooling_device *cl_dev_sysrst_skin;
static struct thermal_cooling_device *cl_dev_sysrst_battery;
/*=============================================================
 */

/*
 * cooling device callback functions (tscpu_cooling_sysrst_ops)
 * 1 : ON and 0 : OFF
 */
static int sysrst_quiet_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int sysrst_quiet_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_sysrst_state_quiet;
	return 0;
}

static int sysrst_quiet_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_sysrst_state_quiet = state;

	if (cl_dev_sysrst_state_quiet == 1) {
		pr_err("sysrst_quiet_set_cur_state = 1\n");
		pr_err("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		pr_err("*****************************************\n");
		pr_err("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");


		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG();


	}
	return 0;
}

static int sysrst_skin_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int sysrst_skin_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_sysrst_state_skin;
	return 0;
}

static int sysrst_skin_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_sysrst_state_skin = state;

	if (cl_dev_sysrst_state_skin == 1) {
		pr_err("sysrst_skin_set_cur_state = 1\n");
		pr_err("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		pr_err("*****************************************\n");
		pr_err("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");


		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG();

	}
	return 0;
}


static int sysrst_battery_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int sysrst_battery_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_sysrst_state_battery;
	return 0;
}

static int sysrst_battery_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_sysrst_state_battery = state;

	if (cl_dev_sysrst_state_battery == 1) {
		pr_err("sysrst_battery_set_cur_state = 1\n");
		pr_err("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		pr_err("*****************************************\n");
		pr_err("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");

		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG();

	}
	return 0;
}

static struct thermal_cooling_device_ops lgetsquiet_cooling_sysrst_ops = {
	.get_max_state = sysrst_quiet_get_max_state,
	.get_cur_state = sysrst_quiet_get_cur_state,
	.set_cur_state = sysrst_quiet_set_cur_state,
};

static struct thermal_cooling_device_ops lgetsskin_cooling_sysrst_ops = {
	.get_max_state = sysrst_skin_get_max_state,
	.get_cur_state = sysrst_skin_get_cur_state,
	.set_cur_state = sysrst_skin_set_cur_state,
};

static struct thermal_cooling_device_ops lgetsbattery_cooling_sysrst_ops = {
	.get_max_state = sysrst_battery_get_max_state,
	.get_cur_state = sysrst_battery_get_cur_state,
	.set_cur_state = sysrst_battery_set_cur_state,
};

static int __init lge_cooler_sysrst_init(void)
{
	pr_debug("lge_cooler_sysrst_init: Start\n");

	cl_dev_sysrst_quiet = mtk_thermal_cooling_device_register(
						"lgetsquiet-sysrst", NULL,
						&lgetsquiet_cooling_sysrst_ops);

	cl_dev_sysrst_skin = mtk_thermal_cooling_device_register(
						"lgetsskin-sysrst", NULL,
						&lgetsskin_cooling_sysrst_ops);

	cl_dev_sysrst_battery = mtk_thermal_cooling_device_register(
						"lgetsbattery-sysrst", NULL,
						&lgetsbattery_cooling_sysrst_ops);

	pr_debug("lge_cooler_sysrst_init: End\n");
	return 0;
}

static void __exit lge_cooler_sysrst_exit(void)
{
	pr_debug("lge_cooler_sysrst_exit\n");

	if (cl_dev_sysrst_quiet) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst_quiet);
		cl_dev_sysrst_quiet = NULL;
	}

	if (cl_dev_sysrst_skin) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst_skin);
		cl_dev_sysrst_skin = NULL;
	}

	if (cl_dev_sysrst_battery) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst_battery);
		cl_dev_sysrst_battery = NULL;
	}
}
module_init(lge_cooler_sysrst_init);
module_exit(lge_cooler_sysrst_exit);
