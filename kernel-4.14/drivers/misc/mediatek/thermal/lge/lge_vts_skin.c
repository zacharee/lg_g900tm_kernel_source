/*
 * Copyright (C) 2015 MediaTek Inc.
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

#define pr_fmt(fmt) "[Theraml/TZ/SkinVTS] %s: " fmt, __func__

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include "mt-plat/mtk_thermal_platform.h"
#include <linux/uidgid.h>
#include <linux/slab.h>
#include "mt-plat/lge_vts_monitor.h"

#define VTS_DEBUG 0

#define VTS_NAME "lgetsskin"
#define VTS_PROC "tzskin_vts"

#define VTS_PARAMS 5
#define VTS_TRIPS 10

#define VTS_TEMP_CRIT (800)	/* 80.0 degree Celsius */
#define VTS_TEMP_SCALING (100)
#define VTS_WEIGHT_SCALING (1000)

struct vts_param {
	enum mtk_thermal_sensor_id id;
	int scaling;	/* multplier to convert to milli degree Celsius */
	int weight;
};

struct vts_trip {
	char cooler[THERMAL_NAME_LENGTH];
	int temp;
	int type;
};

static struct thermal_zone_device *thz_dev;
static DEFINE_SEMAPHORE(sem_mutex);
static int thermal_mode;

/* default parameters */
static struct vts_param default_params[VTS_PARAMS] = {
	{
		.id = MTK_THERMAL_SENSOR_BATTERY,
		.scaling = 1,
		.weight = VTS_WEIGHT_SCALING,
	},
	{ .id = -1, .scaling = 0, .weight = 0, },
	{ .id = -1, .scaling = 0, .weight = 0, },
	{ .id = -1, .scaling = 0, .weight = 0, },
	{ .id = -1, .scaling = 0, .weight = 0, },
};
static int default_vts_constant = 0;	/* milli degree Celsius */
static int default_cmode_constant = 0;	/* milli degree Celsius */

/* userspace parameters */
static struct vts_param user_params[VTS_PARAMS] = {
	{ .id = -1, .scaling = 0, .weight = 0, },
	{ .id = -1, .scaling = 0, .weight = 0, },
	{ .id = -1, .scaling = 0, .weight = 0, },
	{ .id = -1, .scaling = 0, .weight = 0, },
	{ .id = -1, .scaling = 0, .weight = 0, },
};
static int user_vts_constant = 0;	/* milli degree Celsius */
static int user_cmode_constant = 0;	/* milli degree Celsius */

/* trip points */
static struct vts_trip trips[VTS_TRIPS] = {
	{"no-cooler", 1200, THERMAL_TRIP_ACTIVE},
	{"no-cooler", 1100, THERMAL_TRIP_ACTIVE},
	{"no-cooler", 1000, THERMAL_TRIP_ACTIVE},
	{"no-cooler",  900, THERMAL_TRIP_ACTIVE},
	{"no-cooler",  800, THERMAL_TRIP_ACTIVE},
	{"no-cooler",  700, THERMAL_TRIP_ACTIVE},
	{"no-cooler",  600, THERMAL_TRIP_ACTIVE},
	{"no-cooler",  500, THERMAL_TRIP_ACTIVE},
	{"no-cooler",  400, THERMAL_TRIP_ACTIVE},
	{"no-cooler",  300, THERMAL_TRIP_ACTIVE},
};
static int num_trip = 0;
static unsigned int interval = 5000;	/* mseconds, 0 : no auto polling */

static int vts_valid_user_param(void)
{
	int i;

	for (i = 0; i < VTS_PARAMS; i++) {
		if (user_params[i].id < 0)
			continue;
		if (!user_params[i].weight)
			continue;
		if (!user_params[i].scaling)
			continue;

		return true;
	}
	return false;
}

static DEFINE_MUTEX(vts_lock);
static int vts_calculate_temp(bool use_default)
{
	struct vts_param *params = user_params;
	int vts_constant = user_vts_constant;
	int cmode_constant = user_cmode_constant;
	int vts_temp = 0;
	int temp;
	int i;

#if VTS_DEBUG
	int prev_vts_temp = vts_temp;
#endif

	mutex_lock(&vts_lock);

	if (use_default) {
		params = default_params;
		vts_constant = default_vts_constant;
		cmode_constant = default_cmode_constant;
	}

	for (i = 0; i < VTS_PARAMS; i++) {
		if (!params[i].weight || !params[i].scaling)
			continue;

		if (params[i].id < 0)
			continue;

		temp = mtk_thermal_get_temp(params[i].id);
		/* error while getting temp. return */
		if ((temp == -127000) || (temp == THERMAL_TEMP_INVALID)) {
			mutex_unlock(&vts_lock);
			return temp;
		}

		/* Convert to milli degree Celsius */
		temp *= params[i].scaling;

		/* accumulate temp with weight */
		vts_temp += temp * params[i].weight;
#if VTS_DEBUG
		pr_info("accumulate : %d = %d + (%d * %d)\n",
				vts_temp / VTS_WEIGHT_SCALING,
				prev_vts_temp / VTS_WEIGHT_SCALING,
				temp / VTS_WEIGHT_SCALING, params[i].weight);
		prev_vts_temp = vts_temp;
#endif
	}

	/* Convert to milli degree Celsius */
	vts_temp /= VTS_WEIGHT_SCALING;

	/* Add vts constant to adjust temp */
	vts_temp += vts_constant;
#if VTS_DEBUG
	pr_info("constant: %d = %d + %d\n",
			vts_temp, prev_vts_temp / VTS_WEIGHT_SCALING, vts_constant);
	prev_vts_temp = vts_temp;
#endif

	/* Add cmode constant to adjust temp during fast charging */
	if (lge_vts_cmode_enabled()) {
		vts_temp += cmode_constant;
#if VTS_DEBUG
		pr_info("cmode: %d = %d + %d\n",
				vts_temp, prev_vts_temp, cmode_constant);
		prev_vts_temp = vts_temp;
#endif
	}

	/* Convert milli degree Celsius to tenths of degree Celsius */
	vts_temp /= VTS_TEMP_SCALING;
#if VTS_DEBUG
	pr_info("convert unit: %d = %d / %d\n",
				vts_temp, prev_vts_temp, VTS_TEMP_SCALING);
	prev_vts_temp = vts_temp;
#endif

	mutex_unlock(&vts_lock);

	return vts_temp;
}

#define DEFAULT_TEMP 250
static int vts_get_temp(struct thermal_zone_device *thermal, int *t)
{
	static int old_temp = THERMAL_TEMP_INVALID;
	int temp = -127000;

	if (vts_valid_user_param())
		temp = vts_calculate_temp(false);

	if (temp == THERMAL_TEMP_INVALID) {
		temp = DEFAULT_TEMP;

		if (old_temp != THERMAL_TEMP_INVALID)
			temp = old_temp;
	}

	/* if failed to get temp, use default parameters */
	if (temp == -127000)
		temp = vts_calculate_temp(true);

	*t = temp;

	old_temp = temp;

	return 0;
}

static int vts_bind(struct thermal_zone_device *thermal,
		    struct thermal_cooling_device *cdev)
{
	int i;

	for (i = 0; i < VTS_TRIPS; i++) {
		if (!strcmp(cdev->type, trips[i].cooler))
			break;
	}

	if (i >= VTS_TRIPS)
		return 0;

	if (mtk_thermal_zone_bind_cooling_device(thermal, i, cdev))
		return -EINVAL;

	return 0;
}

static int vts_unbind(struct thermal_zone_device *thermal,
		      struct thermal_cooling_device *cdev)
{
	int i;

	for (i = 0; i < VTS_TRIPS; i++) {
		if (!strcmp(cdev->type, trips[i].cooler))
			break;
	}

	if (i >= VTS_TRIPS)
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, i, cdev))
		return -EINVAL;

	return 0;
}

static int vts_get_mode(struct thermal_zone_device *thermal,
			enum thermal_device_mode *mode)
{
	*mode = (thermal_mode) ?
		THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;

	return 0;
}

static int vts_set_mode(struct thermal_zone_device *thermal,
			enum thermal_device_mode mode)
{
	thermal_mode = mode;

	return 0;
}

static int vts_get_trip_type(struct thermal_zone_device *thermal,
			     int trip, enum thermal_trip_type *type)
{
	*type = trips[trip].type;

	return 0;
}

static int vts_get_trip_temp(struct thermal_zone_device *thermal,
			     int trip, int *temp)
{
	*temp = trips[trip].temp;

	return 0;
}

static int vts_get_crit_temp(struct thermal_zone_device *thermal,
			     int *temperature)
{
	*temperature = VTS_TEMP_CRIT;

	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops vts_dev_ops = {
	.bind = vts_bind,
	.unbind = vts_unbind,
	.get_temp = vts_get_temp,
	.get_mode = vts_get_mode,
	.set_mode = vts_set_mode,
	.get_trip_type = vts_get_trip_type,
	.get_trip_temp = vts_get_trip_temp,
	.get_crit_temp = vts_get_crit_temp,
};

static int vts_register_thermal(void)
{
	if (thz_dev)
		return 0;

	thz_dev = mtk_thermal_zone_device_register(VTS_NAME, num_trip,
			NULL, &vts_dev_ops, 0, 0, 0, interval);

	return 0;
}

static void vts_unregister_thermal(void)
{
	if (!thz_dev)
		return;

	mtk_thermal_zone_device_unregister(thz_dev);
	thz_dev = NULL;
}

/* proc interface */
static int vts_read(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < VTS_TRIPS; i++) {
		seq_printf(m, "[%d] cooler: %s temp: %d type: %d\n", i,
				trips[i].cooler,
				trips[i].temp, trips[i].type);
	}
	seq_printf(m, "time: %d\n", interval);

	return 0;
}

static ssize_t vts_write(struct file *file, const char __user *buffer,
				 size_t count, loff_t *data)
{
	struct vts_trip trip[VTS_TRIPS];
	int polling_time;
	char *buf;
	int rc;
	int i;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (!buf)
		return 0;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return 0;
	}
	buf[count] = '\0';

	rc = sscanf(buf, "%d"
			" %d %d %19s %d %d %19s %d %d %19s %d %d %19s"
			" %d %d %19s %d %d %19s %d %d %19s %d %d %19s"
			" %d %d %19s %d %d %19s"
			" %d",
			&num_trip,
			&(trip[0].temp), &(trip[0].type), trip[0].cooler,
			&(trip[1].temp), &(trip[1].type), trip[1].cooler,
			&(trip[2].temp), &(trip[2].type), trip[2].cooler,
			&(trip[3].temp), &(trip[3].type), trip[3].cooler,
			&(trip[4].temp), &(trip[4].type), trip[4].cooler,
			&(trip[5].temp), &(trip[5].type), trip[5].cooler,
			&(trip[6].temp), &(trip[6].type), trip[6].cooler,
			&(trip[7].temp), &(trip[7].type), trip[7].cooler,
			&(trip[8].temp), &(trip[8].type), trip[8].cooler,
			&(trip[9].temp), &(trip[9].type), trip[9].cooler,
			&polling_time);
	kfree(buf);

	if (!rc)
		return -EINVAL;

	down(&sem_mutex);
	vts_unregister_thermal();

	if (num_trip < 0 || num_trip > 10) {
		up(&sem_mutex);
		return -EINVAL;
	}

	for (i = 0; i < VTS_TRIPS; i++) {
		trips[i].temp = trip[i].temp;
		trips[i].type = trip[i].type;
		strncpy(trips[i].cooler, trip[i].cooler, THERMAL_NAME_LENGTH - 1);
	}

	interval = polling_time;

	vts_register_thermal();
	up(&sem_mutex);

	return count;
}

static int vts_open(struct inode *inode, struct file *file)
{
	return single_open(file, vts_read, NULL);
}

static const struct file_operations vts_fops = {
	.owner = THIS_MODULE,
	.open = vts_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = vts_write,
	.release = single_release,
};

static int vts_param_read(struct seq_file *m, void *v)
{
	struct vts_param *params = user_params;
	int vts_constant = user_vts_constant;
	int cmode_constant = user_cmode_constant;
	int i;

	if (!vts_valid_user_param()) {
		params = default_params;
		vts_constant = default_vts_constant;
		cmode_constant = default_cmode_constant;
	}

	for (i = 0; i < VTS_PARAMS; i++) {
		seq_printf(m, "[%d] tz: %s weight: %d scale: %d\n", i,
				lge_vts_get_thermal_sensor_name(params[i].id),
				params[i].weight, params[i].scaling);
	}

	seq_printf(m, "vts_constant: %d\n", vts_constant);
	seq_printf(m, "cmode_constant: %d\n", cmode_constant);

	return 0;
}

static ssize_t vts_param_write(struct file *file, const char __user *buffer,
			       size_t count, loff_t *data)
{
	struct param {
		char tz[THERMAL_NAME_LENGTH];
		int scaling;	/* multplier to convert to milli degree Celsius */
		int weight;
	};

	struct param param[VTS_PARAMS];
	int vts_constant;
	int cmode_constant;
	char *buf;
	int rc;
	int i;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (!buf)
		return 0;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return 0;
	}
	buf[count] = '\0';

	rc = sscanf(buf, "%d %d" " %19s %d %d" " %19s %d %d" " %19s %d %d"
			" %19s %d %d" " %19s %d %d",
			&vts_constant, &cmode_constant,
			param[0].tz, &(param[0].scaling), &(param[0].weight),
			param[1].tz, &(param[1].scaling), &(param[1].weight),
			param[2].tz, &(param[2].scaling), &(param[2].weight),
			param[3].tz, &(param[3].scaling), &(param[3].weight),
			param[4].tz, &(param[4].scaling), &(param[4].weight));
	kfree(buf);

	if (!rc)
		return -EINVAL;


	user_vts_constant = vts_constant;
	user_cmode_constant = cmode_constant;
	for (i = 0; i < VTS_PARAMS; i++) {
		user_params[i].id = lge_vts_get_thermal_sensor_id(param[i].tz);
		user_params[i].weight = param[i].weight;
		user_params[i].scaling = param[i].scaling;
	}

	return count;
}

static int vts_param_open(struct inode *inode, struct file *file)
{
	return single_open(file, vts_param_read, NULL);
}

static const struct file_operations vts_param_fops = {
	.owner = THIS_MODULE,
	.open = vts_param_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = vts_param_write,
	.release = single_release,
};

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static int __init vts_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *vts_dir = NULL;

	vts_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!vts_dir)
		return 0;

	entry = proc_create(VTS_PROC, S_IRUGO | S_IWUSR | S_IWGRP,
			vts_dir, &vts_fops);
	if (entry)
		proc_set_user(entry, uid, gid);

	entry = proc_create(VTS_PROC "_param", S_IRUGO | S_IWUSR | S_IWGRP,
			vts_dir, &vts_param_fops);
	if (entry)
		proc_set_user(entry, uid, gid);

	return 0;
}

static void __exit vts_exit(void)
{
	vts_unregister_thermal();
}
module_init(vts_init);
module_exit(vts_exit);
