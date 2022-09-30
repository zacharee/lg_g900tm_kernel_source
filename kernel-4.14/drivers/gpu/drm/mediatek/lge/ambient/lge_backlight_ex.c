#define pr_fmt(fmt)	"[Display][lge-backlight-ex:%s:%d] " fmt, __func__, __LINE__

#include <linux/backlight.h>
#include <linux/err.h>

#include "lge_brightness.h"
#include "mtk_panel_ext.h"
#include "mtk_dsi.h"
#include "mtk_disp_aal.h"
#include "mtk_disp_ccorr.h"
#include "mtk_drm_crtc.h"
#include "lge_dsi_panel.h"

#ifdef CONFIG_LEDS_MTK_PWM
#include "leds-mtk-pwm.h"
#endif

#ifdef CONFIG_LEDS_MTK_DISP
#include "leds-mtk-disp.h"
#endif

#define BL_NODE_NAME_SIZE 32
#define BL_MAX_BRIGHTNESS 255

extern struct lge_blmap *lge_get_blmap(struct mtk_panel_ext *panel, enum lge_blmap_type type);
extern char *lge_get_blmapname(enum lge_blmap_type type);
extern char* get_payload_addr(struct mtk_panel_ext *panel, enum lge_ddic_dsi_cmd_set_type type, int position);
extern int lge_ddic_dsi_panel_tx_cmd_set(struct mtk_dsi *dsi,
				enum lge_ddic_dsi_cmd_set_type type, unsigned int need_lock);

int lge_update_backlight_ex(struct mtk_dsi *dsi)
{
	struct mtk_panel_ext *panel = NULL;
	struct drm_panel *drm_panel = NULL;
	struct backlight_device *ex_bd;
	char *bl_payload = NULL;
	int rc = 0;

	if (!dsi || !dsi->ext || !dsi->panel)
		return -EINVAL;
	panel = dsi->ext;

	switch (panel->lge.fp_lhbm_mode) {
		case LGE_FP_LHBM_READY:
		case LGE_FP_LHBM_ON:
		case LGE_FP_LHBM_SM_ON:
		case LGE_FP_LHBM_FORCED_ON:
			pr_info("Skip backlight setting for LHBM\n");
			return 0;
		break;
		case LGE_FP_LHBM_OFF:
		case LGE_FP_LHBM_SM_OFF:
		case LGE_FP_LHBM_FORCED_OFF:
			if (panel->lge.lhbm_ready_enable){
				pr_info("Skip backlight setting (LHBM READY and OFF)\n");
				return 0;
			}
		break;
	}

	drm_panel = dsi->panel;
	ex_bd = panel->lge.bl_ex_device;
	if (ex_bd == NULL)
		return -EINVAL;

	if (!panel->lge.allow_bl_update_ex || !lge_dsi_panel_is_power_on_lp(dsi)) {
		pr_err("returned - allow_bl_update_ex = %d, lge_dsi_panel_is_power_on_lp() = %d\n", \
				panel->lge.allow_bl_update_ex, lge_dsi_panel_is_power_on_lp(dsi));
		goto exit;
	}


	if (panel->lge.bl_ex_lvl_unset < 0) {
		pr_err("returned - bl_ex_lvl_unset = %d\n", \
				panel->lge.bl_ex_lvl_unset);
		goto exit;
	}
	pr_info("bl_ex_lvl_unset = %d\n", panel->lge.bl_ex_lvl_unset);
	mutex_lock(&panel->brightness_lock);
	bl_payload = get_payload_addr(panel, LGE_DDIC_DSI_BL_SET, 0);
	if (!bl_payload) {
		pr_err("LGE_DDIC_DSI_BL_SET is NULL\n");
		return -EINVAL;
	}

	bl_payload[1] = (panel->lge.bl_ex_lvl_unset >> 8) & 0x07;
	bl_payload[2] = panel->lge.bl_ex_lvl_unset & 0xFF;

	lge_ddic_dsi_panel_tx_cmd_set(dsi, LGE_DDIC_DSI_BL_SET, 0);
	panel->lge.allow_bl_update_ex = true;
	panel->lge.bl_ex_lvl_unset = -1;
	mutex_unlock(&panel->brightness_lock);
exit:
	return rc;
}

static int lge_backlight_ex_device_update_status(struct backlight_device *bd)
{
	int brightness;
	struct lge_blmap *blmap;
	struct mtk_panel_ext *panel = NULL;
	struct drm_panel *drm_panel = NULL;
	struct mtk_dsi *dsi = NULL;
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_private *private = NULL;
	enum lge_blmap_type bl_type;
	int bl_lvl;
	char *bl_payload = NULL;

	dsi = bl_get_data(bd);
	if (!dsi || !dsi->ext || !dsi->panel)
		return -EINVAL;
	crtc = dsi->encoder.crtc;
	if (!crtc) {
		pr_info("crtc is null");
		return -EINVAL;
	}

	private = crtc->dev->dev_private;
	if (!private) {
		pr_info("private is null");
		return -EINVAL;
	}
	panel = dsi->ext;

	brightness = bd->props.brightness;
	drm_panel = dsi->panel;
	bl_type = LGE_BLMAP_EX;
	blmap = lge_get_blmap(panel, bl_type);

	if (blmap) {
		// DUMMY panel doesn't have blmap, so this code is mandatory
		if(blmap->size == 0)	return -EINVAL;
		if (brightness >= blmap->size) {
			pr_warn("brightness=%d is bigger than blmap size (%d)\n", brightness, blmap->size);
			brightness = blmap->size-1;
		}
		bl_lvl = blmap->map[brightness];
		panel->lge.bl_level = bl_lvl;
	}

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
	mutex_unlock(&panel->brightness_lock);
	mutex_unlock(&private->commit.lock);

	return 0;
}

static int lge_backlight_ex_device_get_brightness(struct backlight_device *bd)
{
	return 0;
}

static const struct backlight_ops lge_backlight_ex_device_ops = {
	.update_status = lge_backlight_ex_device_update_status,
	.get_brightness = lge_backlight_ex_device_get_brightness,
};

int lge_backlight_ex_setup(struct mtk_dsi *dsi)
{
	struct backlight_properties props = {0,};
	struct mtk_panel_ext *panel;
	char bl_node_name[BL_NODE_NAME_SIZE];
	static int display_count = 0;

	if (!dsi || !dsi->ext) {
		pr_err("invalid param\n");
		return -EINVAL;
	}

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.power = FB_BLANK_UNBLANK;

	panel = dsi->ext;

	props.max_brightness = BL_MAX_BRIGHTNESS;
	props.brightness = 0;
	snprintf(bl_node_name, BL_NODE_NAME_SIZE, "panel%u-backlight-ex", display_count);
	panel->lge.bl_ex_lvl_unset = -1;
	panel->lge.allow_bl_update_ex = false;
	panel->lge.bl_ex_device = backlight_device_register(bl_node_name, dsi->dev,
			dsi, &lge_backlight_ex_device_ops, &props);
	if (IS_ERR_OR_NULL(panel->lge.bl_ex_device)) {
		pr_err("Failed to register backlight-ex: %ld\n",
				    PTR_ERR(panel->lge.bl_ex_device));
		panel->lge.bl_ex_device = NULL;
		return -ENODEV;
	}
	display_count++;

	return 0;
}

void lge_backlight_ex_destroy(struct mtk_dsi *dsi)
{
	struct mtk_panel_ext *panel;

	if(!dsi || !dsi->ext)
		return;

	panel = dsi->ext;

	if (panel->lge.bl_ex_device)
		backlight_device_unregister(panel->lge.bl_ex_device);
}
