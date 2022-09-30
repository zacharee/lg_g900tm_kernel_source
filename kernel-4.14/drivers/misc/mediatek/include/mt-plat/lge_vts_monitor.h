#ifndef _LGE_VTS_MONITOR_H
#define _LGE_VTS_MONITOR_H

#include <linux/power_supply.h>
#include <mt-plat/mtk_thermal_monitor.h>

enum mtk_thermal_sensor_id lge_vts_get_thermal_sensor_id(char *name);
char *lge_vts_get_thermal_sensor_name(enum mtk_thermal_sensor_id id);

bool lge_vts_cmode_enabled(void);

#endif
