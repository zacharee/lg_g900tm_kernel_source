/*
 * LGE CC/SBU Protection Switch driver
 *
 * Copyright (C) 2018 LG Electronics, Inc.
 * Author: Hansun Lee <hansun.lee@lge.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __LGE_SBU_SWITCH_H__
#define __LGE_SBU_SWITCH_H__

/*
 * LGE_SBU_SWITCH modes & flags
 *
 * Must be declared in priority order. Small value have a high priority.
 * That is, 0 is the highest priority.
 */
enum lge_sbu_mode {
	LGE_SBU_MODE_DISABLE,
	LGE_SBU_MODE_USBID,
	LGE_SBU_MODE_AUX,
	LGE_SBU_MODE_UART,
	/* this must be last mode */
	LGE_SBU_MODE_DEFAULT,
};

int lge_sbu_switch_enable(enum lge_sbu_mode mode, bool en);

#endif /* __LGE_SBU_SWITCH_H__ */
