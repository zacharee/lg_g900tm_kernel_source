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

#define ERROR_BUFFER_COUNT 4
#define FENCE_BUFFER_COUNT 22
#define DEBUG_BUFFER_COUNT 30
#define DUMP_BUFFER_COUNT 10
#define STATUS_BUFFER_COUNT 1
#if defined(CONFIG_MT_ENG_BUILD) || !defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
#define LOGGER_BUFFER_SIZE (16 * 1024)
#else
#define LOGGER_BUFFER_SIZE (256)
#endif
#define DEBUG_BUFFER_SIZE                                                      \
	(4096 +                                                                \
	 (ERROR_BUFFER_COUNT + FENCE_BUFFER_COUNT + DEBUG_BUFFER_COUNT +       \
	  DUMP_BUFFER_COUNT + STATUS_BUFFER_COUNT) *                           \
		 LOGGER_BUFFER_SIZE)

enum MTK_DRM_DEBUG_LOG_SWITCH_OPS {
	MTK_DRM_OTHER = 0,
	MTK_DRM_MOBILE_LOG,
	MTK_DRM_DETAIL_LOG,
	MTK_DRM_FENCE_LOG,
	MTK_DRM_IRQ_LOG,
};
extern void disp_color_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_ccorr_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_gamma_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_dither_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_aal_set_bypass(struct drm_crtc *crtc, int bypass);

extern unsigned int m_new_pq_persist_property[32];
enum mtk_pq_persist_property {
	DISP_PQ_COLOR_BYPASS,
	DISP_PQ_CCORR_BYPASS,
	DISP_PQ_GAMMA_BYPASS,
	DISP_PQ_DITHER_BYPASS,
	DISP_PQ_AAL_BYPASS,
	DISP_PQ_CCORR_SILKY_BRIGHTNESS,
	DISP_PQ_GAMMA_SILKY_BRIGHTNESS,
	DISP_PQ_PROPERTY_MAX,
};

int mtk_drm_ioctl_pq_get_persist_property(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

extern int mtk_disp_hrt_bw_dbg(void);

#ifdef _DRM_P_H_
struct disp_rect {
	u32 x;
	u32 y;
	u32 width;
	u32 height;
};
void disp_dbg_probe(void);
void disp_dbg_init(struct drm_device *drm_dev);
void disp_dbg_deinit(void);
void mtk_drm_cwb_backup_copy_size(void);
int mtk_dprec_mmp_dump_ovl_layer(struct mtk_plane_state *plane_state);
int mtk_dprec_mmp_dump_cwb_buffer(struct drm_crtc *crtc,
	void *buffer, unsigned int buf_idx);
int disp_met_set(void *data, u64 val);
void mtk_drm_idlemgr_kick_ext(const char *source);
unsigned int mtk_dbg_get_lfr_mode_value(void);
unsigned int mtk_dbg_get_lfr_type_value(void);
unsigned int mtk_dbg_get_lfr_enable_value(void);
unsigned int mtk_dbg_get_lfr_update_value(void);
unsigned int mtk_dbg_get_lfr_vse_dis_value(void);
unsigned int mtk_dbg_get_lfr_skip_num_value(void);
unsigned int mtk_dbg_get_lfr_dbg_value(void);
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

int mtk_disp_ioctl_debug_log_switch(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

#endif
