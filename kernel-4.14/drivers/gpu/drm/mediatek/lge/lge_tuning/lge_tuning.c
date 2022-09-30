/*
*lge_tuning.c
*
* Copyright (c) 2017 LGE.
*
* author : woonghwan.lee@lge.com and junil.cho@lge.com
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#include <linux/kallsyms.h>
#include <linux/backlight.h>
#include <linux/leds.h>
#include <linux/device.h>
#include <linux/of.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include "mtk_dsi.h"
#include "mtk_panel_ext.h"
#include "mtk_debug.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_disp_recovery.h"
#include "lge_tuning.h"

int bl_type;

extern void mtk_disp_esd_check_switch(struct drm_crtc *crtc, bool enable);
extern struct lge_blmap *lge_get_blmap(struct mtk_panel_ext *panel, enum lge_blmap_type type);

static void lge_set_dsi_value(struct mtk_dsi *dsi, LGE_DISP_CONFIG mode,int value)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(lge_get_crtc());
	int dsi_tmp_buf_bpp = 0;

	dsi->ext->params->is_state_recovery = true;
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	if(dsi->format == MIPI_DSI_FMT_RGB565)
		dsi_tmp_buf_bpp = 2;
	else
		dsi_tmp_buf_bpp = 3;

	switch(mode) {
		case LGE_VFP:
			dsi->vm.vfront_porch = value;
			dsi->vfp = value;
		    writel(dsi->vfp, dsi->regs + LGE_DSI_VFP_NL);
		    break;
		case LGE_VBP:
			dsi->vm.vback_porch = value;
			dsi->vbp = value;
		    writel(dsi->vbp, dsi->regs + LGE_DSI_VBP_NL);
		    break;
		case LGE_VSA:
			dsi->vm.vsync_len = value;
			dsi->vsa = value;
		    writel(dsi->vsa, dsi->regs + LGE_DSI_VSA_NL);
		    break;
		case LGE_HSA:
			dsi->vm.hsync_len = value;
		    dsi->hsa_byte = ALIGN_TO((value * dsi_tmp_buf_bpp - 10), 4);
			writel(dsi->hsa_byte, dsi->regs + LGE_DSI_HSA_WC);
		    break;
		case LGE_HFP:
			dsi->vm.hfront_porch = value;
		    dsi->hfp_byte = ALIGN_TO((value * dsi_tmp_buf_bpp - 12), 4);
			writel(dsi->hfp_byte, dsi->regs + LGE_DSI_HFP_WC);
		    break;
		case LGE_HBP:
			dsi->vm.hback_porch = value;
		    if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
				dsi->hbp_byte =
					ALIGN_TO((value * dsi_tmp_buf_bpp - 10), 4);
			else
				dsi->hbp_byte =
					ALIGN_TO(((value + dsi->vm.hfront_porch) * dsi_tmp_buf_bpp - 10), 4);
			writel(dsi->hbp_byte, dsi->regs + LGE_DSI_HBP_WC);
		    break;
		case LGE_SSC:
		    if(value != 0)
				value = 0;
			else
				value = 1;

		    //DSI_ssc_enable(dsi_index,value);
		    break;
		default :
		    break;
	}
	dsi->ext->params->is_state_recovery = false;
}

//sysfs start
static ssize_t lge_disp_config_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dsi *dsi = NULL;
	ssize_t ret = 0;

	dsi = dev_get_drvdata(dev);
	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return ret;
	}

	ret = sprintf(buf," 0.PLLCLOCK : %d MHz\n 1.SSC_EN : %d\n 2.VFP : %d\n 3.VBP : %d\n\
 4.VSA : %d\n 5.HFP : %d\n 6.HBP : %d\n 7.HSA : %d\n 8.ESD_ON : %d\n",\
			dsi->ext->params->pll_clk,\
			dsi->ext->params->ssc_disable,\
			dsi->vm.vfront_porch,\
			dsi->vm.vback_porch,\
			dsi->vm.vsync_len,\
			dsi->vm.hfront_porch,\
			dsi->vm.hback_porch,\
			dsi->vm.hsync_len,\
			dsi->ext->params->esd_check_enable);

	return ret;
}

static ssize_t lge_disp_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mtk_dsi *dsi = NULL;
	int new_value, mode;
	LGE_DISP_CONFIG disp_mode = LGE_PLLCLK;

	dsi = dev_get_drvdata(dev);
	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return size;
	}

	sscanf(buf, "%d %d\n", &mode, &new_value);
	if(new_value <= 0)
		new_value = 0;

	switch(mode) {
		case 0: //PLL_CLOCK
			disp_mode = LGE_PLLCLK;
			dsi->ext->params->pll_clk = new_value;
			goto PLL;
			break;
		case 1 : //ssc_en
			disp_mode = LGE_SSC;
			break;
		case 2://VFP
			disp_mode = LGE_VFP;
			break;
		case 3://VBP
			disp_mode = LGE_VBP;
			break;
		case 4://VSA
			disp_mode = LGE_VSA;
			break;
		case 5://HFP
			disp_mode = LGE_HFP;
			break;
		case 6://HBP
			disp_mode = LGE_HBP;
			break;
		case 7://HSA
			disp_mode = LGE_HSA;
			break;
		case 8://ESD_ON_OFF
			disp_mode = LGE_ESD_ON_OFF;
			dsi->ext->params->esd_check_enable = new_value;
			mtk_disp_esd_check_switch(lge_get_crtc(), new_value);
			return size;
			break;
		default:
			break;
	}
	lge_set_dsi_value(dsi, disp_mode, new_value);
PLL:
	mtk_drm_report_panel_dead();

	return size;
}
static DEVICE_ATTR(dsi_params, S_IRUGO | S_IWUSR | S_IWGRP, lge_disp_config_show, lge_disp_config_store);

static ssize_t lge_disp_config_add_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(lge_get_crtc());
	struct mtk_dsi_phy_timcon *phy_timcon = NULL;
	struct mtk_dsi *dsi = NULL;
	ssize_t ret = 0;
	u32 value = 0;

	dsi = dev_get_drvdata(dev);
	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return ret;
	}

	phy_timcon = kzalloc(sizeof(struct mtk_dsi_phy_timcon), GFP_KERNEL);
	if(!phy_timcon) {
		pr_err("alloc memory for dsi phy timing params is failed!\n");
		return ret;
	}

	if(dsi->output_en == false) {
		pr_err("dsi is poweroff\n");
		kfree(phy_timcon);
		return ret;
	}


	dsi->ext->params->is_state_recovery = true;
	mtk_drm_idlemgr_kick(__func__, &mtk_crtc->base, 0);

	value = readl(dsi->regs + DSI_PHY_TIMECON0);
	phy_timcon->hs_prpr = REG_FLD_VAL_GET(FLD_HS_PREP, value);
	phy_timcon->hs_zero = REG_FLD_VAL_GET(FLD_HS_ZERO, value);
	phy_timcon->hs_trail = REG_FLD_VAL_GET(FLD_HS_TRAIL, value);

	value = readl(dsi->regs + DSI_PHY_TIMECON1);
	phy_timcon->ta_go = REG_FLD_VAL_GET(FLD_TA_GO, value);
	phy_timcon->ta_sure = REG_FLD_VAL_GET(FLD_TA_SURE, value);
	phy_timcon->ta_get = REG_FLD_VAL_GET(FLD_TA_GET, value);
	phy_timcon->da_hs_exit = REG_FLD_VAL_GET(FLD_DA_HS_EXIT, value);

	value = readl(dsi->regs + DSI_PHY_TIMECON2);
	phy_timcon->cont_det = REG_FLD_VAL_GET(FLD_CONT_DET, value);
	phy_timcon->clk_zero = REG_FLD_VAL_GET(FLD_CLK_HS_ZERO, value);
	phy_timcon->clk_trail = REG_FLD_VAL_GET(FLD_CLK_HS_TRAIL, value);

	value = readl(dsi->regs + DSI_PHY_TIMECON3);
	phy_timcon->clk_hs_prpr = REG_FLD_VAL_GET(FLD_CLK_HS_PREP, value);
	phy_timcon->clk_hs_post = REG_FLD_VAL_GET(FLD_CLK_HS_POST, value);
	phy_timcon->clk_hs_exit = REG_FLD_VAL_GET(FLD_CLK_HS_EXIT, value);

	ret = sprintf(buf," 0.HS_PRPR = %d\n 1.HS_ZERO = %d\n 2.HS_TRAIL = %d\n 3.TA_GO = %d\n\
 4.TA_SURE = %d\n 5.TA_GET = %d\n 6.DA_HS_EXIT = %d\n 7.CLK_ZERO = %d\n 8.CLK_TRAIL = %d\n\
 9.CONT_DET = %d\n 10.CLK_HS_PRPR = %d\n 11.CLK_HS_POST = %d\n 12.CLK_HS_EXIT = %d\n",\
			phy_timcon->hs_prpr, phy_timcon->hs_zero, phy_timcon->hs_trail,\
			phy_timcon->ta_go,\
			phy_timcon->ta_sure, phy_timcon->ta_get, phy_timcon->da_hs_exit,\
			phy_timcon->clk_zero, phy_timcon->clk_trail,\
			phy_timcon->cont_det, phy_timcon->clk_hs_prpr, phy_timcon->clk_hs_post,\
			phy_timcon->clk_hs_exit);

	dsi->ext->params->is_state_recovery = false;
	kfree(phy_timcon);

	return ret;
}

static ssize_t lge_disp_config_add_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mtk_dsi_phy_timcon *phy_timcon = NULL;
	struct mtk_dsi *dsi = NULL;
	int type, value;

	dsi = dev_get_drvdata(dev);
	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return size;
	}

	if(dsi->output_en == false) {
		pr_err("dsi is poweroff\n");
		return size;
	}

	phy_timcon = &dsi->ext->params->phy_timcon;

	sscanf(buf, "%d %d\n", &type, &value);
	if(type > 12 || type < 0) {
		pr_err("Abnormal dsi param type!\n");
		return size;
	}

	switch(type) {
		case 0:
			phy_timcon->hs_prpr = value;
			break;
		case 1:
			phy_timcon->hs_zero = value;
			break;
		case 2:
			phy_timcon->hs_trail = value;
			break;
		case 3:
			phy_timcon->ta_go = value;
			break;
		case 4:
			phy_timcon->ta_sure = value;
			break;
		case 5:
			phy_timcon->ta_get = value;
			break;
		case 6:
			phy_timcon->da_hs_exit = value;
			break;
		case 7:
			phy_timcon->clk_zero = value;
			break;
		case 8:
			phy_timcon->clk_trail = value;
			break;
		case 9:
			phy_timcon->cont_det = value;
			break;
		case 10:
			phy_timcon->clk_hs_prpr = value;
			break;
		case 11:
			phy_timcon->clk_hs_post = value;
			break;
		case 12:
			phy_timcon->clk_hs_exit = value;
			break;
	}

	mtk_drm_report_panel_dead();
	return size;
}
static DEVICE_ATTR(dsi_phy_params, S_IRUGO | S_IWUSR | S_IWGRP, lge_disp_config_add_show, lge_disp_config_add_store);

static ssize_t show_lcd_init_cmd_full(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j, k;
	int str_size = 0;
	struct mtk_panel_para_table * table = NULL;
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;

	dsi = dev_get_drvdata(dev);
	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return 0;
	}

	panel = dsi->ext;

	if (panel->funcs->get_lcm_init_cmd_str)
			table = panel->funcs->get_lcm_init_cmd_str();

	if (panel->funcs->get_init_cmd_str_size)
			str_size = panel->funcs->get_init_cmd_str_size();

	if(table == NULL || str_size == 0)
		return -EINVAL;

	for (i = 0, j = 0; i < str_size && j < PAGE_SIZE; ++i) {
		sprintf(&buf[j], "%d, ", table[i].count);

		if (table[i].count < 10)
			j += 3;
		else if (table[i].count < 100)
	        	j += 4;
		else
	        	j += 5;
		buf[j++] = '{';
		for( k = 0; k < table[i].count; k++ ) {
			sprintf(&buf[j], "%02x, ", table[i].para_list[k]);
			j += 4;
		}
		buf[j] = '}';
		buf[++j] = '\n';
		j++;
	}
	return j;
}

static ssize_t store_lcd_init_cmd_full(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int i, j, k;
	int str_size = 0;
	struct mtk_panel_para_table * table = NULL;
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;

	dsi = dev_get_drvdata(dev);
	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return size;
	}

	panel = dsi->ext;

	if (panel->funcs->get_lcm_init_cmd_str)
		table = panel->funcs->get_lcm_init_cmd_str();

	if (panel->funcs->get_init_cmd_str_size)
		str_size = panel->funcs->get_init_cmd_str_size();

	if(table == NULL || str_size == 0)
		return -EINVAL;

	if (size < 1)
	    return size;

	for (i = 0, j = 0; i < str_size && j < PAGE_SIZE; ++i) {
        while (!isdigit(buf[j]))
			j++;
		sscanf(&buf[j], "%d", &table[i].count);
		printk("[LGE_TUNING] table[%d].count = %d\n", i, table[i].count);

		if (table[i].count == 0) {
			if(table[i].para_list[0] < 10)
				j += 6;
			else if(table[i].para_list[0] >= 10 && table[i].para_list[0] < 100)
				j += 7;
			else
				j += 8;
			continue;
		}

		if (table[i].count < 10)
			j += 4;
		else if (table[i].count < 100)
			j += 5;
		else
			j += 6;

		for( k = 0; k < table[i].count; k++){
			sscanf(&buf[j], "%02x, ", &table[i].para_list[k]);
			printk("[LGE_TUNING] para_list[%d] = %02x\n", k, table[i].para_list[k]);
			j += 4;
			if( buf[j] == '}' ){
				j++;
				break;
			}
		}
		if( buf[j] == '/' )
			break;
	}

	mtk_drm_report_panel_dead();

	return size;
}
static DEVICE_ATTR(init_cmd, S_IRUGO | S_IWUSR | S_IWGRP, show_lcd_init_cmd_full, store_lcd_init_cmd_full);

static ssize_t store_bl_type(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *pvalue = NULL;
	unsigned int blmap_type = 0;

	blmap_type = simple_strtoul(buf, &pvalue, 10);

	if(blmap_type >= LGE_BLMAP_TYPE_MAX)
		pr_err("Abnormal bl_type set\n");
	else
		bl_type = blmap_type;

	return size;
}

static ssize_t show_bl_type(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	switch(bl_type) {
		case LGE_BLMAP_DEFAULT:
			return sprintf(buf, "LGE_BLMAP_DEFAULT(%d)\n", bl_type);
			break;
		case LGE_BLMAP_VE:
			return sprintf(buf, "LGE_BLMAP_VE(%d)\n", bl_type);
			break;
		case LGE_BLMAP_EX:
			return sprintf(buf, "LGE_BLMAP_EX(%d)\n", bl_type);
			break;
		case LGE_BLMAP_BRIGHTER:
			return sprintf(buf, "LGE_BLMAP_BRIGHTER(%d)\n", bl_type);
			break;
		case LGE_BLMAP_HDR:
			return sprintf(buf, "LGE_BLMAP_HDR(%d)\n", bl_type);
			break;
		case LGE_BLMAP_VR:
			return sprintf(buf, "LGE_BLMAP_VR(%d)\n", bl_type);
			break;
		case LGE_BLMAP_DAYLIGHT:
			return sprintf(buf, "LGE_BLMAP_DAYLIGHT(%d)\n", bl_type);
			break;
		case LGE_BLMAP_HDR_DAYLIGHT:
			return sprintf(buf, "LGE_BLMAP_HDR_DAYLIGHT(%d)\n", bl_type);
			break;
		default:
			return sprintf(buf, "Abnormal bl_type was set(%d)\n", bl_type);
			break;
	}
}

static DEVICE_ATTR(bl_type, S_IRUGO | S_IWUSR | S_IWGRP, show_bl_type, store_bl_type);

static ssize_t show_brightness_tb(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_panel_ext *panel = NULL;
	struct mtk_dsi *dsi = NULL;
	int *table = NULL;
	struct lge_blmap *blmap = NULL;
	int i=0, j=0;

	dsi = dev_get_drvdata(dev);
	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return j;
	}

	panel = dsi->ext;
	blmap = lge_get_blmap(panel, bl_type);
	table = blmap->map;

	for (i = 0, j = 0; i < blmap->size && j < PAGE_SIZE; ++i) {
		if (!(i % 15) && i != 0) {
				buf[j] = '\n';
				++j;
		}
		sprintf(&buf[j], "%d ", table[i]);
		if (table[i] < 10)
			j += 2;
		else if (table[i] < 100)
			j += 3;
		else if (table[i] < 1000)
			j += 4;
		else
			j += 5;
	}

	return j;
}

static ssize_t store_brightness_tb(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct mtk_panel_ext *panel = NULL;
	struct mtk_dsi *dsi = NULL;
	struct lge_blmap *blmap = NULL;
	unsigned int *table = NULL;
	int i;
	int j;
	int value, ret=0;

	dsi = dev_get_drvdata(dev);
	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return ret;
	}

	if (size < 1)
		return size;

	panel = dsi->ext;
	blmap = lge_get_blmap(panel, bl_type);
	table = blmap->map;

	for (i = 0, j = 0; i < size && j < blmap->size; ++i) {
		if (!isdigit(buf[i]))
				continue;

		ret = sscanf(&buf[i], "%d", &value);
		if (ret < 1)
				pr_err("read error\n");
		table[j] = (unsigned int)value;

		while (isdigit(buf[i]))
				++i;
		++j;
	}

	return size;
}
static DEVICE_ATTR(brightness_tb, S_IRUGO | S_IWUSR | S_IWGRP, show_brightness_tb, store_brightness_tb);

int lge_tuning_create_sysfs(struct mtk_dsi *dsi,
		struct class *class_panel)
{
	int rc = 0;
	static struct device *tuning_sysfs_dev = NULL;
	struct mtk_panel_ext *panel= NULL;

	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return -1;
	}

	panel = dsi->ext;

	if(!tuning_sysfs_dev) {
		tuning_sysfs_dev = device_create(class_panel, NULL, 0, dsi, "tuning");
		if(IS_ERR(tuning_sysfs_dev)) {
			pr_err("Failed to create dev(tuning_sysfs_dev)!\n");
		} else {
			if (panel->lge.lge_tune_dsi_params) {
				if ((rc = device_create_file(tuning_sysfs_dev,
								&dev_attr_dsi_params)) < 0)
					pr_err("add lge_tune_dsi_params node fail!");
			}

			if (panel->lge.lge_tune_dsi_phy_params) {
				if ((rc = device_create_file(tuning_sysfs_dev,
								&dev_attr_dsi_phy_params)) < 0)
					pr_err("add lge_tune_dsi_phy_params node fail!");
			}

			if (panel->lge.lge_tune_brightness) {
				if ((rc = device_create_file(tuning_sysfs_dev,
								&dev_attr_bl_type)) < 0)
					pr_err("add lge_tune_bl_type node fail!");
				if ((rc = device_create_file(tuning_sysfs_dev,
								&dev_attr_brightness_tb)) < 0)
					pr_err("add lge_tune_brightness_tb node fail!");
			}

			if (panel->lge.lge_tune_init_cmd) {
				if ((rc = device_create_file(tuning_sysfs_dev,
								&dev_attr_init_cmd)) < 0)
					pr_err("add lge_tune_init_cmd node fail!");
			}
		}
	}
	return rc;
}
