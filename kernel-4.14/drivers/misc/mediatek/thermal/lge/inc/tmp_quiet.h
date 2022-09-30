/*
 * Copyright (C) 2017 MediaTek Inc.
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
#ifndef __TMP_QUIET_H__
#define __TMP_QUIET_H__

/* inherit from bts */
#include "tmp_bts.h"

/* chip dependent */

#define AUX_IN5_NTC (5)

#define QUIET_RAP_PULL_UP_R		390000 /* 390K, pull up resister */

#define QUIET_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define QUIET_RAP_PULL_UP_VOLTAGE		1800 /* 1.8V ,pull up voltage */

#define QUIET_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define QUIET_RAP_ADC_CHANNEL		AUX_IN5_NTC /* default is 0 */

#endif	/* __TMP_QUIET_H__ */
