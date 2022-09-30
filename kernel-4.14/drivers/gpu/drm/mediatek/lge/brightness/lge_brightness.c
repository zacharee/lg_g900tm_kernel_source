#define pr_fmt(fmt)	"[Display][lge-backlight:%s:%d] " fmt, __func__, __LINE__

#include <linux/kallsyms.h>
#include <linux/backlight.h>
#include <linux/leds.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include "lge_brightness.h"
#include "mtk_panel_ext.h"
#include "mtk_dsi.h"
#include "mtk_disp_aal.h"
#include "mtk_drm_crtc.h"

#ifdef CONFIG_LEDS_MTK_PWM
#include "leds-mtk-pwm.h"
#elif CONFIG_LEDS_MTK_DISP
#include "leds-mtk-disp.h"
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DIMMING_BOOT_SUPPORT)
#include "lge_dsi_panel.h"
#endif

#if IS_ENABLED(CONFIG_LGE_COVER_DISPLAY)
#include "../cover/lge_backlight_cover.h"
#include "../cover/lge_cover_ctrl.h"

extern int is_dd_connected(void);
extern bool is_dd_button_enabled(void);
#endif

extern void disp_pq_notify_backlight_changed(int bl_1024);
extern void disp_aal_notify_backlight_changed(int bl_1024);

extern char* get_payload_addr(struct mtk_panel_ext *panel, enum lge_ddic_dsi_cmd_set_type type, int position);
extern int lge_ddic_dsi_panel_tx_cmd_set(struct mtk_dsi *dsi,
                                enum lge_ddic_dsi_cmd_set_type type, unsigned int need_lock);

char *blmap_names[] = {
	"lge,blmap",
	"lge,blmap-ve",
	"lge,blmap-ex",
	"lge,dynamic-blmap-brighter",
	"lge,blmap-hdr",
	"lge,blmap-vr",
	"lge,blmap-daylight",
	"lge,blmap-hdr-daylight"
};

static const int blmap_names_num = sizeof(blmap_names)/sizeof(blmap_names[0]);

inline static void lge_blmap_free_sub(struct lge_blmap *blmap)
{
	if (blmap->map) {
		kfree(blmap->map);
		blmap->map = NULL;
		blmap->size = 0;
	}
}

void lge_dsi_panel_blmap_free(struct mtk_panel_ext *panel)
{
	int i;
	if (panel->lge.blmap_list) {
		for (i = 0; i < panel->lge.blmap_list_size; ++i) {
			lge_blmap_free_sub(&panel->lge.blmap_list[i]);
		}
		kfree(panel->lge.blmap_list);
		panel->lge.blmap_list_size = 0;
	}
}

static int lge_dsi_panel_parse_blmap_sub(struct device_node *of_node, const char* blmap_name, struct lge_blmap *blmap)
{
	struct property *data;
	int rc = 0;

	if (!blmap) {
		return -EINVAL;
	}

	blmap->size = 0;
	data = of_find_property(of_node, blmap_name, &blmap->size);
	if (!data) {
		pr_err("can't find %s\n", blmap_name);
		return -EINVAL;
	}
	blmap->size /= sizeof(u32);
	pr_info("%s blmap_size = %d\n", blmap_name, blmap->size);
	blmap->map = kzalloc(sizeof(u32) * blmap->size, GFP_KERNEL);
	if (!blmap->map) {
		blmap->size = 0;
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(of_node, blmap_name, blmap->map,
					blmap->size);
	if (rc) {
		lge_blmap_free_sub(blmap);
	} else {
		pr_info("%s has been successfully parsed. \n", blmap_name);
	}

	return rc;
}

int lge_dsi_panel_parse_brightness(struct mtk_panel_ext *panel,
	struct device_node *of_node)
{
	int rc = 0;

	rc = of_property_read_u32(of_node, "lge,default-brightness", &panel->lge.default_brightness);
	if (rc) {
		return rc;
	} else {
		pr_info("default brightness=%d \n", panel->lge.default_brightness);
	}

	return rc;
};

int lge_dsi_panel_parse_blmap(struct mtk_panel_ext *panel,
	struct device_node *of_node)
{
	int rc = 0;
	int i;

	panel->lge.use_dynamic_brightness = of_property_read_bool(of_node, "lge,use-dynamic-brightness");
	pr_info("use dynamic brightness supported=%d\n", panel->lge.use_dynamic_brightness);

	panel->lge.blmap_list = kzalloc(sizeof(struct lge_blmap) * blmap_names_num, GFP_KERNEL);
	if (!panel->lge.blmap_list)
		return -ENOMEM;
	panel->lge.blmap_list_size = blmap_names_num;

	if (panel->lge.use_dynamic_brightness) {
		blmap_names[LGE_BLMAP_DEFAULT] = "lge,dynamic-blmap-normal";
	}

	for (i = 0; i < blmap_names_num; ++i) {
		lge_dsi_panel_parse_blmap_sub(of_node, blmap_names[i], &panel->lge.blmap_list[i]);
	}

	return rc;
};

char *lge_get_blmapname(enum lge_blmap_type type)
{
	if (type >= 0 && type < LGE_BLMAP_TYPE_MAX)
		return blmap_names[type];
	else
		return blmap_names[LGE_BLMAP_DEFAULT];
}

struct lge_blmap *lge_get_blmap(struct mtk_panel_ext *panel, enum lge_blmap_type type)
{
	struct lge_blmap *blmap;

	if (type < 0 || type > panel->lge.blmap_list_size)
		type = LGE_BLMAP_DEFAULT;

	blmap = &panel->lge.blmap_list[type];
	if (!blmap)
		blmap = &panel->lge.blmap_list[LGE_BLMAP_DEFAULT];

	return blmap;
}

int lge_update_backlight()
{
	int brightness = 0;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(lge_get_crtc());
	struct mtk_panel_ext *panel = NULL;
	int rc = 0;

	if(!mtk_crtc) {
		pr_err("[Display][%s] crtc is NULL!\n", __func__);
		return 0;
	}

	panel = mtk_crtc->panel_ext;


	if (panel->lge.allow_bl_update) {
		pr_info("[Display][%s] allow_bl is already 1\n", __func__);
		return 0;
	}

	if (panel->lge.bl_lvl_unset == -1) {
		panel->lge.allow_bl_update = true;
		pr_info("[Display][%s] lvl_unset is -1\n", __func__);
		return 0;
	}
	mutex_lock(&panel->brightness_lock);

	brightness = panel->lge.bl_lvl_unset;
	panel->lge.bl_lvl_unset = -1;
	panel->lge.allow_bl_update = true;

	disp_pq_notify_backlight_changed(brightness);
	disp_aal_notify_backlight_changed(brightness);
	panel->lge.bl_level = brightness;

	mutex_unlock(&panel->brightness_lock);

	pr_info("[Display] %s : %d\n", __func__, brightness);
	return rc;
}

int lge_get_brightness_mapping_value(int brightness_256)
{
	struct lge_blmap *blmap = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(lge_get_crtc());
	struct mtk_panel_ext *panel = NULL;
	enum lge_blmap_type bl_type;
	int bl_lvl = -1;

	if(!mtk_crtc) {
		pr_err("[%s] crtc is NULL!\n", __func__);
		return 0;
	}

	panel = mtk_crtc->panel_ext;

        if((panel->lge.lp_state == LGE_PANEL_LP2) || (panel->lge.lp_state == LGE_PANEL_LP1)) {
                bl_type = LGE_BLMAP_EX;
        } else if(panel->lge.vr_lp_mode) {
		bl_type = LGE_BLMAP_VR;
	} else if (panel->lge.hdr_mode) {
		bl_type = LGE_BLMAP_HDR;
		if (panel->lge.daylight_mode)
			bl_type = LGE_BLMAP_HDR_DAYLIGHT;
	} else if (panel->lge.use_color_manager && panel->lge.video_enhancement) {
		bl_type = LGE_BLMAP_VE;
	} else if (panel->lge.use_dynamic_brightness && panel->lge.brightness_table) {
		bl_type = LGE_BLMAP_BRIGHTER;
	} else if (panel->lge.daylight_mode) {
		bl_type = LGE_BLMAP_DAYLIGHT;
	} else {
		bl_type = LGE_BLMAP_DEFAULT;
	}

	blmap = lge_get_blmap(panel, bl_type);

	if (blmap) {
		// DUMMY panel doesn't have blmap, so this code is mandatory
		if(blmap->size == 0)	return -EINVAL;
		if (brightness_256 >= blmap->size) {
			pr_warn("brightness=%d is bigger than blmap size (%d)\n", brightness_256, blmap->size);
			brightness_256 = blmap->size-1;
		}
		bl_lvl = blmap->map[brightness_256];
		printk("%s:BR:%d BL:%d %s\n", __func__, brightness_256, bl_lvl, lge_get_blmapname(bl_type));
	}

	return bl_lvl;
}

int lge_backlight_ex_device_update_status(struct mtk_dsi *dsi, int brightness, int bl_lvl, enum lge_blmap_type bl_type)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(lge_get_crtc());
        struct mtk_panel_ext *panel;
        char *bl_payload = NULL;

        panel = mtk_crtc->panel_ext;

        if (((panel->lge.lp_state == LGE_PANEL_LP2) || (panel->lge.lp_state == LGE_PANEL_LP1))
                        && panel->lge.allow_bl_update_ex) {
                panel->lge.bl_ex_lvl_unset = -1;

                bl_payload = get_payload_addr(panel, LGE_DDIC_DSI_BL_SET, 0);
                if (!bl_payload) {
                        pr_err("LGE_DDIC_DSI_BL_SET is NULL\n");
                        goto error;
                }
                bl_payload[1] = (bl_lvl >> 8) & 0x07;
                bl_payload[2] = bl_lvl & 0xFF;

                lge_ddic_dsi_panel_tx_cmd_set(dsi, LGE_DDIC_DSI_BL_SET, 1);
                pr_info("BR:%d BL:%d %s\n", brightness, bl_lvl, lge_get_blmapname(bl_type));

        } else if (!panel->lge.allow_bl_update_ex) {
                panel->lge.bl_ex_lvl_unset = bl_lvl;
                pr_info("brightness=%d, bl_lvl=%d -> differed (not allow)\n", brightness, bl_lvl);
        } else {
                panel->lge.bl_ex_lvl_unset = bl_lvl;
                pr_info("brightness=%d, bl_lvl=%d -> differed\n", brightness, bl_lvl);
        }
error:
        return 0;
}


int lge_backlight_device_update_status()
{
	int brightness;
	struct lge_blmap *blmap;
	struct drm_crtc *crtc = lge_get_crtc();
	struct mtk_drm_private *private = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(lge_get_crtc());
	struct mtk_panel_ext *panel;
	int bl_lvl;
	enum lge_blmap_type bl_type;
	int rc = 0;
	struct mtk_dsi *dsi = NULL;

	pr_info("%s : ++\n", __func__);
	brightness = lge_get_user_brightness_level();

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DIMMING_BOOT_SUPPORT)
	if (lge_get_bootreason_with_lcd_dimming() && !is_blank_called() && brightness > 0) {
		brightness = 1;
		pr_info("lcd dimming mode. set value = %d\n", brightness);
	} else if (is_factory_cable()  && !is_blank_called() && brightness > 0) {
		brightness =  1;
		pr_info("Detect factory cable. set value = %d\n", brightness);
	}
#endif

	//if (!dsi->output_en)
	//	brightness = 0;

	private = crtc->dev->dev_private;
	panel = mtk_crtc->panel_ext;

	dsi = container_of(&panel, struct mtk_dsi, ext);

	if((panel->lge.lp_state == LGE_PANEL_LP2) || (panel->lge.lp_state == LGE_PANEL_LP1)) {
		bl_type = LGE_BLMAP_EX;
	} else if(panel->lge.vr_lp_mode) {
		bl_type = LGE_BLMAP_VR;
	} else if (panel->lge.hdr_mode) {
		bl_type = LGE_BLMAP_HDR;
		if (panel->lge.daylight_mode)
			bl_type = LGE_BLMAP_HDR_DAYLIGHT;
	} else if (panel->lge.use_color_manager && panel->lge.video_enhancement) {
		bl_type = LGE_BLMAP_VE;
	} else if (panel->lge.use_dynamic_brightness && panel->lge.brightness_table) {
		bl_type = LGE_BLMAP_BRIGHTER;
	} else if (panel->lge.daylight_mode) {
		bl_type = LGE_BLMAP_DAYLIGHT;
	} else {
		bl_type = LGE_BLMAP_DEFAULT;
	}
	blmap = lge_get_blmap(panel, bl_type);

#if IS_ENABLED(CONFIG_LGE_COVER_DISPLAY)
	if (panel->lge.br_offset != 0 && is_dd_button_enabled() &&
			(is_dd_connected() & BIT(0))) {
		brightness = br_to_offset_br_ds1(panel, brightness, blmap->size - 1, BR_MD);
	}
#endif
#if 0
#if defined(CONFIG_LGE_DUAL_SCREEN)
	if (panel->lge.br_offset != 0 && is_ds2_connected()) {
		brightness = br_to_offset_br_ds2(panel, brightness, blmap->size - 1, BR_MD);
	}
#endif
#endif

	if (blmap) {
		// DUMMY panel doesn't have blmap, so this code is mandatory
		if (blmap->size == 0)	return -EINVAL;
		if (brightness >= blmap->size) {
			pr_warn("%s : brightness=%d is bigger than blmap size (%d)\n",__func__, brightness, blmap->size);
			brightness = blmap->size-1;
		}

		bl_lvl = blmap->map[brightness];
	}

	//mutex_lock(&display->display_lock);
	pr_info("commit_lock:%s[%d]\n", __func__, __LINE__);
	mutex_lock(&private->commit.lock);
	switch (panel->lge.fp_lhbm_mode) {
		case LGE_FP_LHBM_READY:
		case LGE_FP_LHBM_ON:
		case LGE_FP_LHBM_SM_ON:
		case LGE_FP_LHBM_FORCED_ON:
			pr_info("Skip backlight setting for LHBM\n");
			mutex_unlock(&private->commit.lock);
			return 0;
		break;
		case LGE_FP_LHBM_OFF:
		case LGE_FP_LHBM_SM_OFF:
		case LGE_FP_LHBM_FORCED_OFF:
			if (panel->lge.lhbm_ready_enable){
				pr_info("Skip backlight setting (LHBM READY and OFF)\n");
				mutex_unlock(&private->commit.lock);
				return 0;
			}
		break;
	}
	mutex_lock(&panel->brightness_lock);
	if (bl_type != LGE_BLMAP_EX && panel->lge.lp_state == LGE_PANEL_NOLP && panel->lge.allow_bl_update) {
		panel->lge.bl_lvl_unset = -1;
		panel->lge.bl_level = bl_lvl;
#ifdef CONFIG_MTK_AAL_SUPPORT
		disp_pq_notify_backlight_changed(bl_lvl);
		disp_aal_notify_backlight_changed(bl_lvl);
#else
		mt_leds_brightness_set_ex("lcd-backlight", bl_lvl);
#endif
		pr_info("%s:BR:%d BL:%d %s\n", __func__, brightness, bl_lvl, lge_get_blmapname(bl_type));
	} else if (bl_type == LGE_BLMAP_EX && ((panel->lge.lp_state == LGE_PANEL_LP2) || (panel->lge.lp_state == LGE_PANEL_LP1))) {
		panel->lge.bl_level = bl_lvl;
		lge_backlight_ex_device_update_status(dsi, brightness, bl_lvl, bl_type);
	} else if (bl_type != LGE_BLMAP_EX && !panel->lge.allow_bl_update) {
		panel->lge.bl_lvl_unset = bl_lvl;
		pr_info("brightness = %d, bl_lvl = %d -> differed (not allow) %s\n", brightness, bl_lvl, lge_get_blmapname(bl_type));
	} else if(bl_type != LGE_BLMAP_EX){
		panel->lge.bl_lvl_unset = bl_lvl;
		pr_info("brightness = %d, bl_lvl = %d -> differed %s\n", brightness, bl_lvl, lge_get_blmapname(bl_type));
	}
	mutex_unlock(&panel->brightness_lock);
	mutex_unlock(&private->commit.lock);
	pr_info("commit_unlock:%s[%d]\n", __func__, __LINE__);

	pr_info("%s : --\n", __func__);
	//mutex_unlock(&display->display_lock);

	return rc;
}

static ssize_t irc_brighter_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int ret = 0;

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		pr_err("dsi is NULL\n");
		return ret;
	}
	panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }

	if (panel->lge.ddic_ops && panel->lge.ddic_ops->get_irc_state)
		ret = (int)panel->lge.ddic_ops->get_irc_state(dsi);
	else
		pr_info("Not support\n");

	return sprintf(buf, "%d\n", ret);
}

static ssize_t irc_brighter_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int input;

	dsi = dev_get_drvdata(dev);
	if (!dsi ) {
		pr_err("dsi is NULL\n");
		return -EINVAL;
	}

        panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }

	sscanf(buf, "%d", &input);
	pr_info("input data %d\n", input);

	mutex_lock(&panel->panel_lock);
	if(!dsi->output_en &&
			(panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl)) {
		panel->lge.irc_pending = true;
		panel->lge.irc_request_state = ((input == 1) ? LGE_IRC_OFF : LGE_IRC_ON);
		pr_err("panel not yet initialized. irc_ctrl is stored.\n");
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}
	mutex_unlock(&panel->panel_lock);
#if 0
	display = primary_display;

	if (!display) {
		pr_err("display is null\n");
		return -EINVAL;
	}

	if (display->is_cont_splash_enabled) {
		pr_err("cont_splash enabled\n");
		return -EINVAL;
	}
#endif
	mutex_lock(&panel->panel_lock);
	if ((panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl) &&
			panel->lge.ddic_ops && panel->lge.ddic_ops->set_irc_state) {
		panel->lge.irc_pending = true;
		panel->lge.irc_request_state = ((input == 1) ? LGE_IRC_OFF : LGE_IRC_ON);
		mutex_unlock(&panel->panel_lock);
		panel->lge.ddic_ops->set_irc_state(dsi, LGE_GLOBAL_IRC_HBM,
						panel->lge.irc_request_state,1);
	} else {
		mutex_unlock(&panel->panel_lock);
		pr_info("Not support\n");
	}

	return ret;
}
static DEVICE_ATTR(irc_brighter, S_IRUGO | S_IWUSR | S_IWGRP, irc_brighter_show, irc_brighter_store);

static ssize_t irc_support_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int ret = 0;

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		pr_err("dsi is NULL\n");
		return ret;
	}

        panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }

	if (!(panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl)) {
		pr_err("irc control is not supported\n");
	} else {
		ret = panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl;
	}

	return sprintf(buf, "%d\n", ret);
}
static DEVICE_ATTR(irc_support, S_IRUGO, irc_support_show, NULL);

static ssize_t brightness_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int ret = 0;

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		pr_err("dsi is NULL\n");
		return ret;
	}

	panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }
	mutex_lock(&panel->panel_lock);
	ret = panel->lge.brightness_table;
	mutex_unlock(&panel->panel_lock);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t brightness_table_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int input;

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		pr_err("dsi is NULL\n");
		return -EINVAL;
	}

	panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }
	sscanf(buf, "%d", &input);
	pr_info("input data %d\n", input);

	mutex_lock(&panel->panel_lock);
	if(!dsi->output_en) {
		pr_err("panel not yet initialized..\n");
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}
	mutex_unlock(&panel->panel_lock);
#if 0
	display = primary_display;

	if (!display) {
		pr_err("display is null\n");
		return -EINVAL;
	}

	if (display->is_cont_splash_enabled) {
		pr_err("cont_splash enabled\n");
		return -EINVAL;
	}
#endif
	mutex_lock(&panel->panel_lock);
	panel->lge.brightness_table = input;
	mutex_unlock(&panel->panel_lock);

	lge_backlight_device_update_status();

	return ret;
}
static DEVICE_ATTR(brightness_table, S_IRUGO | S_IWUSR | S_IWGRP, brightness_table_show, brightness_table_store);

static ssize_t fp_lhbm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int ret = 0;

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		pr_err("dsi is NULL\n");
		return ret;
	}

	panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }
	mutex_lock(&panel->panel_lock);
	ret = panel->lge.fp_lhbm_mode;
	mutex_unlock(&panel->panel_lock);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t fp_lhbm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int input;
	int retry_cnt = 0;

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		pr_err("dsi is NULL\n");
		return -EINVAL;
	}

	panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }
	sscanf(buf, "%d", &input);
	pr_info("input data %d\n", input);

retry_lhbm:
	mutex_lock(&panel->panel_lock);

	panel->lge.fp_lhbm_mode = input;

	if(!dsi->output_en || (panel->lge.lp_state == LGE_PANEL_OFF)) {
		if (retry_cnt < 40) {
			retry_cnt++;
			mutex_unlock(&panel->panel_lock);
			msleep(10);
			goto retry_lhbm;
		}

		pr_err("not ready, initialized: %d, lp_state:%d\n", dsi->output_en, panel->lge.lp_state);
		panel->lge.need_fp_lhbm_set = true;
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}

	mutex_unlock(&panel->panel_lock);
#if 0
	display = primary_display;

	if (!display) {
		pr_err("display is null\n");
		return -EINVAL;
	}

	if (display->is_cont_splash_enabled) {
		pr_err("cont_splash enabled\n");
		return -EINVAL;
	}
#endif
	//mutex_lock(&display->display_lock);
	if (panel->lge.ddic_ops && panel->lge.ddic_ops->lge_set_fp_lhbm)
		panel->lge.ddic_ops->lge_set_fp_lhbm(dsi, panel->lge.fp_lhbm_mode, 1);
	//mutex_unlock(&display->display_lock);

	return ret;
}
static DEVICE_ATTR(fp_lhbm, S_IRUGO | S_IWUSR | S_IWGRP, fp_lhbm_show, fp_lhbm_store);

static ssize_t fp_lhbm_br_lvl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int ret = 0;

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		pr_err("dsi is NULL\n");
		return ret;
	}

	panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }

	mutex_lock(&panel->panel_lock);
	ret = panel->lge.fp_lhbm_br_lvl;
	mutex_unlock(&panel->panel_lock);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t fp_lhbm_br_lvl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int input;

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }

	sscanf(buf, "%d", &input);
	pr_info("input data %d\n", input);

	mutex_lock(&panel->panel_lock);
	if(!dsi->output_en) {
		pr_err("panel not yet initialized..\n");
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}
	mutex_unlock(&panel->panel_lock);
#if 0
	display = primary_display;

	if (!display) {
		pr_err("display is null\n");
		return -EINVAL;
	}

	if (display->is_cont_splash_enabled) {
		pr_err("cont_splash enabled\n");
		return -EINVAL;
	}
#endif
	if (panel->lge.ddic_ops && panel->lge.ddic_ops->lge_set_fp_lhbm_br_lvl)
		panel->lge.ddic_ops->lge_set_fp_lhbm_br_lvl(dsi, input);

	return ret;
}
static DEVICE_ATTR(fp_lhbm_br_lvl, S_IRUGO | S_IWUSR | S_IWGRP, fp_lhbm_br_lvl_show, fp_lhbm_br_lvl_store);


static ssize_t tc_perf_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int ret = 0;

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		pr_err("dsi is NULL\n");
		return ret;
	}

	panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }

	mutex_lock(&panel->panel_lock);
	ret = panel->lge.tc_perf;
	mutex_unlock(&panel->panel_lock);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t tc_perf_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int input;

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		pr_err("dsi is NULL\n");
		return -EINVAL;
	}

	panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }

	sscanf(buf, "%d", &input);
	pr_info("input data %d\n", input);

	mutex_lock(&panel->panel_lock);
	if(!dsi->output_en) {
		pr_err("panel not yet initialized..\n");
		mutex_unlock(&panel->panel_lock);
		return -EINVAL;
	}
	mutex_unlock(&panel->panel_lock);
#if 0
	display = primary_display;

	if (!display) {
		pr_err("display is null\n");
		return -EINVAL;
	}

	if (display->is_cont_splash_enabled) {
		pr_err("cont_splash enabled\n");
		return -EINVAL;
	}
#endif
	panel->lge.tc_perf = input;

	if (panel->lge.ddic_ops && panel->lge.ddic_ops->lge_set_tc_perf)
		panel->lge.ddic_ops->lge_set_tc_perf(dsi, input, 1);

	return ret;
}
static DEVICE_ATTR(tc_perf, S_IRUGO | S_IWUSR | S_IWGRP, tc_perf_show, tc_perf_store);

#if IS_ENABLED(CONFIG_LGE_COVER_DISPLAY) || IS_ENABLED(CONFIG_LGE_DUAL_SCREEN)
static ssize_t br_offset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int ret = 0;

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		pr_err("dsi is NULL\n");
		return ret;
	}

	panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }

	return sprintf(buf, "%d\n",panel->lge.br_offset);
}

static ssize_t br_offset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int data;
	bool connect = false, update = false;

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		pr_err("dsi is NULL\n");
		return -EINVAL;
	}

	panel = dsi->ext;
        if (!panel) {
                pr_err("panel is NULL\n");
                return ret;
        }
	sscanf(buf, "%d", &data);

#if IS_ENABLED(CONFIG_LGE_COVER_DISPLAY)
	if ((is_dd_connected() == DS1) && is_dd_button_enabled()) {
		connect = update = true;
	}
#endif
#if 0
#if IS_ENABLED(CONFIG_LGE_DUAL_SCREEN)
	if (is_ds_connected()) {
		connect = true;
	}
#endif
#endif
	if (connect && (data == BR_OFFSET_BYPASS)) {
		panel->lge.br_offset_bypass = true;
		pr_err("enable bypass\n");
		return ret;
	}

	panel->lge.br_offset = data;
	pr_info("request=%d\n", panel->lge.br_offset);

	if (update) {
		if (panel->lge.br_offset_bypass)
			panel->lge.br_offset_bypass = false;
		panel->lge.br_offset_update = true;
#if IS_ENABLED(CONFIG_LGE_COVER_DISPLAY)
		lge_backlight_cover_device_update_status(panel->lge.bl_cover_device);
#endif
		panel->lge.br_offset_update = false;
	}

	return ret;
}
static DEVICE_ATTR(br_offset, S_IRUGO | S_IWUSR | S_IWGRP, br_offset_show, br_offset_store);
#endif

int lge_brightness_create_sysfs(struct mtk_dsi *dsi,
		struct class *class_panel)
{
	int rc = 0;
	static struct device *brightness_sysfs_dev = NULL;
	struct mtk_panel_ext *panel = NULL;

	panel = dsi->ext;
	if(!panel) {
		pr_err("panel is NULL\n");
		return rc;
        }

	if(!brightness_sysfs_dev) {
		brightness_sysfs_dev = device_create(class_panel, NULL, 0, dsi, "brightness");
		if(IS_ERR(brightness_sysfs_dev)) {
			pr_err("Failed to create dev(brightness_sysfs_dev)!\n");
		} else {
			if (panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl) {
				if ((rc = device_create_file(brightness_sysfs_dev,
								&dev_attr_irc_brighter)) < 0)
					pr_err("add irc_mode set node fail!");

				if ((rc = device_create_file(brightness_sysfs_dev,
								&dev_attr_irc_support)) < 0)
					pr_err("add irc_status set node fail!");
			}
			if (panel->lge.use_dynamic_brightness) {
				if ((rc = device_create_file(brightness_sysfs_dev,
								&dev_attr_brightness_table)) < 0)
					pr_err("add brightness_table set node fail!");
			}
			if (panel->lge.use_fp_lhbm) {
				if ((rc = device_create_file(brightness_sysfs_dev,
								&dev_attr_fp_lhbm)) < 0)
					pr_err("add fp_lhbm set node fail!");
				if ((rc = device_create_file(brightness_sysfs_dev,
								&dev_attr_fp_lhbm_br_lvl)) < 0)
					pr_err("add fp_lhbm set node fail!");
			}
			if (panel->lge.use_tc_perf) {
				if ((rc = device_create_file(brightness_sysfs_dev,
								&dev_attr_tc_perf)) < 0)
					pr_err("add tc_perf set node fail!");
			}
#if defined(CONFIG_LGE_COVER_DISPLAY) || defined(CONFIG_LGE_DUAL_SCREEN)
			if ((rc = device_create_file(brightness_sysfs_dev,
							&dev_attr_br_offset)) < 0)
				pr_err("add br_offset node fail!");
#endif
		}
	}
	return rc;
}
