#ifndef _H_LGE_DSI_PANEL_
#define _H_LGE_DSI_PANEL_

#include "mtk_dsi.h"

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DIMMING_BOOT_SUPPORT)
bool is_blank_called(void);
int lge_get_bootreason_with_lcd_dimming(void);
bool is_factory_cable(void);
#endif

bool lge_dsi_panel_is_power_on(struct mtk_dsi *dsi);
bool lge_dsi_panel_is_power_on_lp(struct mtk_dsi *dsi);
bool lge_dsi_panel_is_power_on_interactive(struct mtk_dsi *dsi);

int lge_dsi_panel_get(struct mtk_dsi *dsi, struct device_node *of_node);
int lge_dsi_panel_drv_init(struct mtk_dsi *dsi);
void lge_dsi_panel_put(struct mtk_panel_ext *panel);

int dsi_panel_set_lp2(struct mtk_dsi *dsi);
int dsi_panel_set_lp1(struct mtk_dsi *dsi);
int dsi_panel_set_nolp(struct mtk_dsi *dsi);
#endif //_H_LGE_DSI_PANEL_
