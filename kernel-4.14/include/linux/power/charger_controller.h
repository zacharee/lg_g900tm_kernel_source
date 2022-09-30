#ifndef __CHARGER_CONTROLLER_H__
#define __CHARGER_CONTROLLER_H__

#include <linux/power_supply.h>

void chgctrl_battery_property_override(enum power_supply_property psp,
				       union power_supply_propval *val);
void chgctrl_charger_property_override(enum power_supply_property psp,
				       union power_supply_propval *val);

/* pseudo power-supply support */
enum {
    PSEUDO_BATTERY,
    PSEUDO_HVDCP,
};
void chgctrl_set_pseudo_mode(int mode, int en);

/* water detect support */
void chgctrl_set_water_detect(bool detected);
void chgctrl_set_charger_ov(bool detected);

#if CONFIG_MTK_GAUGE_VERSION == 30
/* mtk gm 3.0 (2.5 included) driver support */
int chgctrl_get_icl_boost(void);
int chgctrl_get_icl(void);
int chgctrl_get_fcc(void);
int chgctrl_get_vfloat(void);
bool chgctrl_get_input_suspend(void);
int chgctrl_get_wless_pwr(void);
#endif

#endif
