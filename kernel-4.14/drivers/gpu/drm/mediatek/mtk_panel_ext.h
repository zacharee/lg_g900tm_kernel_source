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

#ifndef __MTK_PANEL_EXT_H__
#define __MTK_PANEL_EXT_H__

#include <drm/drm_panel.h>
#include <drm/drm_mipi_dsi.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/backlight.h>
#include <linux/workqueue.h>
#include <linux/irqreturn.h>

#define RT_MAX_NUM 10
#define ESD_CHECK_NUM 3
#define MAX_TX_CMD_NUM 20
#define MAX_RX_CMD_NUM 20
#define READ_DDIC_SLOT_NUM (4 * MAX_RX_CMD_NUM)
#define MAX_DYN_CMD_NUM 20


struct mtk_dsi;
struct cmdq_pkt;
struct mtk_panel_ext;
#ifdef CONFIG_LGE_DISPLAY_COMMON
struct lge_dsi_panel;
enum lge_ddic_dsi_cmd_set_type;

#define DEFAULT_HBM_MODE 0x03;
#define FP_LHBM_OFF 0
#define FP_LHBM_ON 1
#define FP_LHBM_DEFAULT_BR_LVL       4095
#endif

struct mtk_panel_para_table {
	u8 count;
	u8 para_list[128];
};

/*
 *	DSI data type:
 *	DSI_DCS_WRITE_SHORT_PACKET_NO_PARAM		0x05
 *	DSI_DCS_WRITE_SHORT_PACKET_1_PARAM		0x15
 *	DSI_DCS_WRITE_LONG_PACKET					0x39
 *	DSI_DCS_READ_NO_PARAM						0x06

 *	DSI_GERNERIC_WRITE_SHORT_NO_PARAM			0x03
 *	DSI_GERNERIC_WRITE_SHORT_1_PARAM			0x13
 *	DSI_GERNERIC_WRITE_SHORT_1_PARAM			0x23
 *	DSI_GERNERIC_WRITE_LONG_PACKET				0x29
 *	DSI_GERNERIC_READ_NO_PARAM					0x04
 *	DSI_GERNERIC_READ_1_PARAM					0x14
 *	DSI_GERNERIC_READ_2_PARAM					0x24
 */

/**
 * struct mtk_ddic_dsi_msg - MTK write/read DDIC RG cmd buffer
 * @channel: virtual channel id
 * @flags: flags controlling this message transmission
 * @type: payload data type array
 * @tx_len: length of @tx_buf
 * @tx_buf: data array to be written
 * @tx_cmd_num: tx cmd number
 * @rx_len: length of @rx_buf
 * @rx_buf: data array to be read, or NULL
 * @rx_cmd_num: rx cmd number
 */
struct mtk_ddic_dsi_msg {
	u8 channel;
	u16 flags;

	u8 type[MAX_TX_CMD_NUM];
	size_t tx_len[MAX_TX_CMD_NUM];
	const void *tx_buf[MAX_TX_CMD_NUM];
	size_t tx_cmd_num;

	size_t rx_len[MAX_RX_CMD_NUM];
	void *rx_buf[MAX_RX_CMD_NUM];
	size_t rx_cmd_num;
};

struct DSI_RX_DATA_REG {
	unsigned char byte0;
	unsigned char byte1;
	unsigned char byte2;
	unsigned char byte3;
};

typedef void (*dcs_write_gce) (struct mtk_dsi *dsi, struct cmdq_pkt *handle,
				const void *data, size_t len);
typedef void (*dcs_grp_write_gce) (struct mtk_dsi *dsi, struct cmdq_pkt *handle,
				struct mtk_panel_para_table *para_table,
				unsigned int para_size);
typedef int (*panel_tch_rst) (void);

enum MTK_PANEL_OUTPUT_MODE {
	MTK_PANEL_SINGLE_PORT = 0x0,
	MTK_PANEL_DSC_SINGLE_PORT,
	MTK_PANEL_DUAL_PORT,
};

struct esd_check_item {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[RT_MAX_NUM];
	unsigned char mask_list[RT_MAX_NUM];
};

enum MTK_PANEL_MODE_SWITCH_STAGE {
	BEFORE_DSI_POWERDOWN,
	AFTER_DSI_POWERON,
};

enum MIPITX_PHY_PORT {
	MIPITX_PHY_PORT_0 = 0,
	MIPITX_PHY_PORT_1,
	MIPITX_PHY_PORT_NUM
};

enum MIPITX_PHY_LANE_SWAP {
	MIPITX_PHY_LANE_0 = 0,
	MIPITX_PHY_LANE_1,
	MIPITX_PHY_LANE_2,
	MIPITX_PHY_LANE_3,
	MIPITX_PHY_LANE_CK,
	MIPITX_PHY_LANE_RX,
	MIPITX_PHY_LANE_NUM
};

enum FPS_CHANGE_INDEX {
	DYNFPS_NOT_DEFINED = 0,
	DYNFPS_DSI_VFP = 1,
	DYNFPS_DSI_HFP = 2,
	DYNFPS_DSI_MIPI_CLK = 4,
};

struct mtk_panel_dsc_params {
	unsigned int enable;
	unsigned int ver; /* [7:4] major [3:0] minor */
	unsigned int slice_mode;
	unsigned int rgb_swap;
	unsigned int dsc_cfg;
	unsigned int rct_on;
	unsigned int bit_per_channel;
	unsigned int dsc_line_buf_depth;
	unsigned int bp_enable;
	unsigned int bit_per_pixel;
	unsigned int pic_height; /* need to check */
	unsigned int pic_width;  /* need to check */
	unsigned int slice_height;
	unsigned int slice_width;
	unsigned int chunk_size;
	unsigned int xmit_delay;
	unsigned int dec_delay;
	unsigned int scale_value;
	unsigned int increment_interval;
	unsigned int decrement_interval;
	unsigned int line_bpg_offset;
	unsigned int nfl_bpg_offset;
	unsigned int slice_bpg_offset;
	unsigned int initial_offset;
	unsigned int final_offset;
	unsigned int flatness_minqp;
	unsigned int flatness_maxqp;
	unsigned int rc_model_size;
	unsigned int rc_edge_factor;
	unsigned int rc_quant_incr_limit0;
	unsigned int rc_quant_incr_limit1;
	unsigned int rc_tgt_offset_hi;
	unsigned int rc_tgt_offset_lo;
};
struct mtk_dsi_phy_timcon {
	unsigned int hs_trail;
	unsigned int hs_prpr;
	unsigned int hs_zero;
	unsigned int lpx;
	unsigned int ta_get;
	unsigned int ta_sure;
	unsigned int ta_go;
	unsigned int da_hs_exit;
	unsigned int clk_trail;
	unsigned int cont_det;
	unsigned int da_hs_sync;
	unsigned int clk_zero;
	unsigned int clk_hs_prpr;
	unsigned int clk_hs_exit;
	unsigned int clk_hs_post;
};

struct dynamic_mipi_params {
	unsigned int switch_en;
	unsigned int pll_clk;
	unsigned int data_rate;

	unsigned int vsa;
	unsigned int vbp;
	unsigned int vfp;
	unsigned int vfp_lp_dyn;

	unsigned int hsa;
	unsigned int hbp;
	unsigned int hfp;
};

struct dfps_switch_cmd {
	unsigned int src_fps;
	unsigned int cmd_num;
	unsigned char para_list[64];
};

struct dynamic_fps_params {
	unsigned int switch_en;
	unsigned int vact_timing_fps;
	unsigned int data_rate;
	struct dfps_switch_cmd dfps_cmd_table[MAX_DYN_CMD_NUM];
};

struct mtk_panel_params {
	unsigned int pll_clk;
	unsigned int data_rate;
	struct mtk_dsi_phy_timcon phy_timcon;
	unsigned int vfp_low_power;
	struct dynamic_mipi_params dyn;
	struct dynamic_fps_params dyn_fps;
	unsigned int cust_esd_check;
	unsigned int esd_check_enable;
	struct esd_check_item lcm_esd_check_table[ESD_CHECK_NUM];
	unsigned int ssc_disable;
	unsigned int ssc_range;
	int lcm_color_mode;
	unsigned int min_luminance;
	unsigned int average_luminance;
	unsigned int max_luminance;
	unsigned int round_corner_en;
	unsigned int corner_pattern_height;
	unsigned int corner_pattern_height_bot;
	unsigned int corner_pattern_tp_size;
	unsigned int corner_pattern_tp_size_l;
	unsigned int corner_pattern_tp_size_r;
	void *corner_pattern_lt_addr;
	void *corner_pattern_lt_addr_l;
	void *corner_pattern_lt_addr_r;
	unsigned int physical_width_um;
	unsigned int physical_height_um;
	unsigned int lane_swap_en;
	unsigned int is_cphy;
	enum MIPITX_PHY_LANE_SWAP
		lane_swap[MIPITX_PHY_PORT_NUM][MIPITX_PHY_LANE_NUM];
	struct mtk_panel_dsc_params dsc_params;
	unsigned int output_mode;
	unsigned int lcm_cmd_if;
	unsigned int hbm_en_time;
	unsigned int hbm_dis_time;
	unsigned int lcm_index;
	unsigned int wait_sof_before_dec_vfp;
	unsigned int doze_delay;
#if defined(CONFIG_LGE_DISPLAY_RECOVERY)
	u32 chk_mode;
	bool is_state_recovery;
#endif

	//Settings for LFR Function:
	unsigned int lfr_enable;
	unsigned int lfr_minimum_fps;
};

#ifdef CONFIG_LGE_DISPLAY_COMMON
#define NUM_COLOR_MODES  10
#define MAX_BIST_USAGE_TYPE 5

enum lge_blmap_type {
	LGE_BLMAP_DEFAULT = 0,
	LGE_BLMAP_VE,
	LGE_BLMAP_EX,
	LGE_BLMAP_BRIGHTER,
	LGE_BLMAP_HDR,
	LGE_BLMAP_VR,
	LGE_BLMAP_DAYLIGHT,
	LGE_BLMAP_HDR_DAYLIGHT,
	LGE_BLMAP_TYPE_MAX
};

enum fp_lhbm_state {
	LGE_FP_LHBM_OFF = 0,
	LGE_FP_LHBM_ON,
	LGE_FP_LHBM_READY,
	LGE_FP_LHBM_EXIT,
	LGE_FP_LHBM_SM_OFF = 10,
	LGE_FP_LHBM_SM_ON,
	LGE_FP_LHBM_FORCED_OFF = 20,
	LGE_FP_LHBM_FORCED_ON,
	LGE_FP_LHBM_FORCED_READY,
	LGE_FP_LHBM_FORCED_EXIT,
	LGE_FP_LHBM_STATE_MAX,
};

/**
 * enum dsi_cmd_set_type  - DSI command set type
 * @DSI_CMD_SET_PRE_ON:	                   Panel pre on
 * @DSI_CMD_SET_ON:                        Panel on
 * @DSI_CMD_SET_POST_ON:                   Panel post on
 * @DSI_CMD_SET_PRE_OFF:                   Panel pre off
 * @DSI_CMD_SET_OFF:                       Panel off
 * @DSI_CMD_SET_POST_OFF:                  Panel post off
 * @DSI_CMD_SET_PRE_RES_SWITCH:            Pre resolution switch
 * @DSI_CMD_SET_RES_SWITCH:                Resolution switch
 * @DSI_CMD_SET_POST_RES_SWITCH:           Post resolution switch
 * @DSI_CMD_SET_CMD_TO_VID_SWITCH:         Cmd to video mode switch
 * @DSI_CMD_SET_POST_CMD_TO_VID_SWITCH:    Post cmd to vid switch
 * @DSI_CMD_SET_VID_TO_CMD_SWITCH:         Video to cmd mode switch
 * @DSI_CMD_SET_POST_VID_TO_CMD_SWITCH:    Post vid to cmd switch
 * @DSI_CMD_SET_PANEL_STATUS:              Panel status
 * @DSI_CMD_SET_LP1:                       Low power mode 1
 * @DSI_CMD_SET_LP2:                       Low power mode 2
 * @DSI_CMD_SET_NOLP:                      Low power mode disable
 * @DSI_CMD_SET_PPS:                       DSC PPS command
 * @DSI_CMD_SET_ROI:			   Panel ROI update
 * @DSI_CMD_SET_TIMING_SWITCH:             Timing switch
 * @DSI_CMD_SET_POST_TIMING_SWITCH:        Post timing switch
 * @DSI_CMD_SET_QSYNC_ON                   Enable qsync mode
 * @DSI_CMD_SET_QSYNC_OFF                  Disable qsync mode
 * @DSI_CMD_SET_MAX
 */
enum dsi_cmd_set_type {
	DSI_CMD_SET_PRE_ON = 0,
	DSI_CMD_SET_ON,
	DSI_CMD_SET_POST_ON,
	DSI_CMD_SET_PRE_OFF,
	DSI_CMD_SET_OFF,
	DSI_CMD_SET_POST_OFF,
	DSI_CMD_SET_PRE_RES_SWITCH,
	DSI_CMD_SET_RES_SWITCH,
	DSI_CMD_SET_POST_RES_SWITCH,
	DSI_CMD_SET_CMD_TO_VID_SWITCH,
	DSI_CMD_SET_POST_CMD_TO_VID_SWITCH,
	DSI_CMD_SET_VID_TO_CMD_SWITCH,
	DSI_CMD_SET_POST_VID_TO_CMD_SWITCH,
	DSI_CMD_SET_PANEL_STATUS,
	DSI_CMD_SET_LP1,
	DSI_CMD_SET_LP2,
	DSI_CMD_SET_NOLP,
	DSI_CMD_SET_PPS,
	DSI_CMD_SET_ROI,
	DSI_CMD_SET_TIMING_SWITCH,
	DSI_CMD_SET_POST_TIMING_SWITCH,
	DSI_CMD_SET_LOW_PERSIST_MODE_OFF,
	DSI_CMD_SET_LOW_PERSIST_MODE_ON,
	DSI_CMD_SET_QSYNC_ON,
	DSI_CMD_SET_QSYNC_OFF,
	DSI_CMD_SET_MAX
};

/**
 * enum dsi_cmd_set_state - command set state
 * @DSI_CMD_SET_STATE_LP:   dsi low power mode
 * @DSI_CMD_SET_STATE_HS:   dsi high speed mode
 * @DSI_CMD_SET_STATE_MAX
 */
enum dsi_cmd_set_state {
	DSI_CMD_SET_STATE_LP = 0,
	DSI_CMD_SET_STATE_HS,
	DSI_CMD_SET_STATE_MAX
};

enum lge_ddic_dsi_cmd_set_type {
	LGE_DDIC_DSI_SET_IMAGE_ENHANCEMENT = 0,
	LGE_DDIC_DSI_SET_BIST_ON,
	LGE_DDIC_DSI_SET_BIST_OFF,
	LGE_DDIC_DSI_SET_WB_DEFAULT,
	LGE_DDIC_DSI_SET_CM_DCI_P3,
	LGE_DDIC_DSI_SET_CM_SRGB,
	LGE_DDIC_DSI_SET_CM_ADOBE,
	LGE_DDIC_DSI_SET_CM_NATIVE,
	LGE_DDIC_DSI_DISP_CTRL_COMMAND_1,
	LGE_DDIC_DSI_DISP_CTRL_COMMAND_2,
	LGE_DDIC_DSI_DISP_DG_COMMAND_DUMMY,
	LGE_DDIC_DSI_BC_DIMMING,
	LGE_DDIC_DSI_BC_DEFAULT_DIMMING,
	LGE_DDIC_DSI_SET_VR_MODE_ON,
	LGE_DDIC_DSI_SET_VR_MODE_OFF,
	LGE_DDIC_DSI_SET_LP1,
	LGE_DDIC_DSI_SET_LP2,
	LGE_DDIC_DSI_SET_NOLP,
	LGE_DDIC_DSI_SET_SATURATION,
	LGE_DDIC_DSI_SET_HUE,
	LGE_DDIC_DSI_SET_SHARPNESS,
	LGE_DDIC_DSI_SET_SATURATION_DEFAULT,
	LGE_DDIC_DSI_SET_HUE_DEFAULT,
	LGE_DDIC_DSI_SET_SHARPNESS_DEFAULT,
	LGE_DDIC_DSI_CM_NATURAL,
	LGE_DDIC_DSI_CM_VIVID,
	LGE_DDIC_DSI_CM_CINEMA,
	LGE_DDIC_DSI_CM_SPORTS,
	LGE_DDIC_DSI_CM_GAME,
	LGE_DDIC_DSI_CM_PHOTO,
	LGE_DDIC_DSI_CM_WEB,
	LGE_DDIC_DSI_DETECT_VERT_LINE_RESTORE,
	LGE_DDIC_DSI_DETECT_BLACK_VERT_LINE,
	LGE_DDIC_DSI_DETECT_WHITE_VERT_LINE,
	LGE_DDIC_DSI_MEM_ERR_DETECT,
	LGE_DDIC_DSI_ESD_DETECT,
	LGE_DDIC_DSI_LINE_DEFECT_DETECT,
	LGE_DDIC_DSI_DISP_SC_COMMAND_DUMMY,
	LGE_DDIC_DSI_REGISTER_LOCK,
	LGE_DDIC_DSI_REGISTER_UNLOCK,
	LGE_DDIC_DSI_VIDEO_ENHANCEMENT_ON,
	LGE_DDIC_DSI_VIDEO_ENHANCEMENT_OFF,
	LGE_DDIC_DSI_HDR_SET_CTRL,
	LGE_DDIC_DSI_IRC_CTRL,
	LGE_DDIC_DSI_ACE_TUNE,
	LGE_DDIC_DSI_ACE_RESTORE,
	LGE_DDIC_DSI_DIGITAL_GAMMA_SET,
	LGE_DDIC_DSI_AOD_AREA,
	LGE_DDIC_DSI_DISP_CM_COMMAND_DUMMY,
	LGE_DDIC_DSI_DISP_CM_SET,
	LGE_DDIC_DSI_DISP_RGB_HUE_LUT,
	LGE_DDIC_DSI_DISP_SAT_LUT,
	LGE_DDIC_DSI_DISP_SHA_LUT,
	LGE_DDIC_DSI_DISP_TRUEVIEW_LUT,
	LGE_DDIC_DSI_BRIGHTNESS_CTRL_EXT_COMMAND,
	LGE_DDIC_DSI_TC_PERF_ON_COMMAND,
	LGE_DDIC_DSI_TC_PERF_OFF_COMMAND,
	LGE_DDIC_DSI_RGB_LUT,
	LGE_DDIC_DSI_ACE_LUT,
	LGE_DDIC_DSI_FP_LHBM_READY,
	LGE_DDIC_DSI_FP_LHBM_EXIT,
	LGE_DDIC_DSI_FP_LHBM_ON,
	LGE_DDIC_DSI_FP_LHBM_OFF,
	LGE_DDIC_DSI_FP_LHBM_AOD_TO_FPS,
	LGE_DDIC_DSI_FP_LHBM_FPS_TO_AOD,
	LGE_DDIC_DSI_BL_SET,
	LGE_DDIC_DSI_DAYLIGHT_ON,
	LGE_DDIC_DSI_DAYLIGHT_OFF,
	LGE_DDIC_DSI_ECC_STATUS_ON,
	LGE_DDIC_DSI_ECC_STATUS_OFF,
	LGE_DDIC_DSI_CMD_SET_MAX
};

enum lge_panel_lp_state {
	LGE_PANEL_NOLP = 0,
	LGE_PANEL_LP1,
	LGE_PANEL_LP2,
	LGE_PANEL_OFF,
	LGE_PANEL_STATE_MAX,
};

enum lge_irc_ctrl {
	LGE_IRC_ON = 0,
	LGE_IRC_OFF,
	LGE_IRC_MAX,
};

enum lge_irc_mode {
	LGE_GLOBAL_IRC_HBM = 0,
	LGE_GLOBAL_IRC_HDR,
	LGE_GLOBAL_IRC_MAX,
};

struct lge_dsi_color_manager_mode_entry {
	u32 color_manager_mode;
	u32 color_manager_status;
};

struct lge_rect {
	int x;
	int y;
	int w;
	int h;
};

struct dsi_cmd_desc {
    struct mipi_dsi_msg msg;
	bool last_command;
    u32  post_wait_ms;
};

struct dsi_panel_cmd_set {
	enum dsi_cmd_set_type type;
	enum dsi_cmd_set_state state;
	u32 count;
	u32 ctrl_idx;
	struct dsi_cmd_desc *cmds;
	struct mtk_panel_para_table *para_table;
};

struct lge_ddic_dsi_panel_cmd_set {
	enum lge_ddic_dsi_cmd_set_type type;
	enum dsi_cmd_set_state state;
	u32 count;
	u32 ctrl_idx;
	struct dsi_cmd_desc *cmds;
	struct mtk_panel_para_table *para_table;
};

struct lge_ddic_ops {
	/* For DISPLAY_AMBIENT */
	int (*store_aod_area)(struct mtk_dsi *dsi, struct lge_rect *rect);
	int (*prepare_aod_cmds)(struct mtk_dsi *dsi, struct dsi_cmd_desc *cmds, int cmds_count);
	void (*prepare_aod_area)(struct mtk_dsi *dsi, struct dsi_cmd_desc *cmds, int cmds_count);
	/* For DISPLAY_COLOR_MANAGER */
	void (*lge_display_control_store)(struct mtk_dsi *dsi, bool send_cmd, unsigned int need_lock);
	void (*lge_bc_dim_set)(struct mtk_dsi *dsi, u8 bc_dim_en, u8 bc_dim_f_cnt);
	int (*lge_set_therm_dim)(struct mtk_dsi *dsi, int input);
	int (*lge_get_brightness_dim)(struct mtk_dsi *dsi);
	void (*lge_set_brightness_dim)(struct mtk_dsi *dsi, int input);
	void (*lge_set_custom_rgb)(struct mtk_dsi *dsi, bool send_cmd, unsigned int need_lock);
	void (*lge_set_rgb_tune)(struct mtk_dsi *dsi, bool send_cmd);
	void (*lge_set_screen_tune)(struct mtk_dsi *dsi, unsigned int need_lock);
	void (*lge_set_screen_mode)(struct mtk_dsi *dsi, bool send_cmd, unsigned int need_lock);
	void (*lge_send_screen_mode_cmd)(struct mtk_dsi *dsi, int index);
	void (*lge_set_hdr_hbm_lut)(struct mtk_dsi *dsi, int input);
	void (*lge_set_hdr_mode)(struct mtk_dsi *dsi, int input);
	void (*lge_set_acl_mode)(struct mtk_dsi *dsi, int input);
	void (*lge_set_video_enhancement)(struct mtk_dsi *dsi, int input, unsigned int need_lock);
	void (*lge_set_ecc_status)(struct mtk_dsi *dsi, int input, unsigned int need_lock);
	void (*lge_vr_lp_mode_set)(struct mtk_dsi *dsi, int input, unsigned int need_lock);
	void (*sharpness_set)(struct mtk_dsi *dsi, int mode);
	void (*lge_set_true_view_mode)(struct mtk_dsi *dsi, bool send_cmd);
	int (*daylight_mode_set)(struct mtk_dsi *dsi, int input, unsigned int need_lock);
	int (*hdr_mode_set)(struct mtk_dsi *dsi, int input, unsigned int need_lock);
	int (*get_irc_state)(struct mtk_dsi *dsi);
	int (*set_irc_state)(struct mtk_dsi *dsi, enum lge_irc_mode mode, enum lge_irc_ctrl enable, unsigned int need_lock);
	int (*set_irc_default_state)(struct mtk_dsi *dsi);
	void (*set_dim_ctrl)(struct mtk_dsi *dsi, bool status);
	void (*lge_set_fp_lhbm)(struct mtk_dsi *dsi, int input, unsigned int need_lock);
	void (*lge_set_fp_lhbm_br_lvl)(struct mtk_dsi *dsi, int input);
	void (*lge_set_tc_perf)(struct mtk_dsi *dsi, int input, unsigned int need_lock);
	void (*lge_damping_mode_set)(struct mtk_dsi *dsi, int input);
	/* For DISPLAY_FACTORY */
	void (*lge_check_vert_black_line)(struct mtk_dsi *dsi);
	void (*lge_check_vert_white_line)(struct mtk_dsi *dsi);
	void (*lge_check_vert_line_restore)(struct mtk_dsi *dsi);

	/* For DISPLAY_ERR_DETECT */
	void (*err_detect_work)(struct work_struct *work);
	irqreturn_t (*err_detect_irq_handler)(int irq, void *data);
	int (*set_err_detect_mask)(struct mtk_dsi *dsi);

	/* For Display DRS */
	int (*bist_ctrl)(struct mtk_dsi *dsi, bool enable);
	int (*release_bist)(struct mtk_dsi *dsi);
	int (*get_current_res)(struct mtk_dsi *dsi);
	void (*get_support_res)(int idx, void* input);
	struct backup_info* (*get_reg_backup_list)(int *cnt);
	int (*set_pps_cmds)(struct mtk_dsi *dsi, enum lge_ddic_dsi_cmd_set_type type);
	int (*unset_pps_cmds)(struct mtk_dsi *dsi, enum lge_ddic_dsi_cmd_set_type type);
};

struct lge_blmap {
	int size;
	int *map;
};

struct lge_dsi_panel {
	int *pins;
	int pins_num;

	//struct lge_panel_pin_seq *panel_on_seq;
	//struct lge_panel_pin_seq *panel_off_seq;

	bool use_labibb;
	bool reset_after_ddvd;
	bool is_incell;
	bool use_panel_reset_low_before_lp11;
	bool use_extra_recovery_cmd;

	enum lge_panel_lp_state lp_state;
	enum lge_panel_lp_state panel_state;

	struct lge_blmap *blmap_list;
	int blmap_list_size;
	int default_brightness;
	bool dcs_brightness_be;
	bool use_bist;
	bool update_pps_in_lp;
	bool panel_dead;
	bool panel_dead_pending;
	//struct delayed_work panel_dead_work;
	int pps_orig;
	int bist_on;
	const char *bist_usage_type[MAX_BIST_USAGE_TYPE];
	struct mutex bist_lock;
	bool use_drs_mngr;
	bool use_internal_pps_switch;
	//struct lge_drs_mngr drs_mngr;
	bool use_ddic_reg_backup;
	bool ddic_reg_backup_complete;
	bool is_sent_bc_dim_set;
	int daylight_mode;
	int hdr_mode;
	int mfts_auto_touch;

	/* For DISPLAY_FACTORY */
	int use_line_detect;

//Display Bringup need to check is it need
	int screen_tune_status;
	int sc_sat_step;
	int sc_hue_step;
	int sc_sha_step;
	int color_filter;
	bool sharpness_control;

	/* For DISPLAY_COLOR_MANAGER */
	bool use_color_manager;
	bool use_color_manager_oled;
	bool dgc_absent;
	u8 move_wp;
	u8 dgc_status;
	u8 sharpness_status;
	int color_manager_status;
	int color_manager_mode;
	bool is_backup;
	int screen_mode;
	struct dsi_panel_cmd_set d65_default_cmds;
	int cm_preset_step;
	int cm_red_step;
	int cm_green_step;
	int cm_blue_step;
	struct lge_dsi_color_manager_mode_entry color_manager_table[NUM_COLOR_MODES];
	u32 color_manager_table_len;
	bool color_manager_default_status;
	struct dsi_panel_cmd_set dg_preset_cmds;
	int sharpness;
	int video_enhancement;
	int ecc_status;
	int hdr_hbm_lut;
	int ddic_hdr;
	int ve_hdr;
	int ace_hdr;
	int hbm_mode;
	int acl_mode;
	bool is_cm_reg_backup;
	u8 bc_dim_en;
	u8 bc_dim_f_cnt;
	struct delayed_work bc_dim_work;
	bool use_bc_dimming_work;
	bool use_vr_lp_mode;
	int vr_lp_mode;
	bool use_dim_ctrl;
	bool use_fp_lhbm;
	bool need_fp_lhbm_set;
	int fp_lhbm_mode;
	int old_fp_lhbm_mode;
	int fp_lhbm_br_lvl;
	int old_panel_fp_mode;
	int forced_lhbm;
	bool use_tc_perf;
	int tc_perf;
	bool lhbm_ready_enable;

	bool use_damping_mode;
	int damping_hdr;

	bool true_view_supported;
	u32 true_view_mode;

	bool use_irc_ctrl;
	bool irc_pending;
	int irc_request_state;
	int irc_current_state;
	bool use_ace_ctrl;
	int ace_mode;

	bool use_dynamic_brightness;
	int brightness_table;

	struct lge_ddic_ops *ddic_ops;

	/* FOR DISPLAY BACKLIGT CONTROL */
	bool allow_bl_update;
	int bl_lvl_unset;
	int bl_lvl_recovery_unset;
	bool use_dcs_brightness_short;
	unsigned int bl_level;
	unsigned int bl_user_level;

	/* For DISPLAY_AMBIENT */
	bool use_ambient;
	bool use_cmd_wait_pa_changed;
	bool wait_pa_changed;
	struct completion pa_changed_done;
	bool allow_bl_update_ex;
	int bl_ex_lvl_unset;
	struct backlight_device *bl_ex_device;
	struct lge_rect aod_area;
	struct mutex pa_changed_lock;
	bool partial_area_vertical_changed;
	bool partial_area_horizontal_changed;
	bool partial_area_height_changed;
	bool aod_power_mode;
	bool use_br_ctrl_ext;

	u32 aod_interface;
	const char *aod_interface_type[3];

#ifdef CONFIG_LGE_DUAL_SCREEN
	/*For Cover Display */
	struct backlight_device *bl_cover_device;
	int bl_cover_lvl_unset;
	int br_offset;
	bool br_offset_update;
	bool br_offset_bypass;
#endif /* CONFIG_LGE_DUAL_SCREEN */

	/* For DISPLAY_ERR_DETECT */
	bool use_panel_err_detect;
	bool err_detect_crash_enabled;
	bool err_detect_irq_enabled;
	bool is_first_err_mask;
	int err_detect_gpio;
	int err_detect_result;
	int err_detect_mask;
	struct workqueue_struct *err_detect_int_workq;
	struct delayed_work err_detect_int_work;
	struct workqueue_struct *bl_workq;
	struct delayed_work bl_work;

	bool use_ddic_reg_lock;
	struct lge_ddic_dsi_panel_cmd_set lge_cmd_sets[LGE_DDIC_DSI_CMD_SET_MAX];
	struct dsi_panel_cmd_set read_cmds;

	bool support_display_scenario;
	bool lge_tune_brightness;
	bool lge_tune_dsi_params;
	bool lge_tune_dsi_phy_params;
	bool lge_tune_init_cmd;
#if IS_ENABLED(CONFIG_LGE_DISPLAY_REFRESH_RATE_DIV)
        int refresh_rate_div;
#endif
};
#endif

struct mtk_panel_ext {
	struct mtk_panel_funcs *funcs;
	struct mtk_panel_params *params;
#ifdef CONFIG_LGE_DISPLAY_COMMON
	struct mutex panel_lock;
	struct mutex brightness_lock;
	struct lge_dsi_panel lge;
#endif
};

struct mtk_panel_ctx {
	struct drm_panel *panel;
	struct mtk_panel_ext *ext;

	struct list_head list;
};

struct mtk_panel_funcs {
	int (*set_backlight_cmdq)(void *dsi_drv, dcs_write_gce cb,
		void *handle, unsigned int level);
	int (*set_aod_light_mode)(void *dsi_drv, dcs_write_gce cb,
		void *handle, unsigned int mode);
	int (*set_backlight_grp_cmdq)(void *dsi_drv, dcs_grp_write_gce cb,
		void *handle, unsigned int level);
	int (*reset)(struct drm_panel *panel, int on);
	int (*ata_check)(struct drm_panel *panel);
	int (*ext_param_set)(struct drm_panel *panel, unsigned int mode);
	int (*ext_param_get)(struct mtk_panel_params *ext_para,
		unsigned int mode);
	int (*mode_switch)(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage);
	int (*get_virtual_heigh)(void);
	int (*get_virtual_width)(void);
	/**
	 * @doze_enable_start:
	 *
	 * Call the @doze_enable_start before starting AOD mode.
	 * The LCM off may add here to avoid panel show unexpected
	 * content when switching to specific panel low power mode.
	 */
	int (*doze_enable_start)(struct drm_panel *panel,
		void *dsi_drv, dcs_write_gce cb, void *handle);

	/**
	 * @doze_enable:
	 *
	 * Call the @doze_enable starts AOD mode.
	 */
	int (*doze_enable)(struct drm_panel *panel,
		void *dsi_drv, dcs_write_gce cb, void *handle);

	/**
	 * @doze_disable:
	 *
	 * Call the @doze_disable before ending AOD mode.
	 */
	int (*doze_disable)(struct drm_panel *panel,
		void *dsi_drv, dcs_write_gce cb, void *handle);

	/**
	 * @doze_post_disp_on:
	 *
	 * In some situation, the LCM off may set in @doze_enable & @disable.
	 * After LCM switch to the new mode stable, system call
	 * @doze_post_disp_on to turn on panel.
	 */
	int (*doze_post_disp_on)(struct drm_panel *panel,
		void *dsi_drv, dcs_write_gce cb, void *handle);

	/**
	 * @doze_area:
	 *
	 * Send the panel area in command here.
	 */
	int (*doze_area)(struct drm_panel *panel,
		void *dsi_drv, dcs_write_gce cb, void *handle);

	/**
	 * @doze_get_mode_flags:
	 *
	 * If CV switch is needed for doze mode, fill the mode_flags in this
	 * function for both CMD and VDO mode.
	 */
	unsigned long (*doze_get_mode_flags)(
		struct drm_panel *panel, int aod_en);

	int (*hbm_set_cmdq)(struct drm_panel *panel, void *dsi_drv,
			    dcs_write_gce cb, void *handle, bool en);
	void (*hbm_get_state)(struct drm_panel *panel, bool *state);
	void (*hbm_get_wait_state)(struct drm_panel *panel, bool *wait);
	bool (*hbm_set_wait_state)(struct drm_panel *panel, bool wait);
#ifdef CONFIG_LGE_DISPLAY_COMMON
#ifdef CONFIG_LGE_DISPLAY_TUNING_SUPPORT
	struct mtk_panel_para_table* (*get_lcm_init_cmd_str)(void);
	int (*get_init_cmd_str_size)(void);
#endif
	int (*set_backlight_cmdq_ex_direct)(struct drm_panel *drm_panel, struct mtk_panel_ext *panel, unsigned int level);
	int (*panel_tx_cmd_set)(struct drm_panel *panel, struct mtk_dsi *dsi, enum lge_ddic_dsi_cmd_set_type type);
	int (*set_panel_power_pre)(struct drm_panel *drm_panel);
	int (*set_panel_power_off_post)(struct drm_panel *drm_panel);
#endif
};

void mtk_panel_init(struct mtk_panel_ctx *ctx);
void mtk_panel_add(struct mtk_panel_ctx *ctx);
void mtk_panel_remove(struct mtk_panel_ctx *ctx);
int mtk_panel_attach(struct mtk_panel_ctx *ctx, struct drm_panel *panel);
int mtk_panel_detach(struct mtk_panel_ctx *ctx);
struct mtk_panel_ext *find_panel_ext(struct drm_panel *panel);
int mtk_panel_ext_create(struct device *dev,
			 struct mtk_panel_params *ext_params,
			 struct mtk_panel_funcs *ext_funcs,
			 struct drm_panel *panel);
int mtk_panel_tch_handle_reg(struct drm_panel *panel);
void **mtk_panel_tch_handle_init(void);
int mtk_panel_tch_rst(struct drm_panel *panel);

#endif
