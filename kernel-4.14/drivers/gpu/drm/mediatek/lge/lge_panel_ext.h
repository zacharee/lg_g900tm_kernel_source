#ifndef __LGE_PANEL_EXT_H__
#define __LGE_PANEL_EXT_H__

#include <linux/irqreturn.h>
#include <drm/drm_mipi_dsi.h>

#define DEFAULT_HBM_MODE 0x03;
#define FP_LHBM_OFF 0
#define FP_LHBM_ON 1
#define FP_LHBM_DEFAULT_BR_LVL	4095

#define NUM_COLOR_MODES  10
#define MAX_BIST_USAGE_TYPE 5

#define MTK_DSI_POWER_MODE_ON	0
#define MTK_DSI_POWER_MODE_LP1	1
#define MTK_DSI_POWER_MODE_LP2	2
#define MTK_DSI_POWER_MODE_OFF	3

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

struct mtk_panel_ext;

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
	int (*store_aod_area)(struct mtk_panel_ext *panel, struct lge_rect *rect);
	int (*prepare_aod_cmds)(struct mtk_panel_ext *panel, struct dsi_cmd_desc *cmds, int cmds_count);
	int (*prepare_aod_area)(struct mtk_panel_ext *panel, struct dsi_cmd_desc *cmds, int cmds_count);
	/* For DISPLAY_COLOR_MANAGER */
	int (*lge_display_control_store)(struct mtk_panel_ext *panel, bool send_cmd, unsigned int need_lock);
	int (*lge_bc_dim_set)(struct mtk_panel_ext *panel, u8 bc_dim_en, u8 bc_dim_f_cnt);
	int (*lge_set_therm_dim)(struct mtk_panel_ext *panel, int input);
	int (*lge_get_brightness_dim)(struct mtk_panel_ext *panel);
	int (*lge_set_brightness_dim)(struct mtk_panel_ext *panel, int input);
	int (*lge_set_custom_rgb)(struct mtk_panel_ext *panel, bool send_cmd, unsigned int need_lock);
	int (*lge_set_rgb_tune)(struct mtk_panel_ext *panel, bool send_cmd);
	int (*lge_set_screen_tune)(struct mtk_panel_ext *panel, unsigned int need_lock);
	int (*lge_set_screen_mode)(struct mtk_panel_ext *panel, bool send_cmd, unsigned int need_lock);
	int (*lge_send_screen_mode_cmd)(struct mtk_panel_ext *panel, int index);
	int (*lge_set_hdr_hbm_lut)(struct mtk_panel_ext *panel, int input);
	int (*lge_set_hdr_mode)(struct mtk_panel_ext *panel, int input);
	int (*lge_set_acl_mode)(struct mtk_panel_ext *panel, int input);
	int (*lge_set_video_enhancement)(struct mtk_panel_ext *panel, int input, unsigned int need_lock);
	int (*lge_set_ecc_status)(struct mtk_panel_ext *panel, int input, unsigned int need_lock);
	int (*lge_vr_lp_mode_set)(struct mtk_panel_ext *panel, int input, unsigned int need_lock);
	int (*sharpness_set)(struct mtk_panel_ext *panel, int mode);
	int (*lge_set_true_view_mode)(struct mtk_panel_ext *panel, bool send_cmd);
	int (*daylight_mode_set)(struct mtk_panel_ext *panel, int input, unsigned int need_lock);
	int (*hdr_mode_set)(struct mtk_panel_ext *panel, int input, unsigned int need_lock);
	int (*get_irc_state)(struct mtk_panel_ext *panel);
	int (*set_irc_state)(struct mtk_panel_ext *panel, enum lge_irc_mode mode, enum lge_irc_ctrl enable, unsigned int need_lock);
	int (*set_irc_default_state)(struct mtk_panel_ext *panel);
	int (*set_dim_ctrl)(struct mtk_panel_ext *panel, bool status);
	int (*lge_set_fp_lhbm)(struct mtk_panel_ext *panel, int input, unsigned int need_lock);
	int (*lge_set_fp_lhbm_br_lvl)(struct mtk_panel_ext *panel, int input);
	int (*lge_set_tc_perf)(struct mtk_panel_ext *panel, int input, unsigned int need_lock);
	int (*lge_damping_mode_set)(struct mtk_panel_ext *panel, int input);
	/* For DISPLAY_FACTORY */
	int (*lge_check_vert_black_line)(struct mtk_panel_ext *panel);
	int (*lge_check_vert_white_line)(struct mtk_panel_ext *panel);
	int (*lge_check_vert_line_restore)(struct mtk_panel_ext *panel);

	/* For DISPLAY_ERR_DETECT */
	void (*err_detect_work)(struct work_struct *work);
	irqreturn_t (*err_detect_irq_handler)(int irq, void *data);
	int (*set_err_detect_mask)(struct mtk_panel_ext *panel);

	/* For Display DRS */
	int (*bist_ctrl)(struct mtk_panel_ext *panel, bool enable);
	int (*release_bist)(struct mtk_panel_ext *panel);
	int (*get_current_res)(struct mtk_panel_ext *panel);
	void (*get_support_res)(int idx, void* input);
	struct backup_info* (*get_reg_backup_list)(int *cnt);
	int (*set_pps_cmds)(struct mtk_panel_ext *panel, enum lge_ddic_dsi_cmd_set_type type);
	int (*unset_pps_cmds)(struct mtk_panel_ext *panel, enum lge_ddic_dsi_cmd_set_type type);

	/* tx cmd set */
	int (*panel_tx_cmd_set)(struct mtk_panel_ext *panel_ext, enum lge_ddic_dsi_cmd_set_type type);
};

struct lge_blmap {
	int size;
	int *map;
};

struct lge_dsi_panel {
	struct mipi_dsi_host *dsi_host;
	struct drm_panel *drm_panel;

	int *pins;
	int pins_num;

	//struct lge_panel_pin_seq *panel_on_seq;
	//struct lge_panel_pin_seq *panel_off_seq;

	bool use_labibb;
	bool reset_after_ddvd;
	bool is_incell;
	bool use_panel_reset_low_before_lp11;
	bool use_extra_recovery_cmd;

	int output_pwr_mode;
	int last_output_pwr_mode;

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
	bool use_br_ess_table;
	int *br_ess_table;
};

#define LGE_DDIC_OP(c,op, ...) ((c && (c->op))?(c->op(__VA_ARGS__)):0)

#endif // __LGE_PANEL_EXT_H__
