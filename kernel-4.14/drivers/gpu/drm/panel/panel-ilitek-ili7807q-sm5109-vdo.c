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

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include <linux/input/lge_touch_notify.h>

#define DEBUG

struct lcm {
    struct device *dev;
    struct drm_panel panel;
    struct backlight_device *backlight;
    struct gpio_desc *reset_gpio;
    struct gpio_desc *bias_pos, *bias_neg;

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

    //pr_info("[LCM][KERNEL]%s\n", __func__);

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
    //pr_info("[LCM][KERNEL]%s is OK\n", __func__);
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
    struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
    ssize_t ret;

    pr_info("[LCM][KERNEL]%s\n", __func__);

    if (ctx->error < 0)
        return 0;

    ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
    if (ret < 0) {
        dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
        ctx->error = ret;
    }
    pr_info("[LCM][KERNEL]%s is OK\n", __func__);

    return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
    u8 buffer[3] = {0};
    static int ret;

    pr_info("[LCM][KERNEL]%s\n", __func__);

    if (ret == 0) {
        ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
        dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
             ret, buffer[0] | (buffer[1] << 8));
    }
}
#endif

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
static struct regulator *disp_bias_pos;
static struct regulator *disp_bias_neg;

static int lcm_panel_bias_regulator_init(void)
{
    static int regulator_inited;
    int ret = 0;

    pr_info("[LCM][KERNEL]%s\n", __func__);
    if (regulator_inited)
        return ret;
    pr_info("[LCM][KERNEL]%s start!\n", __func__);

    /* please only get regulator once in a driver */
    disp_bias_pos = regulator_get(NULL, "dsv_pos");
    if (IS_ERR(disp_bias_pos)) { /* handle return value */
        ret = PTR_ERR(disp_bias_pos);
        pr_err("get dsv_pos fail, error: %d\n", ret);
        return ret;
    }

    disp_bias_neg = regulator_get(NULL, "dsv_neg");
    if (IS_ERR(disp_bias_neg)) { /* handle return value */
        ret = PTR_ERR(disp_bias_neg);
        pr_err("get dsv_neg fail, error: %d\n", ret);
        return ret;
    }

    regulator_inited = 1;
    return ret; /* must be 0 */
}

static int lcm_panel_bias_enable(void)
{
    int ret = 0;
    int retval = 0;

    lcm_panel_bias_regulator_init();

    /* set voltage with min & max*/
    ret = regulator_set_voltage(disp_bias_pos, 5400000, 5400000);
    if (ret < 0)
        pr_err("set voltage disp_bias_pos fail, ret = %d\n", ret);
    retval |= ret;

    ret = regulator_set_voltage(disp_bias_neg, 5400000, 5400000);
    if (ret < 0)
        pr_err("set voltage disp_bias_neg fail, ret = %d\n", ret);
    retval |= ret;

    /* enable regulator */
    ret = regulator_enable(disp_bias_pos);
    if (ret < 0)
        pr_err("enable regulator disp_bias_pos fail, ret = %d\n", ret);
    retval |= ret;

    ret = regulator_enable(disp_bias_neg);
    if (ret < 0)
        pr_err("enable regulator disp_bias_neg fail, ret = %d\n", ret);
    retval |= ret;

    return retval;
}

static int lcm_panel_bias_disable(void)
{
    int ret = 0;
    int retval = 0;

    lcm_panel_bias_regulator_init();

    ret = regulator_disable(disp_bias_neg);
    if (ret < 0)
        pr_err("disable regulator disp_bias_neg fail, ret = %d\n", ret);
    retval |= ret;

    ret = regulator_disable(disp_bias_pos);
    if (ret < 0)
        pr_err("disable regulator disp_bias_pos fail, ret = %d\n", ret);
    retval |= ret;

    return retval;
}
#endif

static void lcm_panel_init(struct lcm *ctx)
{
    pr_info("[LCM][KERNEL]%s\n", __func__);

    ctx->reset_gpio =
        devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->reset_gpio)) {
        dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
            __func__, PTR_ERR(ctx->reset_gpio));
        return;
    }
    gpiod_set_value(ctx->reset_gpio, 0);
    udelay(15 * 1000);
    gpiod_set_value(ctx->reset_gpio, 1);
    udelay(1 * 1000);
    gpiod_set_value(ctx->reset_gpio, 0);
    udelay(10 * 1000);
    gpiod_set_value(ctx->reset_gpio, 1);
    udelay(10 * 1000);
    devm_gpiod_put(ctx->dev, ctx->reset_gpio);

    lcm_dcs_write_seq_static(ctx,0XFF,0X78,0X07,0X01);
    lcm_dcs_write_seq_static(ctx,0X00,0X63);
    lcm_dcs_write_seq_static(ctx,0X02,0X34);    //20191023
    lcm_dcs_write_seq_static(ctx,0X03,0X1f);    //20191023
    lcm_dcs_write_seq_static(ctx,0X04,0X23);
    lcm_dcs_write_seq_static(ctx,0X05,0X42);
    lcm_dcs_write_seq_static(ctx,0X06,0X00);    //20200304
    lcm_dcs_write_seq_static(ctx,0X07,0X00);    //20191023
    lcm_dcs_write_seq_static(ctx,0X08,0XA3);
    lcm_dcs_write_seq_static(ctx,0X09,0X04);
    lcm_dcs_write_seq_static(ctx,0X0A,0X30);
    lcm_dcs_write_seq_static(ctx,0X0B,0X01);
    lcm_dcs_write_seq_static(ctx,0X0C,0X00);
    lcm_dcs_write_seq_static(ctx,0X0E,0X5A);
    lcm_dcs_write_seq_static(ctx,0X01,0XE2);
    lcm_dcs_write_seq_static(ctx,0X31,0X07);    //FGOUTR01
    lcm_dcs_write_seq_static(ctx,0X32,0X07);    //FGOUTR02
    lcm_dcs_write_seq_static(ctx,0X33,0X07);    //FGOUTR03
    lcm_dcs_write_seq_static(ctx,0X34,0X3D);    //FGOUTR04    TP_SW
    lcm_dcs_write_seq_static(ctx,0X35,0X3D);    //FGOUTR05    TP_SW
    lcm_dcs_write_seq_static(ctx,0X36,0X02);    //FGOUTR06    CT_SW / CT_CTRL
    lcm_dcs_write_seq_static(ctx,0X37,0X2E);    //FGOUTR07    MUXR1
    lcm_dcs_write_seq_static(ctx,0X38,0X2F);    //FGOUTR08    MUXG1
    lcm_dcs_write_seq_static(ctx,0X39,0X30);    //FGOUTR09    MUXB1
    lcm_dcs_write_seq_static(ctx,0X3A,0X31);    //FGOUTR10    MUXR2
    lcm_dcs_write_seq_static(ctx,0X3B,0X32);    //FGOUTR11    MUXG2
    lcm_dcs_write_seq_static(ctx,0X3C,0X33);    //FGOUTR12    MUXB2
    lcm_dcs_write_seq_static(ctx,0X3D,0X01);    //FGOUTR13    U2D
    lcm_dcs_write_seq_static(ctx,0X3E,0X00);    //FGOUTR14    D2U
    lcm_dcs_write_seq_static(ctx,0X3F,0X11);    //FGOUTR15    CLK_O
    lcm_dcs_write_seq_static(ctx,0X40,0X11);    //FGOUTR16    CLK_O
    lcm_dcs_write_seq_static(ctx,0X41,0X13);    //FGOUTR17    XCLK_O
    lcm_dcs_write_seq_static(ctx,0X42,0X13);    //FGOUTR18    XCLK_O
    lcm_dcs_write_seq_static(ctx,0X43,0X40);    //FGOUTR19    GOFF
    lcm_dcs_write_seq_static(ctx,0X44,0X08);    //FGOUTR20    STV
    lcm_dcs_write_seq_static(ctx,0X45,0X0C);    //FGOUTR21    VEND
    lcm_dcs_write_seq_static(ctx,0X46,0X2C);    //FGOUTR22    RST
    lcm_dcs_write_seq_static(ctx,0X47,0X28);    //FGOUTR23    XDONB
    lcm_dcs_write_seq_static(ctx,0X48,0X28);    //FGOUTR24    XDONB
    lcm_dcs_write_seq_static(ctx,0X49,0X07);    //FGOUTL01
    lcm_dcs_write_seq_static(ctx,0X4A,0X07);    //FGOUTL02
    lcm_dcs_write_seq_static(ctx,0X4B,0X07);    //FGOUTL03
    lcm_dcs_write_seq_static(ctx,0X4C,0X3D);    //FGOUTL04    TP_SW
    lcm_dcs_write_seq_static(ctx,0X4D,0X3D);    //FGOUTL05    TP_SW
    lcm_dcs_write_seq_static(ctx,0X4E,0X02);    //FGOUTL06    CT_SW / CT_CTRL
    lcm_dcs_write_seq_static(ctx,0X4F,0X2E);    //FGOUTL07    MUXR1
    lcm_dcs_write_seq_static(ctx,0X50,0X2F);    //FGOUTL08    MUXG1
    lcm_dcs_write_seq_static(ctx,0X51,0X30);    //FGOUTL09    MUXB1
    lcm_dcs_write_seq_static(ctx,0X52,0X31);    //FGOUTL10    MUXR2
    lcm_dcs_write_seq_static(ctx,0X53,0X32);    //FGOUTL11    MUXG2
    lcm_dcs_write_seq_static(ctx,0X54,0X33);    //FGOUTL12    MUXB2
    lcm_dcs_write_seq_static(ctx,0X55,0X01);    //FGOUTL13    U2D
    lcm_dcs_write_seq_static(ctx,0X56,0X00);    //FGOUTL14    D2U
    lcm_dcs_write_seq_static(ctx,0X57,0X10);    //FGOUTL15    CLK_O
    lcm_dcs_write_seq_static(ctx,0X58,0X10);    //FGOUTL16    CLK_O
    lcm_dcs_write_seq_static(ctx,0X59,0X12);    //FGOUTL17    XCLK_O
    lcm_dcs_write_seq_static(ctx,0X5A,0X12);    //FGOUTL18    XCLK_O
    lcm_dcs_write_seq_static(ctx,0X5B,0X40);    //FGOUTL19    GOFF
    lcm_dcs_write_seq_static(ctx,0X5C,0X08);    //FGOUTL20    STV
    lcm_dcs_write_seq_static(ctx,0X5D,0X0C);    //FGOUTL21    VEND
    lcm_dcs_write_seq_static(ctx,0X5E,0X2C);    //FGOUTL22    RST
    lcm_dcs_write_seq_static(ctx,0X5F,0X28);    //FGOUTL23    XDONB
    lcm_dcs_write_seq_static(ctx,0X60,0X28);    //FGOUTL24    XDONB
    lcm_dcs_write_seq_static(ctx,0X61,0X07);    //BGOUTR01
    lcm_dcs_write_seq_static(ctx,0X62,0X07);    //BGOUTR02
    lcm_dcs_write_seq_static(ctx,0X63,0X07);    //BGOUTR03
    lcm_dcs_write_seq_static(ctx,0X64,0X3D);    //BGOUTR04   TP_SW
    lcm_dcs_write_seq_static(ctx,0X65,0X3D);    //BGOUTR05   TP_SW
    lcm_dcs_write_seq_static(ctx,0X66,0X28);    //BGOUTR06   CT_SW
    lcm_dcs_write_seq_static(ctx,0X67,0X2E);    //BGOUTR07   MUXR1
    lcm_dcs_write_seq_static(ctx,0X68,0X2F);    //BGOUTR08   MUXG1
    lcm_dcs_write_seq_static(ctx,0X69,0X30);    //BGOUTR09   MUXB1
    lcm_dcs_write_seq_static(ctx,0X6A,0X31);    //BGOUTR10   MUXR2
    lcm_dcs_write_seq_static(ctx,0X6B,0X32);    //BGOUTR11   MUXG2
    lcm_dcs_write_seq_static(ctx,0X6C,0X33);    //BGOUTR12   MUXB2
    lcm_dcs_write_seq_static(ctx,0X6D,0X01);    //BGOUTR13   U2D
    lcm_dcs_write_seq_static(ctx,0X6E,0X00);    //BGOUTR14   D2U
    lcm_dcs_write_seq_static(ctx,0X6F,0X12);    //BGOUTR15   XCLK_O
    lcm_dcs_write_seq_static(ctx,0X70,0X12);    //BGOUTR16   XCLK_O
    lcm_dcs_write_seq_static(ctx,0X71,0X10);    //BGOUTR17   CLK_O
    lcm_dcs_write_seq_static(ctx,0X72,0X10);    //BGOUTR18   CLK_O
    lcm_dcs_write_seq_static(ctx,0X73,0X3C);    //BGOUTR19   GOFF
    lcm_dcs_write_seq_static(ctx,0X74,0X0C);    //BGOUTR20   VEND
    lcm_dcs_write_seq_static(ctx,0X75,0X08);    //BGOUTR21   STV
    lcm_dcs_write_seq_static(ctx,0X76,0X2C);    //BGOUTR22   RST
    lcm_dcs_write_seq_static(ctx,0X77,0X28);    //BGOUTR23   XDONB
    lcm_dcs_write_seq_static(ctx,0X78,0X28);    //BGOUTR24   XDONB
    lcm_dcs_write_seq_static(ctx,0X79,0X07);    //BGOUTL01
    lcm_dcs_write_seq_static(ctx,0X7A,0X07);    //BGOUTL02
    lcm_dcs_write_seq_static(ctx,0X7B,0X07);    //BGOUTL03
    lcm_dcs_write_seq_static(ctx,0X7C,0X3D);    //BGOUTL04   TP_SW
    lcm_dcs_write_seq_static(ctx,0X7D,0X3D);    //BGOUTL05   TP_SW
    lcm_dcs_write_seq_static(ctx,0X7E,0X28);    //BGOUTL06   CT_SW
    lcm_dcs_write_seq_static(ctx,0X7F,0X2E);    //BGOUTL07   MUXR1
    lcm_dcs_write_seq_static(ctx,0X80,0X2F);    //BGOUTL08   MUXG1
    lcm_dcs_write_seq_static(ctx,0X81,0X30);    //BGOUTL09   MUXB1
    lcm_dcs_write_seq_static(ctx,0X82,0X31);    //BGOUTL10   MUXR2
    lcm_dcs_write_seq_static(ctx,0X83,0X32);    //BGOUTL11   MUXG2
    lcm_dcs_write_seq_static(ctx,0X84,0X33);    //BGOUTL12   MUXB2
    lcm_dcs_write_seq_static(ctx,0X85,0X01);    //BGOUTL13   U2D
    lcm_dcs_write_seq_static(ctx,0X86,0X00);    //BGOUTL14   D2U
    lcm_dcs_write_seq_static(ctx,0X87,0X13);    //BGOUTL15   XCLK_O
    lcm_dcs_write_seq_static(ctx,0X88,0X13);    //BGOUTL16   XCLK_O
    lcm_dcs_write_seq_static(ctx,0X89,0X11);    //BGOUTL17   CLK_O
    lcm_dcs_write_seq_static(ctx,0X8A,0X11);    //BGOUTL18   CLK_O
    lcm_dcs_write_seq_static(ctx,0X8B,0X3C);    //BGOUTL19   GOFF
    lcm_dcs_write_seq_static(ctx,0X8C,0X0C);    //BGOUTL20   VEND
    lcm_dcs_write_seq_static(ctx,0X8D,0X08);    //BGOUTL21   STV
    lcm_dcs_write_seq_static(ctx,0X8E,0X2C);    //BGOUTL22   RST
    lcm_dcs_write_seq_static(ctx,0X8F,0X28);    //BGOUTL23   XDONB
    lcm_dcs_write_seq_static(ctx,0X90,0X28);    //BGOUTL24   XDONB
    lcm_dcs_write_seq_static(ctx,0X91,0XE1);
    lcm_dcs_write_seq_static(ctx,0X92,0X19);
    lcm_dcs_write_seq_static(ctx,0X93,0X08);
    lcm_dcs_write_seq_static(ctx,0X94,0X00);
    lcm_dcs_write_seq_static(ctx,0X95,0X21);
    lcm_dcs_write_seq_static(ctx,0X96,0X19);
    lcm_dcs_write_seq_static(ctx,0X97,0X08);
    lcm_dcs_write_seq_static(ctx,0X98,0X00);
    lcm_dcs_write_seq_static(ctx,0XA0,0X83);
    lcm_dcs_write_seq_static(ctx,0XA1,0X44);
    lcm_dcs_write_seq_static(ctx,0XA2,0X83);
    lcm_dcs_write_seq_static(ctx,0XA3,0X44);
    lcm_dcs_write_seq_static(ctx,0XA4,0X61);
    lcm_dcs_write_seq_static(ctx,0XA5,0X00);    //20200304
    lcm_dcs_write_seq_static(ctx,0XA6,0X15);
    lcm_dcs_write_seq_static(ctx,0XA7,0X50);
    lcm_dcs_write_seq_static(ctx,0XA8,0X1A);
    lcm_dcs_write_seq_static(ctx,0XAE,0X00);
    lcm_dcs_write_seq_static(ctx,0XB0,0X00);
    lcm_dcs_write_seq_static(ctx,0XB1,0X00);
    lcm_dcs_write_seq_static(ctx,0XB2,0X02);
    lcm_dcs_write_seq_static(ctx,0XB3,0X00);
    lcm_dcs_write_seq_static(ctx,0XB4,0X02);
    lcm_dcs_write_seq_static(ctx,0XC1,0X60);
    lcm_dcs_write_seq_static(ctx,0XC2,0X60);
    lcm_dcs_write_seq_static(ctx,0XC5,0X29);
    lcm_dcs_write_seq_static(ctx,0XC6,0X20);
    lcm_dcs_write_seq_static(ctx,0XC7,0X20);
    lcm_dcs_write_seq_static(ctx,0XC8,0X1F);
    lcm_dcs_write_seq_static(ctx,0XC9,0X00);    //20191023
    lcm_dcs_write_seq_static(ctx,0XCA,0X01);
    lcm_dcs_write_seq_static(ctx,0XD1,0X11);
    lcm_dcs_write_seq_static(ctx,0XD2,0X00);
    lcm_dcs_write_seq_static(ctx,0XD3,0X01);
    lcm_dcs_write_seq_static(ctx,0XD4,0X00);
    lcm_dcs_write_seq_static(ctx,0XD5,0X00);
    lcm_dcs_write_seq_static(ctx,0XD6,0X3D);
    lcm_dcs_write_seq_static(ctx,0XD7,0X00);
    lcm_dcs_write_seq_static(ctx,0XD8,0X01);
    lcm_dcs_write_seq_static(ctx,0XD9,0X54);
    lcm_dcs_write_seq_static(ctx,0XDA,0X00);
    lcm_dcs_write_seq_static(ctx,0XDB,0X00);
    lcm_dcs_write_seq_static(ctx,0XDC,0X00);
    lcm_dcs_write_seq_static(ctx,0XDD,0X00);
    lcm_dcs_write_seq_static(ctx,0XDE,0X00);
    lcm_dcs_write_seq_static(ctx,0XDF,0X00);
    lcm_dcs_write_seq_static(ctx,0XE0,0X00);
    lcm_dcs_write_seq_static(ctx,0XE1,0X04);    //20191025
    lcm_dcs_write_seq_static(ctx,0XE2,0X00);
    lcm_dcs_write_seq_static(ctx,0XE3,0X1B);
    lcm_dcs_write_seq_static(ctx,0XE4,0X52);
    lcm_dcs_write_seq_static(ctx,0XE5,0X4B);    //20200819  49
    lcm_dcs_write_seq_static(ctx,0XE6,0X44);    //20191025
    lcm_dcs_write_seq_static(ctx,0XE7,0X00);
    lcm_dcs_write_seq_static(ctx,0XE8,0X01);
    lcm_dcs_write_seq_static(ctx,0XED,0X55);
    lcm_dcs_write_seq_static(ctx,0XEF,0X30);
    lcm_dcs_write_seq_static(ctx,0XF0,0X00);
    lcm_dcs_write_seq_static(ctx,0XF4,0X54);
    // SRC PART
    lcm_dcs_write_seq_static(ctx,0XFF,0X78,0X07,0X02);
    lcm_dcs_write_seq_static(ctx,0X01,0X7D);    //20191023
    lcm_dcs_write_seq_static(ctx,0X02,0X08);    //mipi time out 256us
    lcm_dcs_write_seq_static(ctx,0X06,0X6B);    //BIST FRAME 60 hz
    lcm_dcs_write_seq_static(ctx,0X08,0X00);    //BIST FRAME //20191025
    lcm_dcs_write_seq_static(ctx,0X0E,0X14);
    lcm_dcs_write_seq_static(ctx,0X0F,0X34);
    lcm_dcs_write_seq_static(ctx,0X40,0X0F);    //t8_de 0.5us
    lcm_dcs_write_seq_static(ctx,0X41,0X00);
    lcm_dcs_write_seq_static(ctx,0X42,0X09);
    lcm_dcs_write_seq_static(ctx,0X43,0X12);    //ckh width 600ns
    lcm_dcs_write_seq_static(ctx,0X46,0X32);
    lcm_dcs_write_seq_static(ctx,0X4D,0X02);    //RRGGBB
    lcm_dcs_write_seq_static(ctx,0X47,0X00);    //CKH connect
    lcm_dcs_write_seq_static(ctx,0X53,0X09);
    lcm_dcs_write_seq_static(ctx,0X54,0X05);
    lcm_dcs_write_seq_static(ctx,0X56,0X02);
    lcm_dcs_write_seq_static(ctx,0X5D,0X07);
    lcm_dcs_write_seq_static(ctx,0X5E,0XC0);
    lcm_dcs_write_seq_static(ctx,0X80,0X3F);    // 20200722
    lcm_dcs_write_seq_static(ctx,0XF4,0X00);
    lcm_dcs_write_seq_static(ctx,0XF5,0X00);
    lcm_dcs_write_seq_static(ctx,0XF6,0X00);
    lcm_dcs_write_seq_static(ctx,0XF7,0X00);
    lcm_dcs_write_seq_static(ctx,0XFF,0X78,0X07,0X04);
    lcm_dcs_write_seq_static(ctx,0XB7,0X45);
    lcm_dcs_write_seq_static(ctx,0XFF,0X78,0X07,0X05); // SWITCH TO PAGE 5 //REGULATOR
    lcm_dcs_write_seq_static(ctx,0X5B,0X7E);    //GVDDP +5V
    lcm_dcs_write_seq_static(ctx,0X5C,0X7E);    //GVDDP -5V
    lcm_dcs_write_seq_static(ctx,0X3C,0X00);    //FOLLOW ABNORMAL SEQ 01:TM SLP IN SEQ
    lcm_dcs_write_seq_static(ctx,0X52,0X60);    //VGH 9.5V
    lcm_dcs_write_seq_static(ctx,0X56,0X5B);    //VGHO 8.5V
    lcm_dcs_write_seq_static(ctx,0X5A,0X54);    //VGLO -8.15V
    lcm_dcs_write_seq_static(ctx,0X54,0X56);    //VGL -9V
    lcm_dcs_write_seq_static(ctx,0X02,0X00);    //VCOM -0.2V
    lcm_dcs_write_seq_static(ctx,0X03,0X84);
    lcm_dcs_write_seq_static(ctx,0X44,0XEF);    //VGH X2  //FF VGH X3
    lcm_dcs_write_seq_static(ctx,0X4E,0X3F);    //VGL X2  //7F VGL X3
    lcm_dcs_write_seq_static(ctx,0X20,0X13);    //20191023
    lcm_dcs_write_seq_static(ctx,0X2A,0X13);    //20191023
    lcm_dcs_write_seq_static(ctx,0X2B,0X08);
    lcm_dcs_write_seq_static(ctx,0X23,0XF7);    //20191025
    lcm_dcs_write_seq_static(ctx,0X2D,0X54);    //20191025 VGHO
    lcm_dcs_write_seq_static(ctx,0X2E,0X74);    //20191025 VGLO
    lcm_dcs_write_seq_static(ctx,0XB5,0X54);    //20191025
    lcm_dcs_write_seq_static(ctx,0XB7,0X74);    //20191025
    lcm_dcs_write_seq_static(ctx,0XFF,0X78,0X07,0X06);
    lcm_dcs_write_seq_static(ctx,0X0A,0X0C);    //NL_SEL = 1 NL[10:8] = 4  2+(2NL) = 2400
    lcm_dcs_write_seq_static(ctx,0X0B,0XAF);    //NL[7:0]  = 91
    lcm_dcs_write_seq_static(ctx,0X0E,0X06);    //SS
    lcm_dcs_write_seq_static(ctx,0XD6,0x55);
    lcm_dcs_write_seq_static(ctx,0XCD,0X68);
    lcm_dcs_write_seq_static(ctx,0XFF,0X78,0X07,0X08);
    lcm_dcs_write_seq_static(ctx,0XE0,0X00,0X00,0X1A,0X44,0X00,0X67,0X81,0X9A,0X00,0XB0,0XC3,
                                 0XD5,0X15,0X0E,0X3B,0X7E,0X25,0XAE,0XFA,0X34,0X2A,0X36,0X6E,
                                 0XAF,0X3E,0XD7,0X0A,0X30,0X3F,0X55,0X63,0X71,0X3F,0X7E,0X91,
                                 0XA2,0X3F,0XBD,0XD8,0XD9);
    lcm_dcs_write_seq_static(ctx,0XE1,0X00,0X00,0X1A,0X44,0X00,0X67,0X81,0X9A,0X00,0XB0,0XC3,
                                 0XD5,0X15,0X0E,0X3B,0X7E,0X25,0XAE,0XFA,0X34,0X2A,0X36,0X6E,
                                 0XAF,0X3E,0XD7,0X0A,0X30,0X3F,0X55,0X63,0X71,0X3F,0X7E,0X91,
                                 0XA2,0X3F,0XBD,0XD8,0XD9);
    //************AUTO TRIM********************
    lcm_dcs_write_seq_static(ctx,0XFF,0X78,0X07,0X0B);
    lcm_dcs_write_seq_static(ctx,0X94,0X88);
    lcm_dcs_write_seq_static(ctx,0X95,0X22);
    lcm_dcs_write_seq_static(ctx,0X96,0X06);
    lcm_dcs_write_seq_static(ctx,0X97,0X06);
    lcm_dcs_write_seq_static(ctx,0X98,0XCB);
    lcm_dcs_write_seq_static(ctx,0X99,0XCB);
    lcm_dcs_write_seq_static(ctx,0X9A,0X06);
    lcm_dcs_write_seq_static(ctx,0X9B,0XCD);
    lcm_dcs_write_seq_static(ctx,0X9C,0X05);
    lcm_dcs_write_seq_static(ctx,0X9D,0X05);
    lcm_dcs_write_seq_static(ctx,0X9E,0XAA);
    lcm_dcs_write_seq_static(ctx,0X9F,0XAA);
    lcm_dcs_write_seq_static(ctx,0XAB,0XF0);
    //************LONG H TIMING SETTING********************
    lcm_dcs_write_seq_static(ctx,0XFF,0X78,0X07,0X0E);
    lcm_dcs_write_seq_static(ctx,0X00,0XA3);    //LH MODE
    lcm_dcs_write_seq_static(ctx,0X02,0X12);    //VBP
    lcm_dcs_write_seq_static(ctx,0X40,0X07);    //8 UNIT
    lcm_dcs_write_seq_static(ctx,0X49,0X2C);    //UNIT LINE[7:0] = 300
    lcm_dcs_write_seq_static(ctx,0X47,0X90);    //UNIT LINE[7:0] = 300
    lcm_dcs_write_seq_static(ctx,0X45,0X0A);    //TP TERM2_UNIT 0 = 170 US
    lcm_dcs_write_seq_static(ctx,0X46,0XFF);
    lcm_dcs_write_seq_static(ctx,0X4D,0XC4);    //RTN = 6.15 US
    lcm_dcs_write_seq_static(ctx,0X51,0X00);
    lcm_dcs_write_seq_static(ctx,0XB0,0X01);    //term1 number
    lcm_dcs_write_seq_static(ctx,0XB1,0X62);
    lcm_dcs_write_seq_static(ctx,0XB3,0X00);
    lcm_dcs_write_seq_static(ctx,0XB4,0X33);
    lcm_dcs_write_seq_static(ctx,0XBC,0X04);
    lcm_dcs_write_seq_static(ctx,0XBD,0XFC);
    lcm_dcs_write_seq_static(ctx,0XC0,0X63);    //term3 number
    lcm_dcs_write_seq_static(ctx,0XC7,0X62);
    lcm_dcs_write_seq_static(ctx,0XC8,0X62);
    lcm_dcs_write_seq_static(ctx,0XC9,0X62);
    lcm_dcs_write_seq_static(ctx,0XD2,0X00);
    lcm_dcs_write_seq_static(ctx,0XD3,0XA8);
    lcm_dcs_write_seq_static(ctx,0XD4,0X77);    //FF
    lcm_dcs_write_seq_static(ctx,0XD5,0XA8);
    lcm_dcs_write_seq_static(ctx,0XD6,0X00);
    lcm_dcs_write_seq_static(ctx,0XD7,0XA8);
    lcm_dcs_write_seq_static(ctx,0XD8,0X00);
    lcm_dcs_write_seq_static(ctx,0XD9,0XA8);
    lcm_dcs_write_seq_static(ctx,0XE0,0X00);
    lcm_dcs_write_seq_static(ctx,0XE1,0X00);
    lcm_dcs_write_seq_static(ctx,0XE2,0X09);
    lcm_dcs_write_seq_static(ctx,0XE3,0X17);
    lcm_dcs_write_seq_static(ctx,0XE4,0X04);
    lcm_dcs_write_seq_static(ctx,0XE5,0X04);
    lcm_dcs_write_seq_static(ctx,0XE6,0X00);
    lcm_dcs_write_seq_static(ctx,0XE7,0X05);
    lcm_dcs_write_seq_static(ctx,0XE8,0X00);
    lcm_dcs_write_seq_static(ctx,0XE9,0X02);
    lcm_dcs_write_seq_static(ctx,0XEA,0X07);
    lcm_dcs_write_seq_static(ctx,0X07,0X21);
    lcm_dcs_write_seq_static(ctx,0X4B,0X09);
    //***********TP modulation  ***************************
    lcm_dcs_write_seq_static(ctx,0XFF,0X78,0x07,0x0C);
    lcm_dcs_write_seq_static(ctx,0X00,0X11);
    lcm_dcs_write_seq_static(ctx,0X01,0X1E);
    lcm_dcs_write_seq_static(ctx,0X02,0X12);
    lcm_dcs_write_seq_static(ctx,0X03,0X21);
    lcm_dcs_write_seq_static(ctx,0X04,0X11);
    lcm_dcs_write_seq_static(ctx,0X05,0X16);
    lcm_dcs_write_seq_static(ctx,0X06,0X12);
    lcm_dcs_write_seq_static(ctx,0X07,0X26);
    lcm_dcs_write_seq_static(ctx,0X08,0X12);
    lcm_dcs_write_seq_static(ctx,0X09,0X27);
    lcm_dcs_write_seq_static(ctx,0X0A,0X11);
    lcm_dcs_write_seq_static(ctx,0X0B,0X1C);
    lcm_dcs_write_seq_static(ctx,0X0C,0X11);
    lcm_dcs_write_seq_static(ctx,0X0D,0X18);
    lcm_dcs_write_seq_static(ctx,0X0E,0X11);
    lcm_dcs_write_seq_static(ctx,0X0F,0X18);
    lcm_dcs_write_seq_static(ctx,0X10,0X11);
    lcm_dcs_write_seq_static(ctx,0X11,0X19);
    lcm_dcs_write_seq_static(ctx,0X12,0X11);
    lcm_dcs_write_seq_static(ctx,0X13,0X15);
    lcm_dcs_write_seq_static(ctx,0X14,0X11);
    lcm_dcs_write_seq_static(ctx,0X15,0X1F);
    lcm_dcs_write_seq_static(ctx,0X16,0X11);
    lcm_dcs_write_seq_static(ctx,0X17,0X1B);
    lcm_dcs_write_seq_static(ctx,0X18,0X11);
    lcm_dcs_write_seq_static(ctx,0X19,0X1B);
    lcm_dcs_write_seq_static(ctx,0X1A,0X11);
    lcm_dcs_write_seq_static(ctx,0X1B,0X17);
    lcm_dcs_write_seq_static(ctx,0X1C,0X11);
    lcm_dcs_write_seq_static(ctx,0X1D,0X1D);
    lcm_dcs_write_seq_static(ctx,0X1E,0X11);
    lcm_dcs_write_seq_static(ctx,0X1F,0X1A);
    lcm_dcs_write_seq_static(ctx,0X20,0X11);
    lcm_dcs_write_seq_static(ctx,0X21,0X14);
    lcm_dcs_write_seq_static(ctx,0X22,0X11);
    lcm_dcs_write_seq_static(ctx,0X23,0X14);
    lcm_dcs_write_seq_static(ctx,0X24,0X12);
    lcm_dcs_write_seq_static(ctx,0X25,0X23);
    lcm_dcs_write_seq_static(ctx,0X26,0X11);
    lcm_dcs_write_seq_static(ctx,0X27,0X20);
    lcm_dcs_write_seq_static(ctx,0X28,0X12);
    lcm_dcs_write_seq_static(ctx,0X29,0X24);
    lcm_dcs_write_seq_static(ctx,0XFF,0X78,0X07,0X00);
    lcm_dcs_write_seq_static(ctx,0X11,0X00);    //SLEEP OUT
    msleep(80);
    lcm_dcs_write_seq_static(ctx,0X29,0X00);    //DISPLAY ON
    msleep(20);
}

static int lcm_disable(struct drm_panel *panel)
{
    struct lcm *ctx = panel_to_lcm(panel);
    pr_info("%s\n", __func__);
    if (!ctx->enabled)
        return 0;

    pr_info("[LCM][KERNEL]%s start!\n", __func__);
    if (ctx->backlight) {
        ctx->backlight->props.power = FB_BLANK_POWERDOWN;
        backlight_update_status(ctx->backlight);
        pr_info("[LCM][KERNEL]FB_BLANK_POWERDOWN!\n");
    }

    ctx->enabled = false;
    pr_info("[LCM][KERNEL]%s end!\n", __func__);
    return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
    struct lcm *ctx = panel_to_lcm(panel);
    pr_info("%s\n", __func__);
    if (!ctx->prepared)
        return 0;

    pr_info("[LCM][KERNEL][unprepare]%s start!\n", __func__);
    lcm_dcs_write_seq_static(ctx, 0x28);
    msleep(50);
    lcm_dcs_write_seq_static(ctx, 0x10);
    msleep(120);

    ctx->error = 0;
    ctx->prepared = false;
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
    lcm_panel_bias_disable();
#endif

    {
        int mode = LCD_EVENT_LCD_MODE_U0;
        touch_notifier_call_chain(LCD_EVENT_LCD_MODE, (void*)&mode);
    }
    return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
    struct lcm *ctx = panel_to_lcm(panel);
    int ret;

    pr_info("[LCM][KERNEL][prepare]%s\n", __func__);
    if (ctx->prepared)
        return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
    lcm_panel_bias_enable();
#endif

    lcm_panel_init(ctx);

    ret = ctx->error;
    if (ret < 0)
        lcm_unprepare(panel);
    pr_info("[LCM][KERNEL][prepare]ret = %d\n", ret);

    ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
    mtk_panel_tch_rst(panel);
    pr_info("[LCM][KERNEL][prepare]mtk_panel_tch_rst\n");
#endif
#ifdef PANEL_SUPPORT_READBACK
    lcm_panel_get_data(ctx);
#endif

    {
        int mode = LCD_EVENT_LCD_MODE_U3;
        touch_notifier_call_chain(LCD_EVENT_LCD_MODE, (void*)&mode);
    }
    return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
    struct lcm *ctx = panel_to_lcm(panel);

    pr_info("[LCM][KERNEL][enable]%s\n", __func__);
    if (ctx->enabled)
        return 0;

    if (ctx->backlight) {
        ctx->backlight->props.power = FB_BLANK_UNBLANK;
        backlight_update_status(ctx->backlight);
    }

    ctx->enabled = true;

    return 0;
}

#define HFP (38)
#define HSA (4)
#define HBP (28)
#define VFP (34)
#define VSA (2)
#define VBP (13)
#define VAC (2400)
#define HAC (1080)

static const struct drm_display_mode default_mode = {
    .clock = 175177,
    .hdisplay = HAC,
    .hsync_start = HAC + HFP,
    .hsync_end = HAC + HFP + HSA,
    .htotal = HAC + HFP + HSA + HBP,
    .vdisplay = VAC,
    .vsync_start = VAC + VFP,
    .vsync_end = VAC + VFP + VSA,
    .vtotal = VAC + VFP + VSA + VBP,
    .vrefresh = 60,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
    struct lcm *ctx = panel_to_lcm(panel);

    pr_info("[LCM][KERNEL][reset]%s\n", __func__);

    ctx->reset_gpio =
        devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->reset_gpio)) {
        dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
            __func__, PTR_ERR(ctx->reset_gpio));
        return PTR_ERR(ctx->reset_gpio);
    }
    gpiod_set_value(ctx->reset_gpio, on);
    devm_gpiod_put(ctx->dev, ctx->reset_gpio);
    pr_info("[LCM][KERNEL][reset]reset_gpio is %d\n", on);

    return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
    struct lcm *ctx = panel_to_lcm(panel);
    struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
    unsigned char data[3] = {0x00, 0x00, 0x00};
    unsigned char id[3] = {0x00, 0x00, 0x00};
    ssize_t ret;

    pr_info("[LCM][KERNEL][ata_check]%s\n", __func__);
    ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
    if (ret < 0) {
        pr_err("%s error\n", __func__);
        return 0;
    }

    DDPINFO("ATA read data %x %x %x\n", data[0], data[1], data[2]);

    if (data[0] == id[0] && \
        data[1] == id[1] && \
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

    pr_info("[LCM][KERNEL][setbacklight]%s\n", __func__);
    bl_tb0[1] = level;

    if (!cb)
        return -1;

    cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

    pr_info("[LCM][KERNEL][setbacklight]reset_gpio is %d\n", level);

    return 0;
}

static struct mtk_panel_params ext_params = {
    .pll_clk = 550,
    .vfp_low_power = 0,
    .cust_esd_check = 0,
    .esd_check_enable = 0,
    .lcm_esd_check_table[0] = {
        .cmd = 0x0a,
        .count = 1,
        .para_list[0] = 0x9c,
    },
    .dyn_fps = {
        .switch_en = 1,
        .vact_timing_fps = 60,
    },
    .esd_check_enable = 1,
    .lcm_esd_check_table[0] = {
        .cmd = 0x0a,
        .count = 0,
        .para_list[0] = 0x1c,
        .mask_list[0] = 0x1c, // 00011100
    },
};

static int lcm_ext_param_get(struct mtk_panel_params *ext_para, unsigned int mode)
{
    int ret = 0;
    switch (mode) {
        case 0: // 60Hz
#if IS_ENABLED(CONFIG_LGE_DISPLAY_REFRESH_RATE_DIV)
        case 1: // fake 30Hz; physically same with 60Hz
#endif
            *ext_para = ext_params;
            break;
        default:
            ret = 1;
    }

    return ret;
}

static struct mtk_panel_funcs ext_funcs = {
    .reset = panel_ext_reset,
    .set_backlight_cmdq = lcm_setbacklight_cmdq,
    .ata_check = panel_ata_check,
    .ext_param_get = lcm_ext_param_get,
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

static void mode_set_name(struct drm_display_mode *mode)
{
	snprintf(mode->name, DRM_DISPLAY_MODE_LEN, "%dx%dx%d", mode->hdisplay, mode->vdisplay, mode->vrefresh);
}

static int lcm_get_modes(struct drm_panel *panel)
{
    struct drm_display_mode *mode;
    int num = 1;

    pr_info("[LCM][KERNEL][get_modes]%s\n", __func__);

    mode = drm_mode_duplicate(panel->drm, &default_mode);
    if (!mode) {
        dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
            default_mode.hdisplay, default_mode.vdisplay,
            default_mode.vrefresh);
        return -ENOMEM;
    }

    mode_set_name(mode);
    mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
    drm_mode_probed_add(panel->connector, mode);

    pr_info("[LCM][KERNEL][get_modes][%d] %s\n", num, mode->name);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_REFRESH_RATE_DIV)
    mode = drm_mode_duplicate(panel->drm, &default_mode); // duplicate 60Hz mode
    if (!mode) {
        dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
            default_mode.hdisplay, default_mode.vdisplay,
            default_mode.vrefresh);
        return -ENOMEM;
    }
    mode->vrefresh = 30;
    mode_set_name(mode);
    mode->type = DRM_MODE_TYPE_DRIVER;
    drm_mode_probed_add(panel->connector, mode);
    num++;

    pr_info("[LCM][KERNEL][get_modes][%d] %s\n", num, mode->name);
#endif
    panel->connector->display_info.width_mm = 64;
    panel->connector->display_info.height_mm = 129;

    return num;
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
    struct lcm *ctx;
    struct device_node *backlight;
    int ret;
    struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

    printk("[LCM][KERNEL]ili7807q probe\n");
    dsi_node = of_get_parent(dev->of_node);
    if (dsi_node) {
        endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
        if (endpoint) {
            remote_node = of_graph_get_remote_port_parent(endpoint);
            if (!remote_node) {
                pr_info("No panel connected,skip probe lcm\n");
                return -ENODEV;
            }
            pr_info("device node name:%s\n", remote_node->name);
        }
    }
    if (remote_node != dev->of_node) {
        pr_info("%s+ skip probe due to not current lcm\n", __func__);
        return -ENODEV;
    }

    ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    mipi_dsi_set_drvdata(dsi, ctx);

    ctx->dev = dev;
    dsi->lanes = 4;
    dsi->format = MIPI_DSI_FMT_RGB888;
    dsi->mode_flags = MIPI_DSI_MODE_VIDEO
             | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
             | MIPI_DSI_CLOCK_NON_CONTINUOUS;

    backlight = of_parse_phandle(dev->of_node, "backlight", 0);
    if (backlight) {
        ctx->backlight = of_find_backlight_by_node(backlight);
        of_node_put(backlight);

        if (!ctx->backlight)
            return -EPROBE_DEFER;
    }

    ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->reset_gpio)) {
        dev_err(dev, "%s: cannot get reset-gpios %ld\n",
            __func__, PTR_ERR(ctx->reset_gpio));
        return PTR_ERR(ctx->reset_gpio);
    }
    gpiod_set_value(ctx->reset_gpio, 1);
    devm_gpiod_put(dev, ctx->reset_gpio);
    pr_info("[LCM][KERNEL][probe]reset_gpio Set 1\n");

    ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->bias_pos)) {
        dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
            __func__, PTR_ERR(ctx->bias_pos));
        return PTR_ERR(ctx->bias_pos);
    }
    gpiod_set_value(ctx->bias_pos, 1);
    devm_gpiod_put(dev, ctx->bias_pos);
    pr_info("[LCM][KERNEL][probe]bias_pos Set 1\n");

    udelay(2000);

    ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->bias_neg)) {
        dev_err(dev, "%s: cannot get bias-neg 1 %ld\n",
            __func__, PTR_ERR(ctx->bias_neg));
        return PTR_ERR(ctx->bias_neg);
    }
    gpiod_set_value(ctx->bias_neg, 1);
    devm_gpiod_put(dev, ctx->bias_neg);
    pr_info("[LCM][KERNEL][probe]bias_neg Set 1\n");

    ctx->prepared = true;
    ctx->enabled = true;

    drm_panel_init(&ctx->panel);
    ctx->panel.dev = dev;
    ctx->panel.funcs = &lcm_drm_funcs;

    ret = drm_panel_add(&ctx->panel);
    if (ret < 0)
        return ret;

    ret = mipi_dsi_attach(dsi);
    printk("ili7807q probe ret = %d\n", ret);
    if (ret < 0)
        drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
    mtk_panel_tch_handle_reg(&ctx->panel);
    ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
    if (ret < 0)
        return ret;
#endif

    printk("%s-\n", __func__);

    return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
    struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
    pr_info("[LCM][KERNEL][remove]%s\n", __func__);

    mipi_dsi_detach(dsi);
    drm_panel_remove(&ctx->panel);

    return 0;
}

static const struct of_device_id lcm_of_match[] = {
    { .compatible = "ili7807q_fhdplus_dsi_vdo_auo_sm5109", },
    { }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
    .probe = lcm_probe,
    .remove = lcm_remove,
    .driver = {
        .name = "panel-ilitek-ili7807q-sm5109-vdo",
        .owner = THIS_MODULE,
        .of_match_table = lcm_of_match,
    },
};

module_mipi_dsi_driver(lcm_driver);


