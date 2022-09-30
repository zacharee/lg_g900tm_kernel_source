#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#if IS_ENABLED(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
#include <linux/lge_panel_notify.h>
#endif
#include <drm/drm_panel.h>
#include <drm/drm_mode.h>
#include <drm/drm_mipi_dsi.h>
#include "mtk_drm_drv.h"
#include "lge_dsi_panel.h"
#include "mtk_panel_ext.h"
#include "cm/lge_color_manager.h"
#include "lge_brightness.h"

#ifdef CONFIG_LEDS_MTK_PWM
#include <linux/leds.h>
#include "leds-mtk-pwm.h"
#endif

#ifdef CONFIG_LEDS_MTK_DISP
#include <linux/leds.h>
#include "leds-mtk-disp.h"
#endif

#undef pr_fmt
#define pr_fmt(fmt)	"[Display][ili7807q-ops:%s:%d] " fmt, __func__, __LINE__

#define ADDR_PTLAR 0x30
#define ADDR_PLTAC 0x31
#define ADDR_RDDISPM 0x3F
#define ADDR_ERR_DETECT 0x9F
#define ADDR_WRIECTL 0x55
#define ADDR_PWRCTL3 0xC3

extern int store_aod_area(struct mtk_panel_ext *panel, struct lge_rect *rect);
extern int lge_ddic_dsi_panel_tx_cmd_set(struct mtk_panel_ext *panel,
				enum lge_ddic_dsi_cmd_set_type type, unsigned int need_lock);
extern char* get_payload_addr(struct mtk_panel_ext *panel, enum lge_ddic_dsi_cmd_set_type type, int position);
extern int get_payload_cnt(struct mtk_panel_ext *panel, enum lge_ddic_dsi_cmd_set_type type, int position);
extern int lge_backlight_device_update_status(void);
extern int lcm_panel_tx_cmd_set(struct mtk_panel_ext *panel_ext, enum lge_ddic_dsi_cmd_set_type type);
extern struct drm_crtc* lge_get_crtc(void);

#define IDX_DG_CTRL1 1
#define REG_DG_CTRL1 0xB4
#define NUM_DG_CTRL1 11
#define START_DG_CTRL1 2

#define IDX_DG_CTRL2 2
#define REG_DG_CTRL2 0xB5
#define NUM_DG_CTRL2 28
#define START_DG_CTRL2 1

#define OFFSET_DG_UPPER 3
#define OFFSET_DG_LOWER 9

#define STEP_DG_PRESET 5
#define NUM_DG_PRESET  35

#define COMMIT_LOCK(need_lock, args) do { \
if (need_lock) {\
	pr_info("commit_lock:%s[%d]\n", __func__, __LINE__); \
	mutex_lock(args); \
}\
} while(0)

#define COMMIT_UNLOCK(need_lock, args) do { \
if (need_lock) {\
	pr_info("commit_unlock:%s[%d]\n", __func__, __LINE__); \
	mutex_unlock(args); \
}\
} while(0)

static int lge_set_video_enhancement_ili7807q(struct mtk_panel_ext *panel, int input, unsigned int need_lock)
{
	int rc = 0;
	if(!panel) {
		pr_err("panel not exist\n");
		return -1;
	}

	if (input) {
		rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_VIDEO_ENHANCEMENT_ON, need_lock);
		if (rc)
			pr_err("failed to send VIDEO_ENHANCEMENT_ON cmd, rc=%d\n", rc);
	}
	else {
		rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_VIDEO_ENHANCEMENT_OFF, need_lock);
		if (rc)
			pr_err("failed to send VIDEO_ENHANCEMENT_OFF cmd, rc=%d\n", rc);
	}

	pr_info("send cmds to %s the video enhancer \n", (input == true) ? "enable" : "disable");

	lge_backlight_device_update_status();

    return rc;
}

static int lge_hdr_mode_set_ili7807q(struct mtk_panel_ext *panel, int input, unsigned int need_lock)
{
	bool hdr_mode = ((input > 0) ? true : false);
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_private *private = NULL;

	if(!panel) {
		pr_err("panel not exist\n");
		return -1;
	}

	crtc = lge_get_crtc();
	private = crtc->dev->dev_private;

	COMMIT_LOCK(need_lock, &private->commit.lock);
	mutex_lock(&panel->panel_lock);
	if (hdr_mode) {
		panel->lge.color_manager_status = 0;
		panel->lge.dgc_status = 0x00;
	} else {
		panel->lge.color_manager_status = 1;
	}
	mutex_unlock(&panel->panel_lock);
	COMMIT_UNLOCK(need_lock, &private->commit.lock);
	pr_info("hdr=%s, cm=%s dgc=%s\n", (hdr_mode ? "set" : "unset"),
			((panel->lge.color_manager_status == 1) ? "enabled" : "disabled"),
			((panel->lge.dgc_status == 1) ? "enabled" : "disabled"));
	/*if (hdr_mode)
		lge_display_control_store_ili7807q(panel, true, need_lock);
	else
		lge_set_screen_mode_ili7807q(panel, true, need_lock);*/

	lge_backlight_device_update_status();

	return 0;
}
struct lge_ddic_ops ili7807q_ops = {
	/* image quality */
	.hdr_mode_set = lge_hdr_mode_set_ili7807q,
	.lge_set_video_enhancement = lge_set_video_enhancement_ili7807q,

	.panel_tx_cmd_set = lcm_panel_tx_cmd_set,
};
