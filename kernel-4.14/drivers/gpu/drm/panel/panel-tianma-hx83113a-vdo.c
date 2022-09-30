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

//#include "lm36272.h"
#include "dw8768l.h"
#include "board_lge.h"
#include "mtk_boot_common.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "mtk_panel_ext.h"
#include "mtk_log.h"
#include "mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

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
	//struct backlight_device *backlight;
	struct gpio_desc *vio_ldo_gpio;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *dsv_en_gpio;
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
	{4, {0xB9, 0x83, 0x11, 0x3A}},
    {2, {0xBB, 0x03}},
    {12, {0xB1, 0x4C, 0x1F, 0x1F, 0x85, 0x80, 0xBC, 0x9E, 0x3C, 0x1E, 0x01, 0x01}},
    {17, {0xB2, 0x00, 0x33, 0x88, 0x90, 0x60, 0x00, 0x0A, 0x1E, 0x00, 0x04, 0x00, 0xD2, 0x11, 0x11, 0x00, 0x00}},
    {30, {0xB4, 0x9D, 0x28, 0x9D, 0x28, 0x9D, 0x28, 0x06, 0xCA, 0x06, 0xCA, 0x06, 0xCA, 0x06, 0xCA, 0x00, 0x00,
          0x08, 0x09, 0x00, 0x14, 0x00, 0x0D, 0x07, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x8D}},
    {2, {0xE9, 0xC8}},
    {2, {0xBF, 0x00}},
    {2, {0xE9, 0x3F}},
    {9, {0xC0, 0x23, 0x23, 0x44, 0xFF, 0x12, 0x1A, 0x00, 0x03}},
    {6, {0xC9, 0x00, 0x00, 0x09, 0x60, 0x00}},
    {8, {0xCB, 0x5F, 0x19, 0x10, 0x06, 0x00, 0xEE, 0x04}},
    {2, {0xCC, 0x0A}},
    {39, {0xD3, 0x81, 0x08, 0x01, 0x01, 0x00, 0x08, 0x01, 0x01, 0x07, 0x01, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
          0x20, 0x04, 0x32, 0x10, 0x08, 0x00, 0x08, 0x32, 0x10, 0x07, 0x00, 0x07, 0x32, 0x10, 0x07, 0x00,
          0x07, 0x00, 0x00, 0x0C, 0x09, 0x6C}},
    {49, {0xD5, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x2F, 0x2F, 0x30, 0x30,
          0x31, 0x31, 0x32, 0x32, 0x33, 0x33, 0x34, 0x34, 0x41, 0x41, 0x21, 0x20, 0x03, 0x02, 0x07, 0x06,
          0x01, 0x00, 0x05, 0x04, 0x18, 0x18, 0x41, 0x41, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x40, 0x40}},
    {49, {0xD6, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x2F, 0x2F, 0x30, 0x30,
          0x31, 0x31, 0x32, 0x32, 0x33, 0x33, 0x34, 0x34, 0x41, 0x41, 0x21, 0x20, 0x04, 0x05, 0x00, 0x01,
          0x06, 0x07, 0x02, 0x03, 0x41, 0x41, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x40, 0x40}},
    {37, {0xD8, 0xAA, 0xAF, 0xFF, 0xAA, 0xAA, 0xAB, 0xAA, 0xAF, 0xFF, 0xAA, 0xAA, 0xAB, 0xAA, 0xAF, 0xFF, 0xAF,
          0xFB, 0xAA, 0xAA, 0xAF, 0xFF, 0xAF, 0xFB, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
          0xAA, 0xAA, 0xAA, 0xAA}},
    {62, {0xDF, 0x07, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xA5, 0xE2, 0x4E, 0x74, 0x56, 0x2A, 0xEE, 0x44, 0x67,
		  0xA5, 0xE2, 0x4E, 0x74, 0x56, 0x2A, 0xEE, 0x44, 0x67, 0xA5, 0xE2, 0x4E, 0x74, 0x56, 0x2A, 0xEE,
          0x44, 0x67, 0xA5, 0xE2, 0x4E, 0x74, 0x56, 0x2A, 0xEE, 0x44, 0x67, 0xE1, 0xAC, 0x86, 0xB3, 0x1A,
          0xCE, 0x6A, 0x38, 0xAB, 0xE1, 0xAC, 0x86, 0xB3, 0x1A, 0xCE, 0x6A, 0x38, 0xAB}},
    {2, {0xE9, 0xCB}},
    {2, {0xE4, 0x00}},
    {2, {0xE9, 0x3F}},
    {26, {0xE7, 0x36, 0x2A, 0x08, 0x08, 0x25, 0x72, 0x24, 0x72, 0x00, 0xAA, 0x02, 0x02, 0x00, 0x00, 0x02, 0x02,
          0x12, 0x05, 0xFF, 0xFF, 0x24, 0x72, 0x00, 0xAA, 0x08}},
    {2, {0xE9, 0xDD}},
    {2, {0xE7, 0x17}},
    {2, {0xE9, 0x3F}},
    {2, {0xE9, 0xC4}},
    {3, {0xD1, 0x0C, 0xDD}},
    {2, {0xE9, 0x3F}},
    {2, {0xBD, 0x01}},
    {4, {0xB1, 0xB6, 0x11, 0x61}},
    {25, {0xD8, 0xAA, 0xA0, 0x00, 0x00, 0x00, 0xA0, 0xAA, 0xA0, 0x00, 0x00, 0x00, 0xA0, 0xAA, 0xA0, 0x00, 0x00,
          0x00, 0xA0, 0xAA, 0xA0, 0x00, 0x00, 0x00, 0xA0}},
    {9, {0xE7, 0x02, 0x70, 0x2C, 0x01, 0x2E, 0x07, 0x28, 0x08}},
    {2, {0xE9, 0xCC}},
    {2, {0xE7, 0x02}},
    {2, {0xE9, 0x3F}},
    {2, {0xBD, 0x02}},
    {3, {0xB1, 0x27, 0x27}},
    {4, {0xB4, 0x92, 0x11, 0x89}},
    {13, {0xD8, 0xAA, 0xAF, 0xFF, 0xEA, 0xAA, 0xAE, 0xAA, 0xAF, 0xFF, 0xEA, 0xAA, 0xAE}},
    {20, {0xE7, 0x02, 0x02, 0x02, 0x00, 0xCA, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x02, 0x20, 0x08, 0x40, 0x0B,
          0x0A, 0x00, 0x00}},
    {2, {0xE9, 0xE2}},
    {2, {0xE7, 0x18}},
    {2, {0xE9, 0x3F}},
    {2, {0xBD, 0x03}},
    {25, {0xD8, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAB, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAB, 0xAA, 0xAF, 0xFF, 0xEA,
          0xAA, 0xAE, 0xAA< 0xAF, 0xFF, 0xEA, 0xAA, 0xAE}},
    {2, {0xBD, 0x00}},
	{1, {0x11}},
	{REGFLAG_DELAY, {100}},
	{1, {0x29}},
};

static void push_table(struct lcm *ctx, struct mtk_panel_para_table *table, unsigned int count)
{
    int i;
    int size;

    for (i = 0; i < count; i++) {
		size = table[i].count;

		switch (size) {
			case REGFLAG_DELAY:
				msleep(table[i].para_list[0]);
				break;
			default:
				lcm_dcs_write(ctx, table[i].para_list, table[i].count);
				break;
		}
    }
}

static void chargepump_dsv_ctrl(int enable, int delay)
{
	if (enable)
		dw8768l_ctrl(ENABLE);
	else
		dw8768l_ctrl(DISABLE);

	CPD_LOG(" %s\n", (enable)? "ON":"OFF");
}

static void lcm_panel_init(struct lcm *ctx)
{
	ctx->vio_ldo_gpio = devm_gpiod_get(ctx->dev, "vio", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vio_ldo_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vio_ldo_gpio);

	msleep(3);

	ctx->dsv_en_gpio = devm_gpiod_get(ctx->dev, "dsv", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->dsv_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->dsv_en_gpio);
	msleep(5);
	chargepump_dsv_ctrl(ENABLE, 10);

	msleep(3);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	msleep(65);

	push_table(ctx, init_setting_cmd, sizeof(init_setting_cmd) / sizeof(struct mtk_panel_para_table));
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	ctx->enabled = false;

	CPD_LOG("finished\n");

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	unsigned int boot_mode = 0;

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(100);

	ctx->error = 0;
	ctx->prepared = false;

	boot_mode = get_boot_mode();

	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT || boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		CPD_LOG("shutdown power off\n");
		ctx->vio_ldo_gpio = devm_gpiod_get(ctx->dev, "vio", GPIOD_OUT_HIGH);
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		ctx->dsv_en_gpio = devm_gpiod_get(ctx->dev, "dsv", GPIOD_OUT_HIGH);

		gpiod_set_value(ctx->reset_gpio, 0);
		msleep(5);
		chargepump_dsv_ctrl(DISABLE, 0);
		gpiod_set_value(ctx->dsv_en_gpio, 0);
		gpiod_set_value(ctx->vio_ldo_gpio, 0);

		devm_gpiod_put(ctx->dev, ctx->vio_ldo_gpio);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		devm_gpiod_put(ctx->dev, ctx->dsv_en_gpio);
	}

	CPD_LOG("finished\n");
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

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

static const struct drm_display_mode default_mode = {
	.clock = 163357,									// Pixel Clock = vtotal * htotal * vrefresh / 1000;
	.hdisplay = 1080,									// Frame Width
	.hsync_start = 1080 + 22,							// HFP
	.hsync_end = 1080 + 22 + 6,							// HSA
	.htotal = 1080 + 22 + 6 + 6,						// HBP
	.vdisplay = 2400,									// Frame Height
	.vsync_start = 2400 + 32,							// VFP
	.vsync_end = 2400 + 32 + 6,							// VSA
	.vtotal = 2400 + 32 + 6 + 6,						// VBP
	.vrefresh = 60,										// FPS
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	CPD_LOG("finished\n");

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
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

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
	.pll_clk = 510,
	//.vfp_low_power = 750,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},

};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
#ifdef CONFIG_LGE_DISPLAY_TUNING_SUPPORT
	.get_lcm_init_cmd_str = get_lcm_init_cmd_str,
	.get_init_cmd_str_size = get_init_cmd_str_size,
#endif
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

	panel->connector->display_info.width_mm = 70;				// Physical width
	panel->connector->display_info.height_mm = 140;				// Physical height

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

	if(strcmp(lge_get_lcm_name(), "TIANMA-HX83113A")) {
		pr_err("[%s][hx83113a] is not matched with initalized lcm\n", __func__);
		return -EPERM;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ctx->vio_ldo_gpio = devm_gpiod_get(dev, "vio", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vio_ldo_gpio)) {
		dev_err(dev, "cannot get vio-gpios %ld\n",
			PTR_ERR(ctx->vio_ldo_gpio));
		return PTR_ERR(ctx->vio_ldo_gpio);
	}
	devm_gpiod_put(dev, ctx->vio_ldo_gpio);

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->dsv_en_gpio = devm_gpiod_get(dev, "dsv", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dsv_en_gpio)) {
		dev_err(dev, "cannot get dsv-gpios %ld\n",
			PTR_ERR(ctx->dsv_en_gpio));
		return PTR_ERR(ctx->dsv_en_gpio);
	}
	devm_gpiod_put(dev, ctx->dsv_en_gpio);

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

	pr_info("[%s][hx83113a] probe success\n", __func__);

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
	{ .compatible = "tianma,hx83113a,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-tianma-hx83113a-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Woonghwan Lee <woonghwan.lee@lge.com>");
MODULE_DESCRIPTION("TIANMA HX83113A VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
