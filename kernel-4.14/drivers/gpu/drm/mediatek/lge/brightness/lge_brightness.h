#ifndef _LGE_BRIGHTNESS_H_
#define _LGE_BRIGHTNESS_H_

#include <linux/device.h>
#include <linux/of.h>
#include "../../mtk_panel_ext.h"

#if IS_ENABLED(CONFIG_LGE_DISPLAY_USE_FSC)
#define DEFAULT_LGE_FSC_U3 20000
#define DEFAULT_LGE_FSC_U2 2500
#endif

#define BR_OFFSET_BYPASS 100

enum cover_br_type {
	BR_MD = 0,
	BR_DD,
	BR_XD
};

//extern int lge_backlight_device_update_status(struct backlight_device *bd);
int lge_backlight_device_update_status(void);
int lge_backlight_ex_device_update_status(struct mtk_dsi *dsi, int brightness, int bl_lvl, enum lge_blmap_type bl_type);
int lge_update_backlight(void);
bool lge_get_bl_allow(void);
int lge_get_brightness_mapping_value(int brightness_256);
int lge_dsi_panel_parse_blmap(struct mtk_panel_ext *panel, struct device_node *of_node);
int lge_dsi_panel_parse_brightness(struct mtk_panel_ext *panel, struct device_node *of_node);
void lge_dsi_panel_blmap_free(struct mtk_panel_ext *panel);
int lge_brightness_create_sysfs(struct mtk_dsi *dsi, struct class *class_panel);
#endif // _LGE_BRIGHTNESS_H_

