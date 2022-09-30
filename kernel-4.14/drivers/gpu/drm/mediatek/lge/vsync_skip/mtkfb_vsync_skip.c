/* Copyright (c) 2016, LG Electronics Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/types.h>
#include <linux/device.h>
#include "mtkfb_vsync_skip.h"
#include "mtk_dsi.h"
//#include "primary_display.h"
//#include "display_recorder.h"

extern int primary_display_force_set_fps(unsigned int keep, unsigned int skip);

static unsigned int vsync_skip_param[61][2] = {
	/* keep, skip */
	{  0,  0},
	{  1, 59}, {  1, 29}, {  1, 19}, {  1, 14},
	{  1, 11}, {  1,  9}, {  1,  7}, {  2, 13}, {  1,  6},
	{  1,  5}, {  2,  9}, {  1,  4}, {  2,  7}, {  3, 10},
	{  1,  3}, {  3,  8}, {  2,  5}, {  3,  7}, {  4,  9},
	{  1,  2}, {  4,  7}, {  3,  5}, {  5,  8}, {  2,  3},
	{  5,  7}, {  6,  8}, {  4,  5}, {  5,  6}, {  6,  7},
	{  1,  1}, {  7,  6}, {  6,  5}, {  5,  4}, {  8,  6},
	{  7,  5}, {  3,  2}, {  8,  5}, {  5,  3}, {  7,  4},
	{  2,  1}, {  9,  4}, {  7,  3}, {  5,  2}, {  8,  3},
	{  3,  1}, { 10,  3}, {  7,  2}, {  4,  1}, {  9,  2},
	{  5,  1}, {  6,  1}, { 13,  2}, {  7,  1}, {  9,  1},
	{ 11,  1}, { 14,  1}, { 19,  1}, { 29,  1}, { 59,  1},
	{  0,  0},
};

struct vsync_skip_data vsync_skip;
struct vsync_skip_data* vsync_skip_get_data(void)
{
	return &vsync_skip;
}

static int vsync_skip_adjust_param(int fps) {

	if (fps <= 0 || fps >= 60) {
		vsync_skip.keep = 0;
		vsync_skip.skip = 0;
		return 0;
	}

	vsync_skip.keep = vsync_skip_param[fps][0];
	vsync_skip.skip = vsync_skip_param[fps][1];
	return 0;
}

ssize_t fps_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ulong fps;

	if (!count)
		return -EINVAL;

	fps = simple_strtoul(buf, NULL, 10);

	if (fps == 0 || fps >= 60) {
		vsync_skip.enable_skip_vsync = 0;
		vsync_skip.skip_ratio = 60;
		vsync_skip_adjust_param(vsync_skip.skip_ratio);
		primary_display_force_set_fps(vsync_skip.keep,
					      vsync_skip.skip);
		pr_info("Disable frame skip.\n");
	} else {
		vsync_skip.enable_skip_vsync = 1;
		vsync_skip.skip_ratio = fps;
		vsync_skip_adjust_param(vsync_skip.skip_ratio);
		primary_display_force_set_fps(vsync_skip.keep,
					      vsync_skip.skip);
		pr_info("Enable frame skip: Set to %lu fps.\n", fps);
	}
	return count;
}

ssize_t fps_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int r = 0;

	r = snprintf(buf, PAGE_SIZE,
		     "enable_skip_vsync=%d\n"
		     "keep=%d\n"
		     "skip=%d\n"
		     "fps_cnt=%d\n",
		     vsync_skip.enable_skip_vsync,
		     vsync_skip.keep,
		     vsync_skip.skip,
		     vsync_skip.fps_cnt);
	return r;
}

ssize_t fps_ratio_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int r = 0;
	r = snprintf(buf, PAGE_SIZE, "%d 60\n", vsync_skip.skip_ratio);
	return r;
}

ssize_t fps_fcnt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int r = 0;
	static unsigned int fps_cnt_before = 0;

	if (vsync_skip.fps_cnt < 0)
		goto read_fail;

	r = snprintf(buf, PAGE_SIZE, "%d\n",
		     vsync_skip.fps_cnt - fps_cnt_before);

	if (vsync_skip.fps_cnt > UINT_MAX / 2) {
		vsync_skip.fps_cnt = 0;
		fps_cnt_before = 0;
	} else {
		fps_cnt_before = vsync_skip.fps_cnt;
	}

	return r;

read_fail:
	fps_cnt_before = 0;
	r = snprintf(buf,PAGE_SIZE, "0\n");
	return r;
}

ssize_t show_blank_event_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct mtk_dsi *dsi = NULL;
	int r = 0;

	dsi = dev_get_drvdata(dev);
	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	r = snprintf(buf, PAGE_SIZE, "panel_power_on = %d\n",
		     dsi->output_pwr_mode);
	return r;
}

static DEVICE_ATTR(vfps, S_IRUGO | S_IWUSR | S_IWGRP, fps_show, fps_store);
static DEVICE_ATTR(vfps_fcnt, S_IRUGO, fps_fcnt_show, NULL);
static DEVICE_ATTR(vfps_ratio, S_IRUGO, fps_ratio_show, NULL);
static DEVICE_ATTR(show_blank_event, S_IRUGO, show_blank_event_show, NULL);

int lge_vsync_skip_create_sysfs(struct mtk_dsi *dsi)
{
	static struct class *class_panel = NULL;
	static struct device *vsync_skip_sysfs_dev = NULL;
	int rc = 0;

	if (!dsi) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	if(!class_panel){
		class_panel = class_create(THIS_MODULE, "vsync");
		if (IS_ERR(class_panel)) {
			pr_err("Failed to create vsync class\n");
			return -EINVAL;
		}
	}

	if (!vsync_skip_sysfs_dev) {
		vsync_skip_sysfs_dev = device_create(class_panel, NULL, 0, dsi, "vsync_skip");
		if (IS_ERR(vsync_skip_sysfs_dev)) {
			pr_err("Failed to create dev(vsync_skip_sysfs_dev)!");
		} else {
			if ((rc = device_create_file(vsync_skip_sysfs_dev, &dev_attr_vfps)) < 0)
				pr_err("add error mask node fail!");
			if ((rc = device_create_file(vsync_skip_sysfs_dev, &dev_attr_vfps_fcnt)) < 0)
				pr_err("add mem test node fail!");
			if ((rc = device_create_file(vsync_skip_sysfs_dev, &dev_attr_vfps_ratio)) < 0)
				pr_err("add error crash node fail!");
			if ((rc = device_create_file(vsync_skip_sysfs_dev, &dev_attr_show_blank_event)) < 0)
				pr_err("add error crash node fail!");
		}
	}
	return rc;
}
