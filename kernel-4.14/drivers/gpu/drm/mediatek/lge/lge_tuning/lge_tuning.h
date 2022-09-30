/*
* lge_tuning.h
*
* Copyright (c) 2017 LGE.
*
* author : woonghwan.lee@lge.com and junil.cho@lge.com
*
* This software is licensed under the terms of the GNU general Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#ifndef __LGE_SETTING_H__
#define __LGE_SETTING_H__

#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/ctype.h>

#include "../../mtk_dsi.h"

#define LGE_DSI_VSA_NL 0x20
#define LGE_DSI_VBP_NL 0x24
#define LGE_DSI_VFP_NL 0x28

#define LGE_DSI_HSA_WC 0x50
#define LGE_DSI_HBP_WC 0x54
#define LGE_DSI_HFP_WC 0x58

typedef enum{
	LGE_PLLCLK =0,
	LGE_SSC,
	LGE_VFP,
	LGE_VBP,
	LGE_VSA,
	LGE_HFP,
	LGE_HBP,
	LGE_HSA,
	LGE_ESD_ON_OFF,
}LGE_DISP_CONFIG;

static int esd_on_flag = 1;

#if defined(CONFIG_LGE_INIT_CMD_TUNING)
static int reg_num_idx = -1;
static unsigned char init_cmd;
#endif

int lge_tuning_create_sysfs(struct mtk_dsi *dsi, struct class *class_panel);

#endif
