#ifndef _LGE_BACKLIGHT_EX_H_
#define _LGE_BACKLIGHT_EX_H_

#include "mtk_dsi.h"

int lge_backlight_ex_setup(struct mtk_dsi *dsi);
void lge_backlight_ex_destroy(struct mtk_dsi *dsi);
int lge_update_backlight_ex(struct mtk_dsi *dsi);

#endif // _LGE_BACKLIGHT_EX_H_

