/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_mode.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>


#include "mtk_dsi.h"
#include "mtk_disp_recovery.h"
#include "board_lge.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "mtk_panel_ext.h"
#include "mtk_log.h"
#include "mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include "mtk_debug.h"
#include "mtk_drm_crtc.h"

//#define SUPPORT_VIDEO_MODE

#define REGFLAG_DELAY 0
#define ENABLE 1
#define DISABLE 0

#define CPD_TAG                  "[LCD] "
#define CPD_FUN(f)               printk(CPD_TAG"%s\n", __FUNCTION__)
#define CPD_ERR(fmt, args...)    printk(CPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define CPD_LOG(fmt, args...)    printk(CPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vddi_gpio;
	struct gpio_desc *vci_gpio;
	struct gpio_desc *ext_vdd_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	struct regulator *vio;

	bool prepared;
	bool enabled;

	int error;
};

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static struct mtk_panel_para_table init_setting_cmd[] = {
	{2, {0xB0, 0xA1}},
	{5, {0x2A, 0x00, 0x00, 0x04, 0x37}},
	{5, {0x2B, 0x00, 0x00, 0x09, 0x9B}},
	{2, {0x53, 0x00}},
	{5, {0x30, 0x00, 0x00, 0x09, 0x9B}},
	{5, {0x31, 0x00, 0x00, 0x04, 0x37}},
	{2, {0x35, 0x00}},
	{2, {0xB0, 0xCA}},
	{1, {0x11}},	//sleep out
	{REGFLAG_DELAY, {100}},
	{2, {0xB0, 0xA5}},
	{30, {0xCD, 0x10, 0x12, 0x01, 0x5A, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5D, 0x00, 0x01, 0x01, 0x00, 0x01,
         0x11, 0x40, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xFF, 0x70, 0xFF, 0x00}},
	{2, {0xB0, 0xA1}},
	{11, {0x55, 0x08, 0x00, 0x82, 0x8B, 0x1C, 0x61, 0x0A, 0x90, 0x04, 0x00}},
	{2, {0xB0, 0xA3}},
	{6, {0xB3, 0x1F, 0x63, 0x00, 0x00, 0x06}},

	{2, {0x53, 0x84}},
	{2, {0xB0, 0xA4}},
	{11, {0xB4, 0x00, 0x03, 0x05, 0xAF, 0x03, 0x05, 0xAF, 0x03, 0x05, 0xAF}},
	{28, {0xB5, 0x00, 0xFF, 0x94, 0x1F, 0xAE, 0x37, 0xAE, 0x1F, 0x94, 0x00, 0xFB, 0x94, 0x1E, 0xAD, 0x35, 0xAB,
		  0x1C, 0x90, 0x00, 0xDD, 0x8F, 0x16, 0x9F, 0x24, 0x97, 0x04, 0x75}},
	{13, {0xB7, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}},
	{13, {0xB8, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}},
	{13, {0xB9, 0x35, 0x2D, 0x2D, 0x37, 0x34, 0x35, 0x36, 0x36, 0x36, 0x2D, 0x35, 0x33}},
	{13, {0xBA, 0x80, 0x80, 0x80, 0x80, 0x88, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7D}},
	{13, {0xBB, 0x80, 0x5A, 0x7B, 0x95, 0x65, 0x70, 0x96, 0x90, 0x90, 0x90, 0x83, 0x72}},
	{2, {0xB0, 0xA1}},
	{6, {0xC2, 0xF3, 0x20, 0x00, 0x00, 0x1D}},
	{2, {0xB0, 0xA5}},
	{4, {0xD1, 0x0A, 0x00, 0x01}},
	{REGFLAG_DELAY, {20}},
	{4, {0xD1, 0x00, 0x00, 0x01}},//disable GCC/ECC
	{2, {0xB0, 0xA1}},
	{5, {0xBC, 0x20, 0x05, 0x0A, 0x00}},//Disable ECC_Mask
	{2, {0xB0, 0xA2}},
	{6, {0xB7, 0x30, 0x0F, 0x0F, 0x00, 0x10}},// ELVDD/ELVSS off timing to 0
#if defined(SUPPORT_VIDEO_MODE)
	{2, {0xB0, 0xA1}},
	{8, {0xB1, 0x47, 0x0A, 0x00, 0x00, 0x80, 0x00, 0x00}},
	{13, {0xB3, 0x91, 0x49, 0x00, 0x0F, 0x0C, 0x00, 0x10, 0x0C, 0x28, 0x00, 0x06, 0x14}},
	{5, {0xBC, 0x25, 0x05, 0x0A, 0x00}},
	{2, {0x3D, 0x10}},
	{2, {0xB0, 0xA2}},
	{32, {0xB3, 0x11, 0x02, 0x56, 0x40, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                       0x00, 0x10, 0x36, 0x40, 0x08, 0xFF, 0x44, 0x7A, 0x02, 0x7A, 0x02, 0x04,
                       0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00}},
#endif
	{1, {0x29}},//Display on
	{2, {0xB0, 0xCA}},
	{REGFLAG_DELAY, {20}},
};

static void push_table(struct lcm *ctx, struct mtk_panel_para_table *table, unsigned int count)
{
    int i;
    int size;

    for (i = 0; i < count; i++) {
		size = table[i].count;

		switch (size) {
			case REGFLAG_DELAY:
				mdelay(table[i].para_list[0]);
				break;
			default:
				lcm_dcs_write(ctx, table[i].para_list, table[i].count);
				break;
		}
    }
}

static void lcm_panel_init(struct lcm *ctx)
{
	push_table(ctx, init_setting_cmd, sizeof(init_setting_cmd) / sizeof(struct mtk_panel_para_table));
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);//set BL off
	_dev_info(ctx->dev,"[Display]set bl off in panel driver\n");
	ctx->enabled = false;
	CPD_LOG("finished\n");

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("[Display]%s\n", __func__);
	if (!ctx->prepared) {
		pr_info("[Display]%s: it's already unprepared\n", __func__);
		return 0;
	}
	lcm_dcs_write_seq_static(ctx, 0x13);
	mdelay(70);
	lcm_dcs_write_seq_static(ctx, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x10);
	mdelay(70);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	mdelay(1);
	CPD_LOG("finished\n");

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("[Display]%s\n", __func__);
	if (ctx->prepared) {
		pr_info("[Display]%s: it's already prepared\n", __func__);
		return 0;
	}
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(1);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(1);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(6);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	CPD_LOG("finished\n");

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	ctx->enabled = true;

	CPD_LOG("finished\n");

	return 0;
}

extern struct drm_crtc* lge_get_crtc(void);
static int lcm_panel_tx_cmd_set(struct drm_panel *panel, struct mtk_dsi *dsi, enum lge_ddic_dsi_cmd_set_type type)
{
	struct mtk_panel_ext *panel_ext = dsi->ext;
	struct dsi_cmd_desc *cmds;
	enum dsi_cmd_set_state state;
	const struct mipi_dsi_host_ops *ops = dsi->host.ops;
	int count = 0, i = 0;
	ssize_t len;

	struct drm_crtc *crtc = lge_get_crtc();
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	cmds = panel_ext->lge.lge_cmd_sets[type].cmds;
	count = panel_ext->lge.lge_cmd_sets[type].count;
	state = panel_ext->lge.lge_cmd_sets[type].state;

	if (count == 0) {
		CPD_ERR("[%s] No commands to be sent for state(%d)\n",
			 __func__, type);
		return -1;
	}

	if (!crtc) {
               DDPPR_ERR("find crtc fail\n");
               return -EINVAL;
	}

	mtk_crtc = to_mtk_crtc(crtc);

	if (!mtk_crtc->enabled) {
		CPD_ERR("crtc%d disable skip %s\n", drm_crtc_index(&mtk_crtc->base), __func__);
		return -EINVAL;
	} else if (mtk_crtc->ddp_mode == DDP_NO_USE) {
		CPD_ERR("skip %s, ddp_mode: NO_USE\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		if (state == DSI_CMD_SET_STATE_LP)
			cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;

		len = ops->transfer(&dsi->host, &cmds->msg);
		if (len < 0) {
			pr_err("failed to set cmds(%d), len=%d\n", type, len);
			return -1;
		}

		if (cmds->post_wait_ms) {
			usleep_range(cmds->post_wait_ms*1000,
					((cmds->post_wait_ms*1000)+10));
		}
		cmds++;
	}
	CPD_LOG("finished\n");

	return 0;
}

#if defined(SUPPORT_VIDEO_MODE)
static const struct drm_display_mode default_mode = {
	.clock = 168170,									// Pixel Clock = vtotal * htotal * vrefresh / 1000;
	.hdisplay = 1080,									// Frame Width
	.hsync_start = 1080 + 10,							// HFP
	.hsync_end = 1080 + 10 + 32,						// HSA
	.htotal = 1080 + 10 + 32 + 10,						// HBP
	.vdisplay = 2460,									// Frame Height
	.vsync_start = 2460 + 9,							// VFP
	.vsync_end = 2460 + 9 + 2,							// VSA
	.vtotal = 2460 + 9 + 2 + 5,							// VBP
	.vrefresh = 60,										// FPS
};
#else
static const struct drm_display_mode default_mode = {
	.clock = 179176,									// Pixel Clock = vtotal * htotal * vrefresh / 1000;
	.hdisplay = 1080,									// Frame Width
	.hsync_start = 1080 + 50,							// HFP
	.hsync_end = 1080 + 50 + 30,						// HSA
	.htotal = 1080 + 50 + 30 + 50,						// HBP
	.vdisplay = 2460,									// Frame Height
	.vsync_start = 2460 + 4,							// VFP
	.vsync_end = 2460 + 4 + 2,							// VSA
	.vtotal = 2460 + 4 + 2 + 2,							// VBP
	.vrefresh = 60,										// FPS
};
#endif

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3];
	unsigned char id[3] = {0x00, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0)
		pr_err("%s error\n", __func__);

	DDPINFO("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	DDPINFO("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0x00, 0x00};
#ifdef CONFIG_LGE_DISPLAY_COMMON
	struct mtk_dsi *p_dsi = (struct mtk_dsi *)dsi;
	struct mtk_panel_ext *panel = NULL;

	panel = p_dsi->ext;

	if(panel->lge.lp_state == LGE_PANEL_OFF){
		pr_err("[%s][sw43103] lcm is on power off state\n", __func__);
		return 0;
	}
#endif

	bl_tb0[1] = (level >> 8) & 0x07;
	bl_tb0[2] = level & 0xFF;

	CPD_LOG("bl_tb0[0] = 0x%x, bl_tb0[1] = 0x%x, level = %d\n", bl_tb0[1], bl_tb0[2], level);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int lcm_setbacklight_ex_cmdq(struct drm_panel *drm_panel, struct mtk_panel_ext *panel, unsigned int level)
{
	struct lcm *ctx = panel_to_lcm(drm_panel);
	char bl_tb_ex[] = {0x51, 0x00, 0x00};
	if(panel->lge.lp_state == LGE_PANEL_OFF){
		pr_err("[%s][sw43103] lcm is power off state\n", __func__);
		return 0;
	}
	bl_tb_ex[1] = (level >> 8) & 0x07;
	bl_tb_ex[2] = level & 0xFF;
	lcm_dcs_write(ctx, bl_tb_ex, ARRAY_SIZE(bl_tb_ex));
	CPD_LOG("bl_tb_ex[1] = 0x%x, bl_tb_ex[2] = 0x%x, level = %d\n", bl_tb_ex[1], bl_tb_ex[2], level);
	return 0;
}

static int lcm_panel_power_pre(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("[Display]%s\n", __func__);
	if (ctx->prepared) {
		pr_info("[Display]%s: it's already prepared\n", __func__);
		return 0;
	}

	ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddi_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vddi_gpio);
	mdelay(2);

	ctx->vci_gpio = devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vci_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vci_gpio);
	mdelay(2);

	ctx->ext_vdd_gpio = devm_gpiod_get(ctx->dev, "ext_vdd", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->ext_vdd_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->ext_vdd_gpio);
	mdelay(1);
	pr_info("%s\n", __func__);

	return 0;
}

static int lcm_panel_power_off_post(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("[Display]%s\n", __func__);
	if (!ctx->prepared) {
		pr_info("[Display]%s: it's already unprepared\n", __func__);
		return 0;
	}

	ctx->ext_vdd_gpio = devm_gpiod_get(ctx->dev, "ext_vdd", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->ext_vdd_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->ext_vdd_gpio);

	ctx->vci_gpio = devm_gpiod_get(ctx->dev, "vci", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vci_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vci_gpio);
	mdelay(5);

	ctx->vddi_gpio = devm_gpiod_get(ctx->dev, "vddi", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddi_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vddi_gpio);

	ctx->error = 0;
	ctx->prepared = false;
	pr_info("%s\n", __func__);

	return 0;
}

#ifdef CONFIG_LGE_DISPLAY_TUNING_SUPPORT
static struct mtk_panel_para_table * get_lcm_init_cmd_str(void)
{
	struct mtk_panel_para_table * ptable = NULL;

	ptable = init_setting_cmd;

	return ptable;
}

static int get_init_cmd_str_size(void)
{
	return sizeof(init_setting_cmd) / sizeof(struct mtk_panel_para_table);
}
#endif

static struct mtk_panel_params ext_params = {
#if defined(SUPPORT_VIDEO_MODE)
	.pll_clk = 540,
#else
	.pll_clk = 210,
#endif
	//.vfp_low_power = 750,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
#if defined(CONFIG_LGE_DISPLAY_RECOVERY)
	.chk_mode = READ_EINT,
#endif
#if !defined(SUPPORT_VIDEO_MODE)
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2460,
		.pic_width = 1080,
		.slice_height = 30,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 739,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 848,
		.slice_bpg_offset = 868,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
#endif
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
#ifdef CONFIG_LGE_DISPLAY_TUNING_SUPPORT
	.get_lcm_init_cmd_str = get_lcm_init_cmd_str,
	.get_init_cmd_str_size = get_init_cmd_str_size,
#endif
	.set_backlight_cmdq_ex_direct  = lcm_setbacklight_ex_cmdq,
	.panel_tx_cmd_set = lcm_panel_tx_cmd_set,
	.set_panel_power_pre = lcm_panel_power_pre,
	.set_panel_power_off_post = lcm_panel_power_off_post,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	panel->connector->display_info.width_mm = 69;				// Physical width
	panel->connector->display_info.height_mm = 158;				// Physical height

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	//struct drm_bridge *bridge;
	struct lcm *ctx;
	int ret;

	if(strcmp(lge_get_lcm_name(), "BOE-SW43103")) {
		pr_err("[%s][sw43103] is not matched with initalized lcm\n", __func__);
		return -EPERM;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
#if defined(SUPPORT_VIDEO_MODE)
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
#else
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
#endif
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;
/*
	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}
*/
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->vddi_gpio = devm_gpiod_get(dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_gpio)) {
		dev_err(dev, "cannot get vddi_gpio %ld\n",
			PTR_ERR(ctx->vddi_gpio));
		return PTR_ERR(ctx->vddi_gpio);
	}
	devm_gpiod_put(dev, ctx->vddi_gpio);

	ctx->vci_gpio = devm_gpiod_get(dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_gpio)) {
		dev_err(dev, "cannot get vci_gpio %ld\n",
			PTR_ERR(ctx->vci_gpio));
		return PTR_ERR(ctx->vci_gpio);
	}
	devm_gpiod_put(dev, ctx->vci_gpio);

	ctx->ext_vdd_gpio = devm_gpiod_get(dev, "ext_vdd", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->ext_vdd_gpio)) {
		dev_err(dev, "cannot get ext_vdd_gpio %ld\n",
			PTR_ERR(ctx->ext_vdd_gpio));
		return PTR_ERR(ctx->ext_vdd_gpio);
	}
	devm_gpiod_put(dev, ctx->ext_vdd_gpio);

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("[%s][sw43103] probe success\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "boe,sw43103,cmd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-boe-sw43103-cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Woonghwan Lee <woonghwan.lee@lge.com>");
MODULE_DESCRIPTION("BOE sw43103 CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");
