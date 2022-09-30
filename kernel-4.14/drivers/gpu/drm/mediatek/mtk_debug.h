/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __MTKFB_DEBUG_H
#define __MTKFB_DEBUG_H

#include <drm/drm_mipi_dsi.h>
#include "mtk_panel_ext.h"

#define LOGGER_BUFFER_SIZE (16 * 1024)
#define ERROR_BUFFER_COUNT 4
#define FENCE_BUFFER_COUNT 22
#define DEBUG_BUFFER_COUNT 30
#define DUMP_BUFFER_COUNT 10
#define STATUS_BUFFER_COUNT 1
#if defined(CONFIG_MT_ENG_BUILD) || !defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
#define DEBUG_BUFFER_SIZE                                                      \
	(4096 +                                                                \
	 (ERROR_BUFFER_COUNT + FENCE_BUFFER_COUNT + DEBUG_BUFFER_COUNT +       \
	  DUMP_BUFFER_COUNT + STATUS_BUFFER_COUNT) *                           \
		 LOGGER_BUFFER_SIZE)
#else
#define DEBUG_BUFFER_SIZE 10240
#endif

extern int mtk_disp_hrt_bw_dbg(void);

#ifdef _DRM_P_H_
void disp_dbg_probe(void);
void disp_dbg_init(struct drm_device *drm_dev);
void disp_dbg_deinit(void);
void mtk_drm_idlemgr_kick_ext(const char *source);
#endif

int mtkfb_set_backlight_level(unsigned int level);


#if defined(CONFIG_LGE_DISPLAY_COMMON)
int lge_ddic_dsi_send_cmd(enum lge_ddic_dsi_cmd_set_type type,
			bool blocking, int need_lock);
int lge_ddic_dsi_send_cmd_msg(struct mipi_dsi_msg *msg,
			bool blocking, int need_lock);
int lge_ddic_dsi_read_cmd_msg(struct mipi_dsi_msg *cmd_msg, int need_lock);
#endif
#if defined(CONFIG_LGE_DISPLAY_COMMON) || defined(CONFIG_LGE_DISPLAY_TUNING_SUPPORT) || defined(CONFIG_LGE_VSYNC_SKIP)
struct drm_crtc* lge_get_crtc(void);
#endif
#endif
