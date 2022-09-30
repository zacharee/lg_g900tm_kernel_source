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

#ifndef _MTK_DRM_PMQOS_H_
#define _MTK_DRM_PMQOS_H_

#include "mmdvfs_pmqos.h"
#if defined(CONFIG_MACH_MT6779)
#include "helio-dvfsrc-opp-mt6779.h"
#elif defined(CONFIG_MACH_MT6885)
#include "helio-dvfsrc-opp-mt6885.h"
#include "dt-bindings/memory/mt6885-larb-port.h"
#elif defined(CONFIG_MACH_MT6873)
#include "helio-dvfsrc-opp-mt6873.h"
#include "dt-bindings/memory/mt6873-larb-port.h"
#endif
#include <linux/pm_qos.h>

enum DISP_QOS_BW_MODE {
	DISP_BW_NORMAL_MODE = 0,
	DISP_BW_FBDC_MODE,
	DISP_BW_HRT_MODE,
};

#define NO_PENDING_HRT (0xFFFFFFFF)
#define OVL_REQ_HRT (0x1)
#define RDMA_REQ_HRT (0x2)

struct drm_crtc;
struct mtk_drm_crtc;

struct mtk_drm_qos_ctx {
	unsigned int last_hrt_req;
	atomic_t last_hrt_idx;
	atomic_t hrt_cond_sig;
	wait_queue_head_t hrt_cond_wq;
};

int __mtk_disp_set_module_bw(struct mm_qos_request *request, int comp_id,
			     unsigned int bandwidth, unsigned int mode);
void __mtk_disp_set_module_hrt(struct mm_qos_request *request,
			       unsigned int bandwidth);
int mtk_disp_set_hrt_bw(struct mtk_drm_crtc *mtk_crtc,
			unsigned int overlap_num);
int __mtk_disp_pmqos_slot_look_up(int comp_id, int mode);
int __mtk_disp_pmqos_port_look_up(int comp_id);
int mtk_disp_hrt_cond_init(struct drm_crtc *crtc);
void mtk_drm_mmdvfs_init(void);
void mtk_drm_set_mmclk(struct drm_crtc *crtc, int level, const char *caller);
#endif
