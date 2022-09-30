#ifndef _LGE_PSEUDO_POWER_SUPPLY_H
#define _LGE_PSEUDO_POWER_SUPPLY_H

#ifdef CONFIG_LGE_PM_CHARGER_CONTROLLER
#include <linux/power/charger_controller.h>
#endif

void pseudo_power_supply_update(char *name);

#endif