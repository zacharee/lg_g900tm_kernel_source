/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __KD_SENSORLIST_H__
#define __KD_SENSORLIST_H__

#include "kd_camera_typedef.h"
#include "imgsensor_sensor.h"

struct IMGSENSOR_INIT_FUNC_LIST {
	MUINT32   id;
	MUINT8    name[32];
	MUINT32 (*init)(struct SENSOR_FUNCTION_STRUCT **pfFunc);
};

UINT32 S5KGM2_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 HI1634_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 SL846_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);
UINT32 SL556_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc);

extern struct IMGSENSOR_SENSOR_LIST gimgsensor_sensor_list[];

#endif

