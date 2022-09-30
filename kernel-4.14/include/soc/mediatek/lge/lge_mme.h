/* Copyright (c) 2016 LG Electronics, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MACH_LGE_MME_TEST_H
#define __MACH_LGE_MME_TEST_H

/* f2f data format */
#define TRACK1_FORMAT            1        // 0001
#define TRACK2_FORMAT            2        // 0010
#define TRACK3_FORMAT            4        // 0100
#define COMBO1_FORMAT            9        // 1001
#define COMBO2_FORMAT            10       // 1010

#define TRACK1_SIZE              79
#define TRACK2_SIZE              40
#define TRACK3_SIZE              107
#define COMBO_TRACK_SIZE         (TRACK1_SIZE + TRACK2_SIZE)

#define TRACK1_BIT_FORMAT         7
#define TRACK2_BIT_FORMAT         5
#define TRACK3_BIT_FORMAT         5

#define TRACK1_TX_BIT_LENGTH      (TRACK1_SIZE * TRACK1_BIT_FORMAT)
#define TRACK2_TX_BIT_LENGTH      (TRACK2_SIZE * TRACK2_BIT_FORMAT)
#define COMBO_TRACK_TX_BIT_LENGTH (TRACK1_TX_BIT_LENGTH + TRACK2_TX_BIT_LENGTH)

#define F2F_TIME                  370
#define F2F_INITIAL_ZERO          30

#define DEVICE_NAME               "lge_mme"

#endif
