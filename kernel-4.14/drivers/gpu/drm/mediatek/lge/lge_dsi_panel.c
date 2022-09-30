#define pr_fmt(fmt)	"[Display][lge-dsi-panel:%s:%d] " fmt, __func__, __LINE__

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include <drm/drm_panel.h>
#include <drm/drm_mode.h>
#include <drm/drm_mipi_dsi.h>
#include "lge_dsi_panel.h"
#include "mtk_panel_ext.h"
#include "mtk_drm_crtc.h"
#include "../mtk_dsi.h"
#include "../mtk_debug.h"
//#include <soc/mediatek/lge/board_lge.h>
//#include "lge_ddic_ops_helper.h"
#include "cm/lge_color_manager.h"
#include "brightness/lge_brightness.h"
#include "cover/lge_cover_ctrl.h"
#ifdef CONFIG_LGE_DISPLAY_TUNING_SUPPORT
#include "lge_tuning.h"
#endif
#include "factory/lge_factory.h"
#include "err_detect/lge_err_detect.h"
#ifdef CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY
#include <linux/lge_panel_notify.h>
#endif

extern int lge_ddic_dsi_panel_tx_cmd_set(struct mtk_dsi *dsi,
				enum lge_ddic_dsi_cmd_set_type type, unsigned int need_lock);
extern int lge_ddic_dsi_panel_parse_cmd_sets(struct mtk_dsi *dsi,
	struct device_node *of_node);
extern void lge_ddic_ops_init(struct mtk_panel_ext *panel);
extern void lge_ddic_feature_init(struct mtk_dsi *dsi);
extern void lge_ambient_create_sysfs(struct mtk_dsi *dsi, struct class *class_panel);
extern int lge_ambient_set_interface_data(struct mtk_panel_ext *panel);
extern void lge_panel_reg_create_sysfs(struct mtk_dsi *dsi, struct class *class_panel);
extern bool is_bist_supported(struct mtk_panel_ext *panel, const char type[]);
extern void lge_cover_create_sysfs(void);

#define MAN_NAME_LEN    10
#define	DDIC_NAME_LEN	15

static char lge_man_name[MAN_NAME_LEN+1];
static char lge_ddic_name[DDIC_NAME_LEN+1];
static const char *LPNAME[] = { "NOLP", "LP1", "LP2", "OFF", "MAX"};

#ifdef CONFIG_LGE_DISPLAY_DIMMING_BOOT_SUPPORT
/*---------------------------------------------------------------------------*/
/* LCD off & dimming                                                         */
/*---------------------------------------------------------------------------*/
static bool fb_blank_called;

bool is_blank_called(void)
{
	return fb_blank_called;
}

int lge_get_bootreason_with_lcd_dimming(void)
{
	int ret = 0;

	if (lge_get_bootreason() == 0x23)
		ret = 1;
	else if (lge_get_bootreason() == 0x24)
		ret = 2;
	else if (lge_get_bootreason() == 0x25)
		ret = 3;
	return ret;
}

bool is_factory_cable(void)
{
	cable_boot_type cable_info = lge_get_boot_cable();

	if (cable_info == LT_CABLE_56K ||
		cable_info == LT_CABLE_130K ||
		cable_info == LT_CABLE_910K)
		return true;
	return false;
}

static void lge_set_blank_called(void)
{
	fb_blank_called = true;
}
#endif

static int lge_dsi_panel_mode_set(struct mtk_dsi *dsi)
{
	struct mtk_panel_ext *panel = NULL;
	//bool reg_backup_cond = false;
	int tc_perf_status = 0;

	if (!dsi || !dsi->ext) {
		pr_err("invalid params\n");
		return -EINVAL;
	}
	panel = dsi->ext;

	if (panel->lge.ecc_status && panel->lge.ddic_ops && panel->lge.ddic_ops->lge_set_ecc_status)
		panel->lge.ddic_ops->lge_set_ecc_status(dsi, panel->lge.ecc_status,0);
#if 0
	reg_backup_cond = !(panel->lge.use_ddic_reg_backup^panel->lge.ddic_reg_backup_complete);
	pr_info("backup=%d\n", reg_backup_cond);
#endif
	if (panel->lge.use_color_manager) {
		if (panel->lge.ddic_ops &&
				panel->lge.ddic_ops->lge_set_screen_mode)
			panel->lge.ddic_ops->lge_set_screen_mode(dsi, true, 0);

		if (panel->lge.ddic_ops &&
				panel->lge.ddic_ops->lge_bc_dim_set)
			panel->lge.ddic_ops->lge_bc_dim_set(dsi, BC_DIM_ON, BC_DIM_FRAMES_NORMAL);
		panel->lge.is_sent_bc_dim_set = true;
	} else {
		pr_warn("skip ddic mode set on booting or not supported!\n");
	}

	if(panel->lge.use_tc_perf) {
		tc_perf_status = panel->lge.tc_perf;
		if (tc_perf_status && panel->lge.ddic_ops &&
				panel->lge.ddic_ops->lge_set_tc_perf) {
			panel->lge.ddic_ops->lge_set_tc_perf(dsi, tc_perf_status, 0);
		}
	}

	return 0;
}

static inline bool cmd_set_exists(struct mtk_panel_ext *panel, enum lge_ddic_dsi_cmd_set_type type)
{
	if (!panel)
		return false;
	return (panel->lge.lge_cmd_sets[type].count != 0);
}

char* get_ddic_name(void)
{
	return lge_ddic_name;
}

bool is_ddic_name(char *ddic_name)
{
	if (ddic_name == NULL) {
		pr_err("input parameter is NULL\n");
		return false;
	}

	if(!strcmp(lge_ddic_name, ddic_name)) {
		return true;
	}
	pr_err("input ddic_name = %s, lge_ddic = %s\n", ddic_name, lge_ddic_name);
	return false;
}
EXPORT_SYMBOL(is_ddic_name);

static bool dsi_panel_need_mask(struct mtk_panel_ext *panel)
{
	if (panel->lge.partial_area_vertical_changed)
		return true;

	if (panel->lge.partial_area_height_changed &&
			panel->lge.update_pps_in_lp)
		return true;

	return false;
}

static void set_aod_area(struct mtk_dsi *dsi)
{
	struct mtk_panel_ext *panel = NULL;
	int rc = 0;

	if (!dsi || !dsi->ext) {
		pr_err("invalid params\n");
		return;
	}

	panel = dsi->ext;

	if (panel->lge.ddic_ops && panel->lge.ddic_ops->prepare_aod_area) {
		panel->lge.ddic_ops->prepare_aod_area(dsi,
				panel->lge.lge_cmd_sets[LGE_DDIC_DSI_AOD_AREA].cmds,
				panel->lge.lge_cmd_sets[LGE_DDIC_DSI_AOD_AREA].count);
	}

	rc = lge_ddic_dsi_panel_tx_cmd_set(dsi, LGE_DDIC_DSI_AOD_AREA, 0);
	if (rc) {
		pr_err("[%s] failed to send cmd, rc=%d\n",
		       __func__, rc);
	}

	return;
}

static void set_lp(struct mtk_dsi *dsi, enum lge_panel_lp_state lp_state, enum lge_ddic_dsi_cmd_set_type cmd_set_type)
{
	struct mtk_panel_ext *panel = NULL;
	struct drm_panel *drm_panel = NULL;
	int rc = 0;

	printk("[%s] cmd_set_type = %d\n", __func__, cmd_set_type);

	if (!dsi || !dsi->ext || !dsi->panel) {
		pr_err("invalid params\n");
		return;
	}

	panel = dsi->ext;
	drm_panel = dsi->panel;

	if (panel->lge.panel_state == lp_state) {
		pr_err("already %s state\n", LPNAME[lp_state]);
		return;
	}

	if (!cmd_set_exists(panel, cmd_set_type)) {
		pr_err("No %s cmd\n", LPNAME[lp_state]);
		return;
	}

	rc = panel->funcs->panel_tx_cmd_set(drm_panel, dsi, cmd_set_type);
	if (rc) {
		pr_err("[%s] failed to send %d cmd, rc=%d\n",
		       __func__, cmd_set_type, rc);
	} else {
		panel->lge.panel_state = lp_state;
		pr_info("sent %s cmd\n", LPNAME[lp_state]);
	}

	return;
}

static int dsi_panel_get_current_power_mode(struct mtk_dsi *dsi)
{
	if (!dsi) {
		pr_err("invalid panel param\n");
		return -EINVAL;
	}

	return dsi->output_pwr_mode;
}

static inline bool is_power_off(int pwr_mode)
{
	return (pwr_mode == MTK_DSI_POWER_MODE_OFF);
}

static inline bool is_power_on_interactive(int pwr_mode)
{
	return (pwr_mode == MTK_DSI_POWER_MODE_ON);
}

static inline bool is_power_on(int pwr_mode)
{
	return !is_power_off(pwr_mode);
}

static inline bool is_power_on_lp(int pwr_mode)
{
	return !is_power_off(pwr_mode) &&
		!is_power_on_interactive(pwr_mode);
}

static inline bool is_power_on_ulp(int pwr_mode)
{
	return (pwr_mode == MTK_DSI_POWER_MODE_LP2);
}

bool lge_dsi_panel_is_power_off(struct mtk_dsi *dsi)
{
	int last_dsi_power_mode = dsi_panel_get_current_power_mode(dsi);
	return is_power_off(last_dsi_power_mode);
}

bool lge_dsi_panel_is_power_on_interactive(struct mtk_dsi *dsi)
{
	int last_dsi_power_mode = dsi_panel_get_current_power_mode(dsi);
	return is_power_on_interactive(last_dsi_power_mode);
}

bool lge_dsi_panel_is_power_on(struct mtk_dsi *dsi)
{
	int last_dsi_power_mode = dsi_panel_get_current_power_mode(dsi);
	return is_power_on(last_dsi_power_mode);
}

bool lge_dsi_panel_is_power_on_lp(struct mtk_dsi *dsi)
{
	int last_dsi_power_mode = dsi_panel_get_current_power_mode(dsi);
	return is_power_on_lp(last_dsi_power_mode);
}

bool lge_dsi_panel_is_power_on_ulp(struct mtk_dsi *dsi)
{
	int last_dsi_power_mode = dsi_panel_get_current_power_mode(dsi);
	return is_power_on_ulp(last_dsi_power_mode);
}

bool is_need_lhbm_recovery(struct mtk_panel_ext *panel,
				enum lge_ddic_dsi_cmd_set_type cmd_type)
{
	int ret = false;

	if ((panel->lge.use_fp_lhbm) && (panel->lge.ddic_ops)
		&& (panel->lge.ddic_ops->lge_set_fp_lhbm)
		&& ((cmd_type == LGE_DDIC_DSI_SET_LP2)
			|| (cmd_type == LGE_DDIC_DSI_SET_LP1)
			|| (cmd_type == LGE_DDIC_DSI_SET_NOLP))
		&& ((panel->lge.old_fp_lhbm_mode == LGE_FP_LHBM_SM_ON)
			|| (panel->lge.old_fp_lhbm_mode == LGE_FP_LHBM_ON)
			|| (panel->lge.old_fp_lhbm_mode == LGE_FP_LHBM_READY)
			|| (panel->lge.lhbm_ready_enable == true)))
		ret = true;

	return ret;
}

static enum lge_ddic_dsi_cmd_set_type dsi_panel_select_cmd_type(struct mtk_dsi *dsi)
{
	struct mtk_panel_ext *panel = NULL;
	int rc;
	int last_panel_power_mode;
	enum lge_ddic_dsi_cmd_set_type type = LGE_DDIC_DSI_CMD_SET_MAX;

	if (!dsi || !dsi->ext) {
		pr_err("invalid panel param\n");
		return -EINVAL;
	}

	panel = dsi->ext;

	last_panel_power_mode = dsi->last_output_pwr_mode;
	if (last_panel_power_mode < 0) {
		pr_err("fail to get last_panel_pwr_mode\n");
		goto exit;
	}
	mutex_lock(&panel->lge.pa_changed_lock);
	if (!is_power_on_lp(last_panel_power_mode)) {
		if (is_power_off(last_panel_power_mode) &&
				panel->lge.use_cmd_wait_pa_changed &&
				((panel->lge.aod_area.w == 0) ||
				(panel->lge.aod_area.h == 0))) {
			init_completion(&panel->lge.pa_changed_done);
			panel->lge.wait_pa_changed = true;
			mutex_unlock(&panel->lge.pa_changed_lock);
			rc = wait_for_completion_timeout(&panel->lge.pa_changed_done, 2000);
			if (rc <= 0) {
				pr_warn("aod image will be displayed by default setting\n");
			}
			mutex_lock(&panel->lge.pa_changed_lock);
		}
		type = LGE_DDIC_DSI_SET_LP2;
	} else if (panel->lge.partial_area_vertical_changed ||
			panel->lge.partial_area_height_changed) {
		type = LGE_DDIC_DSI_AOD_AREA;
	} else {
		pr_debug("skip command\n");
	}
	mutex_unlock(&panel->lge.pa_changed_lock);

exit:
	pr_info("select_cmd=%d\n", type);
	return type;
}

static int dsi_panel_send_lp_cmds(struct mtk_dsi *dsi,
				enum lge_ddic_dsi_cmd_set_type cmd_type)
{
	struct mtk_panel_ext *panel = NULL;
	int rc = 0;
	bool need_mask = true;
	bool need_prepare = true;
	char *bist_name;
	enum lge_panel_lp_state panel_state = LGE_PANEL_STATE_MAX;

	printk("[%s] cmd_type = %d\n", __func__, cmd_type);

	if (!dsi || !dsi->ext)
		return -EINVAL;

	panel = dsi->ext;

	if (is_need_lhbm_recovery(panel, cmd_type)) {
		panel->lge.fp_lhbm_mode = LGE_FP_LHBM_EXIT;
		panel->lge.ddic_ops->lge_set_fp_lhbm(dsi, panel->lge.fp_lhbm_mode, 0);
	}

	switch (cmd_type) {
	case LGE_DDIC_DSI_SET_LP1:
		bist_name = "lp1";
		panel_state = LGE_PANEL_LP1;
		break;
	case LGE_DDIC_DSI_SET_LP2:
		bist_name = "lp2";
		panel_state = LGE_PANEL_LP2;
		break;
	case LGE_DDIC_DSI_SET_NOLP:
		bist_name = "nolp";
		panel_state = LGE_PANEL_NOLP;
		need_prepare = false;
		break;
	case LGE_DDIC_DSI_AOD_AREA:
		bist_name ="aod_area";
		break;
	default:
		bist_name = "none";
		break;
	};

	mutex_lock(&panel->lge.pa_changed_lock);

	if (cmd_type == LGE_DDIC_DSI_CMD_SET_MAX)
		goto exit;

	need_mask = dsi_panel_need_mask(panel);

	/* 1. masking */
	if (is_bist_supported(panel, bist_name) && need_mask &&
			(panel->lge.ddic_ops && panel->lge.ddic_ops->bist_ctrl)) {
		mutex_lock(&panel->lge.bist_lock);
		if (panel->lge.ddic_ops->bist_ctrl(dsi, true) < 0)
			pr_err("fail to control BIST\n");
		mutex_unlock(&panel->lge.bist_lock);
	}

	/* 2. send lp command */
	mutex_lock(&panel->panel_lock);

	/* UNLOCK REGISTER */
	if (panel->lge.use_ddic_reg_lock)
		lge_ddic_dsi_panel_tx_cmd_set(dsi, LGE_DDIC_DSI_REGISTER_UNLOCK, 0);

	if (need_prepare) {
		set_aod_area(dsi);

		if (cmd_type != LGE_DDIC_DSI_AOD_AREA) {
			if (panel->lge.ddic_ops && panel->lge.ddic_ops->prepare_aod_cmds) {
				panel->lge.ddic_ops->prepare_aod_cmds(dsi,
						panel->lge.lge_cmd_sets[cmd_type].cmds,
						panel->lge.lge_cmd_sets[cmd_type].count);
			}
		}
	}

	if(cmd_type != LGE_DDIC_DSI_AOD_AREA)
		set_lp(dsi, panel_state, cmd_type);

	/* LOCK REGISTER */
	if (panel->lge.use_ddic_reg_lock)
		lge_ddic_dsi_panel_tx_cmd_set(dsi, LGE_DDIC_DSI_REGISTER_LOCK, 0);

	mutex_unlock(&panel->panel_lock);
#if 0
	/* 3. update pps */
	if (panel->lge.update_pps_in_lp) {
		if (panel->lge.ddic_ops && panel->lge.ddic_ops->set_pps_cmds) {
			rc = panel->lge.ddic_ops->set_pps_cmds(panel, cmd_type);
			if (rc) {
				pr_warn("WARNING: fail to update pps info\n");
			}
		}

		rc = dsi_panel_update_pps(panel);
		if (rc)
			pr_err("fail to send pps\n");

		if (panel->lge.ddic_ops && panel->lge.ddic_ops->unset_pps_cmds) {
			rc = panel->lge.ddic_ops->unset_pps_cmds(panel, cmd_type);
			if (rc) {
				pr_warn("WARNING: fail to unset pps info\n");
			}
		}
	}
#endif

	if (panel->lge.forced_lhbm == true)
		panel->lge.ddic_ops->lge_set_fp_lhbm(dsi, panel->lge.fp_lhbm_mode, 0);

	/* 4. un-masking */
	if ((is_bist_supported(panel, bist_name)) && need_mask &&
			(panel->lge.ddic_ops && panel->lge.ddic_ops->bist_ctrl)) {
		mutex_lock(&panel->lge.bist_lock);
		if (panel->lge.ddic_ops->bist_ctrl(dsi, false) < 0)
			pr_err("fail to control BIST\n");
		mutex_unlock(&panel->lge.bist_lock);
	}

	panel->lge.partial_area_vertical_changed = false;
	panel->lge.partial_area_height_changed = false;
exit:
	mutex_unlock(&panel->lge.pa_changed_lock);

	return rc;
}

static int dsi_panel_update_lp_state(struct mtk_dsi *dsi, enum lge_panel_lp_state new)
{
	struct mtk_panel_ext *panel = NULL;

	if (!dsi || !dsi->ext)
		return -EINVAL;

	panel = dsi->ext;
	panel->lge.lp_state = new;
	dsi->last_output_pwr_mode = new;

	return 0;
}

/* @Override */
int dsi_panel_set_lp2(struct mtk_dsi *dsi)
{
	struct mtk_panel_ext *panel = NULL;
	int rc;
	enum lge_ddic_dsi_cmd_set_type cmd_type;

	printk("[%s] start\n", __func__);

	if (!dsi || !dsi->ext)
		return -EINVAL;

	panel = dsi->ext;

	cmd_type = dsi_panel_select_cmd_type(dsi);

	rc = dsi_panel_send_lp_cmds(dsi, cmd_type);
	if (rc < 0) {
		pr_err("fail to send lp command\n");
	}

	if ((cmd_type == LGE_DDIC_DSI_CMD_SET_MAX) || (cmd_type == LGE_DDIC_DSI_AOD_AREA))
		panel->lge.panel_state = LGE_PANEL_LP2;
	rc = dsi_panel_update_lp_state(dsi, LGE_PANEL_LP2);
	if (rc < 0) {
		pr_err("fail to update lp state\n");
	}
#ifdef CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY
	else {
		lge_panel_notifier_call_chain(LGE_PANEL_EVENT_BLANK,
				0, LGE_PANEL_STATE_LP2); /* U2_UNBLANK; DOZE */
	}
#endif

#ifdef CONFIG_LGE_DISPLAY_DIMMING_BOOT_SUPPORT
	lge_set_blank_called();
#endif
	return rc;
}

/* @Override */
int dsi_panel_set_lp1(struct mtk_dsi *dsi)
{
	struct mtk_panel_ext *panel = NULL;
	int rc;
	enum lge_ddic_dsi_cmd_set_type cmd_type;

	printk("[%s] start\n", __func__);

	if (!dsi || !dsi->ext)
		return -EINVAL;

	panel = dsi->ext;

	cmd_type = dsi_panel_select_cmd_type(dsi);

	rc = dsi_panel_send_lp_cmds(dsi, cmd_type);
	if (rc < 0) {
		pr_err("fail to send lp command\n");
	}

	if ((cmd_type == LGE_DDIC_DSI_CMD_SET_MAX) || (cmd_type == LGE_DDIC_DSI_AOD_AREA))
		panel->lge.panel_state = LGE_PANEL_LP1;
	rc = dsi_panel_update_lp_state(dsi, LGE_PANEL_LP1);
	if (rc < 0) {
		pr_err("update lp state\n");
	}
#ifdef CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY
	else {
		lge_panel_notifier_call_chain(LGE_PANEL_EVENT_BLANK,
				0, LGE_PANEL_STATE_LP1); /* U2_BLANK; DOZE_SUSPEND */
	}
#endif

#ifdef CONFIG_LGE_DISPLAY_DIMMING_BOOT_SUPPORT
	lge_set_blank_called();
#endif
	return rc;
}

/* @Override */
int dsi_panel_set_nolp(struct mtk_dsi *dsi)
{
	int rc;
	struct mtk_panel_ext *panel = NULL;
	int dsi_pwr_mode, last_panel_power_mode;

	printk("[%s] start\n", __func__);

	if (!dsi || !dsi->ext)
		return -EINVAL;

	panel = dsi->ext;
	dsi_pwr_mode = dsi->output_pwr_mode; // new power mode
	last_panel_power_mode = dsi->last_output_pwr_mode;

	if (dsi_pwr_mode == MTK_DSI_POWER_MODE_ON && last_panel_power_mode == MTK_DSI_POWER_MODE_OFF) {
		panel->lge.panel_state = LGE_PANEL_NOLP;
		rc = dsi_panel_update_lp_state(dsi, LGE_PANEL_NOLP);
		pr_info("going to on from off\n");
		goto mode_set;
	} else if (dsi_pwr_mode == MTK_DSI_POWER_MODE_OFF) {
		panel->lge.panel_state = LGE_PANEL_OFF;
		rc = dsi_panel_update_lp_state(dsi, LGE_PANEL_OFF);
		pr_info("going to off\n");
#ifdef CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY
		lge_panel_notifier_call_chain(LGE_PANEL_EVENT_BLANK,
				0, LGE_PANEL_STATE_BLANK); /* U0_BLANK; BLANKS */
#endif
		return 0;
	}

	rc = dsi_panel_send_lp_cmds(dsi, LGE_DDIC_DSI_SET_NOLP);
	if (rc < 0) {
		pr_err("fail to send lp command\n");
	}

	rc = dsi_panel_update_lp_state(dsi, LGE_PANEL_NOLP);
	if (rc < 0) {
		pr_err("fail to update lp state\n");
	}

mode_set:
#ifdef CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY
	lge_panel_notifier_call_chain(LGE_PANEL_EVENT_BLANK, 0, LGE_PANEL_STATE_UNBLANK); // U3, UNBLANK
#endif
	lge_dsi_panel_mode_set(dsi);

	return rc;
}

static ssize_t panel_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	pr_err("%s-%s\n", lge_man_name, lge_ddic_name);

	/* The number of characters should not exceed 30 characters. */
	return sprintf(buf, "%s-%s\n", lge_man_name, lge_ddic_name);
}
static DEVICE_ATTR(panel_type, S_IRUGO, panel_type_show, NULL);

static ssize_t mfts_auto_touch_test_mode_get(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;

	dsi = dev_get_drvdata(dev);

	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	panel = dsi->ext;

	return sprintf(buf, "%d\n", panel->lge.mfts_auto_touch);
}

static ssize_t mfts_auto_touch_test_mode_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)

{
	struct mtk_dsi *dsi = NULL;
	struct mtk_panel_ext *panel = NULL;
	int input;

	dsi = dev_get_drvdata(dev);
	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	panel = dsi->ext;

	sscanf(buf, "%d", &input);
	mutex_lock(&panel->panel_lock);
	panel->lge.mfts_auto_touch = input;
	mutex_unlock(&panel->panel_lock);

	pr_info("auto touch test : %d\n", input);

	return size;
}
static DEVICE_ATTR(mfts_auto_touch_test_mode, S_IRUGO | S_IWUSR | S_IWGRP,
		mfts_auto_touch_test_mode_get, mfts_auto_touch_test_mode_set);

void lge_panel_factory_create_sysfs(struct mtk_dsi *dsi, struct class *class_panel)
{
	static struct device *panel_reg_dev = NULL;
	struct mtk_panel_ext *panel = NULL;

	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		return;
	}

	panel = dsi->ext;

	if (!panel_reg_dev) {
		panel_reg_dev = device_create(class_panel, NULL, 0, dsi, "factory");
		if (IS_ERR(panel_reg_dev)) {
			pr_err("Failed to create dev(panel_reg_dev)!\n");
		} else {
			if ((device_create_file(panel_reg_dev, &dev_attr_panel_type)) < 0)
				pr_err("add panel_type node fail!\n");
			if ((device_create_file(panel_reg_dev, &dev_attr_mfts_auto_touch_test_mode)) < 0)
				pr_err("add mfts_auto_touch_test_mode node fail!\n");
			if (panel->lge.use_line_detect)
				lge_panel_line_detect_create_sysfs(panel_reg_dev);
		}
	}
}

static void lge_dsi_panel_create_sysfs(struct mtk_dsi *dsi)
{
	static struct class *class_panel = NULL;
	static struct device *panel_img_tune_sysfs_dev = NULL;
	//static struct device *panel_test_sysfs_dev = NULL;
	struct mtk_panel_ext *panel = NULL;

	panel = dsi->ext;

	if(!class_panel){
		class_panel = class_create(THIS_MODULE, "panel");
		if (IS_ERR(class_panel)) {
			pr_err("Failed to create panel class\n");
			return;
		}
	}

	if(!panel_img_tune_sysfs_dev){
		panel_img_tune_sysfs_dev = device_create(class_panel, NULL, 0, dsi, "img_tune");
		if (IS_ERR(panel_img_tune_sysfs_dev)) {
			pr_err("Failed to create dev(panel_img_tune_sysfs_dev)!\n");
		} else {
			if (panel->lge.use_color_manager)
				lge_color_manager_create_sysfs(panel, panel_img_tune_sysfs_dev);
		}
	}

	if (panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl ||
			panel->lge.use_dynamic_brightness || panel->lge.use_fp_lhbm ||
			panel->lge.use_tc_perf)
		lge_brightness_create_sysfs(dsi, class_panel);

	if (panel->lge.use_ambient)
		lge_ambient_create_sysfs(dsi, class_panel);

	//if (panel->lge.use_drs_mngr)
	//	lge_panel_drs_create_sysfs(panel, class_panel);

	lge_panel_reg_create_sysfs(dsi, class_panel);
	lge_panel_factory_create_sysfs(dsi, class_panel);
	lge_panel_err_detect_create_sysfs(dsi, class_panel);
/*
	if(!panel_test_sysfs_dev){
		panel_test_sysfs_dev = device_create(class_panel, NULL, 0, panel, "test");
		if (IS_ERR(panel_test_sysfs_dev)) {
			pr_err("Failed to create dev(panel_test_sysfs_dev)!\n");
		} else {
			if ((rc = device_create_file(panel_test_sysfs_dev, &dev_attr_report_panel_dead)) < 0)
				pr_err("add report_panel_dead set node fail!");
		}
	}
*/
#ifdef CONFIG_LGE_DISPLAY_TUNING_SUPPORT
	lge_tuning_create_sysfs(dsi, class_panel);
#endif
#if IS_ENABLED(CONFIG_LGE_DUAL_SCREEN)
	lge_cover_create_sysfs();
#endif /* CONFIG_LGE_DUAL_SCREEN */
#if defined(CONFIG_LGE_DISPLAY_RECOVERY)
	lge_esd_recovery_create_sysfs(dsi, class_panel);
#endif
}

int lge_dsi_panel_drv_init(struct mtk_dsi *dsi)
{
	struct mtk_panel_ext *panel = NULL;

	panel = dsi->ext;

	if(!panel->lge.support_display_scenario)
		return -1;

	mutex_init(&panel->panel_lock);
	mutex_init(&panel->brightness_lock);
	mutex_init(&panel->lge.pa_changed_lock);
	lge_dsi_panel_create_sysfs(dsi);

	return 0;
}
EXPORT_SYMBOL(lge_dsi_panel_drv_init);

static int lge_dsi_panel_parse_dt(struct mtk_panel_ext *panel, struct device_node *of_node)
{
	int rc = 0;
	const char *ddic_name;
	const char *man_name;
	u32 tmp = 0;

	memset(lge_man_name, 0x0, MAN_NAME_LEN+1);
	man_name = of_get_property(of_node, "lge,man-name", NULL);
	if (man_name) {
		strncpy(lge_man_name, man_name, MAN_NAME_LEN);
		pr_info("lge_man_name=%s\n", lge_man_name);
	} else {
		strncpy(lge_man_name, "undefined", MAN_NAME_LEN);
		pr_info("manufacturer name is not set\n");
	}

	memset(lge_ddic_name, 0x0, DDIC_NAME_LEN+1);
	ddic_name = of_get_property(of_node, "lge,ddic-name", NULL);
	if (ddic_name) {
		strncpy(lge_ddic_name, ddic_name, DDIC_NAME_LEN);
		pr_info("lge_ddic_name=%s\n", lge_ddic_name);
	} else {
		strncpy(lge_ddic_name, "undefined", DDIC_NAME_LEN);
		pr_info("ddic name is not set\n");
	}

	// TODO: temporal use
	panel->lge.dcs_brightness_be = of_property_read_bool(of_node, "lge,dcs-brightness-bigendian");
	pr_info("dcs_brightness_be=%d\n", panel->lge.dcs_brightness_be);

	panel->lge.is_incell = of_property_read_bool(of_node, "lge,incell-panel");
	pr_info("is_incell=%d\n", panel->lge.is_incell);

	panel->lge.use_bist = of_property_read_bool(of_node, "lge,ddic-bist-enabled");
	pr_info("use bist pattern=%d\n", panel->lge.use_bist);
	if (panel->lge.use_bist) {
		int i = 0;
		rc = of_property_read_string_array(of_node, "lge,ddic-bist-usage-type",
						panel->lge.bist_usage_type,
						MAX_BIST_USAGE_TYPE);
		for (i = 0; i < MAX_BIST_USAGE_TYPE; i++) {
			pr_debug("bist type=%s\n", panel->lge.bist_usage_type[i]);
		}
	}

	panel->lge.update_pps_in_lp = of_property_read_bool(of_node, "lge,update-pps-in-lp-mode");
	pr_info("update_pps in lp state=%d\n", panel->lge.update_pps_in_lp);

	panel->lge.use_drs_mngr = of_property_read_bool(of_node, "lge,drs-mngr-enabled");
	pr_info("use drs manager=%d\n", panel->lge.use_drs_mngr);

	panel->lge.use_internal_pps_switch =
		of_property_read_bool(of_node, "lge,drs-mngr-internal-pps-switch-enabled");

	panel->lge.bc_dim_en = of_property_read_bool(of_node, "lge,use-bc-dim");
	pr_info("use bc dim =%d\n", panel->lge.bc_dim_en);

	panel->lge.use_color_manager = of_property_read_bool(of_node, "lge,use-color-manager");
	pr_info("use color manager=%d\n", panel->lge.use_color_manager);

	panel->lge.use_color_manager_oled = of_property_read_bool(of_node, "lge,use-color-manager-oled");
	pr_info("use color manager oled=%d\n", panel->lge.use_color_manager_oled);

	rc = of_property_read_u32(of_node, "lge,hbm-mode", &tmp);
	if (rc) {
		panel->lge.hbm_mode = DEFAULT_HBM_MODE;
		pr_err("fail to parse lge.hbm_mode Set to Default %d\n", panel->lge.hbm_mode);
	} else {
		panel->lge.hbm_mode = tmp;
		pr_info("lge.hbm_mode %d\n", panel->lge.hbm_mode);
	}

	panel->lge.use_ambient = of_property_read_bool(of_node, "lge,use-ambient");
	pr_info("use ambient=%d\n", panel->lge.use_ambient);
	if (panel->lge.use_ambient) {
		rc = of_property_read_string_array(of_node, "lge,aod-interface-data",
						panel->lge.aod_interface_type, 3);
		lge_ambient_set_interface_data(panel);
	}

	panel->lge.use_cmd_wait_pa_changed = of_property_read_bool(of_node, "lge,cmd-wait-pa-changed");
	pr_info("use cmd_wait_pa_changed=%d\n", panel->lge.use_cmd_wait_pa_changed);

	panel->lge.use_line_detect = of_property_read_bool(of_node, "lge,use-line-detect");
	pr_info("use line detect=%d\n", panel->lge.use_line_detect);

	panel->lge.use_bc_dimming_work = of_property_read_bool(of_node, "lge,bc-dimming-work");
	pr_info("use bc dimming work=%d\n", panel->lge.use_bc_dimming_work);

	if (panel->lge.use_color_manager) {
		rc = of_property_read_u32(of_node, "lge,color-manager-default-status", &tmp);
		if (rc) {
			pr_err("fail to parse lge,color-manager-default-status\n");
			panel->lge.color_manager_default_status = false;
		} else {
			panel->lge.color_manager_default_status = (tmp > 0)? true : false;
			panel->lge.color_manager_status = 1;
			pr_info("color manager default status is %d\n", panel->lge.color_manager_default_status);
		}

		lge_dsi_parse_color_manager_modes(of_node, panel->lge.color_manager_table,
					&(panel->lge.color_manager_table_len), "lge,mdss-dsi-color-manager-mode-table");

		panel->lge.dgc_absent = of_property_read_bool(of_node, "lge,digital-gamma-absent");
		pr_info("digital gamma absent = %d\n", panel->lge.dgc_absent);
	} else {
		panel->lge.color_manager_default_status = false;
	}

	panel->lge.use_panel_err_detect = of_property_read_bool(of_node, "lge,use-panel-err-detect");
	pr_info("use panel err detect = %d\n", panel->lge.use_panel_err_detect);

	if (panel->lge.use_panel_err_detect) {
		lge_panel_err_detect_parse_dt(panel, of_node);
	}

	panel->lge.use_extra_recovery_cmd = of_property_read_bool(of_node, "lge,use-extra-recovery-cmd");
	pr_info("use extra recovery command = %d\n", panel->lge.use_extra_recovery_cmd);

	panel->lge.use_dcs_brightness_short = of_property_read_bool(of_node, "lge,dcs-brightness-short-write");
	pr_info("use dcs_brightness_short=%d\n", panel->lge.use_dcs_brightness_short);

	panel->lge.use_ddic_reg_lock = of_property_read_bool(of_node, "lge,use-ddic-register-lock");
	pr_info("use ddic_register_lock=%d\n", panel->lge.use_ddic_reg_lock);

	panel->lge.use_irc_ctrl = of_property_read_bool(of_node, "lge,use-irc-ctrl");
	pr_info("use irc_ctrl=%d\n", panel->lge.use_irc_ctrl);

	panel->lge.use_ace_ctrl = of_property_read_bool(of_node, "lge,use-ace-ctrl");
	pr_info("use ace_ctrl=%d\n", panel->lge.use_ace_ctrl);

	if (panel->lge.use_ace_ctrl) {
		rc = of_property_read_u32(of_node, "lge,default-ace-mode", &tmp);
		if (rc) {
			pr_err("fail to get ace default, set %d\n", panel->lge.ace_mode);
		} else {
			panel->lge.ace_mode = tmp;
			pr_info("ace default mode=%d\n", panel->lge.ace_mode);
		}
	}

	panel->lge.true_view_supported = of_property_read_bool(of_node, "lge,true-view-supported");
	pr_info("use true_view supported=%d\n", panel->lge.true_view_supported);

	panel->lge.use_vr_lp_mode = of_property_read_bool(of_node, "lge,use-vr-lp-mode");
	pr_info("use vr_lp_mode=%d\n", panel->lge.use_vr_lp_mode);

	panel->lge.use_dim_ctrl = of_property_read_bool(of_node, "lge,use-dim-ctrl");
	pr_info("use_dim_ctrl=%d\n", panel->lge.use_dim_ctrl);

	panel->lge.use_br_ctrl_ext = of_property_read_bool(of_node, "lge,disp-br-ctrl-ext-supported");
	pr_info("use_br_ctrl_ext=%d\n", panel->lge.use_br_ctrl_ext);

	panel->lge.use_fp_lhbm = of_property_read_bool(of_node, "lge,use-fp-lhbm");
	if(panel->lge.use_fp_lhbm) {
		panel->lge.fp_lhbm_br_lvl = FP_LHBM_DEFAULT_BR_LVL;
		panel->lge.need_fp_lhbm_set = false;
	}
	pr_info("use_fp_lhbm=%d\n", panel->lge.use_fp_lhbm);

	panel->lge.use_tc_perf = of_property_read_bool(of_node, "lge,use-tc-perf");
	pr_info("use_tc_perf=%d\n", panel->lge.use_tc_perf);

#ifdef CONFIG_LGE_DISPLAY_TUNING_SUPPORT
	panel->lge.lge_tune_dsi_params = of_property_read_bool(of_node, "lge,tune-dsi-params");
	pr_info("tune-dsi_params=%d\n", panel->lge.lge_tune_dsi_params);

	panel->lge.lge_tune_dsi_phy_params = of_property_read_bool(of_node, "lge,tune-dsi-phy-params");
	pr_info("tune-dsi_phy_params=%d\n", panel->lge.lge_tune_dsi_phy_params);

	panel->lge.lge_tune_brightness = of_property_read_bool(of_node, "lge,tune-brightness");
	pr_info("tune-brightness=%d\n", panel->lge.lge_tune_brightness);

	panel->lge.lge_tune_init_cmd = of_property_read_bool(of_node, "lge,tune-init-command");
	pr_info("tune-init-command=%d\n", panel->lge.lge_tune_init_cmd);
#endif
	return rc;
}

static int lge_dsi_panel_parse_support_display_scenario(struct mtk_panel_ext *panel, struct device_node *of_node)
{
	panel->lge.support_display_scenario = of_property_read_bool(of_node, "lge,support-display-scenario");
	pr_info("support_display_scenario=%d\n", panel->lge.support_display_scenario);

	return 0;
}

int lge_dsi_panel_get(struct mtk_dsi *dsi, struct device_node *of_node)
{
	struct mtk_panel_ext *panel = NULL;
	int rc = 0;

    if(!dsi || !dsi->ext) {
        pr_err("Panel is NULL\n");
        return -1;
    }

    panel = dsi->ext;

	rc = lge_dsi_panel_parse_support_display_scenario(panel, of_node);
	if (rc)
		pr_err("failed to parse blmap, rc=%d\n", rc);

	if(!panel->lge.support_display_scenario)
		return -1;

	rc = lge_dsi_panel_parse_blmap(panel, of_node);
	if (rc)
		pr_err("failed to parse blmap, rc=%d\n", rc);

	rc = lge_dsi_panel_parse_brightness(panel, of_node);
	if (rc)
		pr_err("failed to parse default brightness, rc=%d\n", rc);

	rc = lge_dsi_panel_parse_dt(panel, of_node);
	if (rc)
		pr_err("failed to parse dt, rc=%d\n", rc);

	rc = lge_ddic_dsi_panel_parse_cmd_sets(dsi, of_node);
	if (rc)
		pr_err("failed to parse ddic cmds sets, rc=%d\n", rc);

	lge_ddic_ops_init(panel);

	lge_ddic_feature_init(dsi);

	if (panel->lge.use_color_manager) {
		pr_info("default cm_preset_step 2\n");
		panel->lge.cm_preset_step = 2;

		if (panel->lge.use_bc_dimming_work)
			lge_bc_dim_work_init(panel);
	}

	return rc;
}
EXPORT_SYMBOL(lge_dsi_panel_get);

void lge_dsi_panel_put(struct mtk_panel_ext *panel)
{
	lge_dsi_panel_blmap_free(panel);
}

int dsi_panel_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt)
{
	const u32 cmd_set_min_size = 7;
	u32 count = 0;
	u32 packet_length;
	u32 tmp;

	while (length >= cmd_set_min_size) {
		packet_length = cmd_set_min_size;
		tmp = ((data[5] << 8) | (data[6]));
		packet_length += tmp;
		if (packet_length > length) {
			pr_err("format error\n");
			return -EINVAL;
		}
		length -= packet_length;
		data += packet_length;
		count++;
	};

	*cnt = count;
	return 0;
}

int dsi_panel_create_cmd_packets(const char *data,
					u32 length,
					u32 count,
					struct dsi_panel_cmd_set *cmd)
{
	int rc = 0;
	int i, j;
	u8 *payload;
	struct dsi_cmd_desc *cmds = NULL;

	cmds = cmd->cmds;

	for (i = 0; i < count; i++) {
		u32 size;

		cmds[i].msg.type = data[0];
		cmds[i].last_command = (data[1] == 1 ? true : false);
		cmds[i].msg.channel = data[2];
		cmds[i].msg.flags |= (data[3] == 1 ? MIPI_DSI_MSG_REQ_ACK : 0);
		cmds[i].post_wait_ms = data[4];
		cmds[i].msg.tx_len = ((data[5] << 8) | (data[6]));

		size = cmds[i].msg.tx_len * sizeof(u8);

		payload = kzalloc(size, GFP_KERNEL);
		if (!payload) {
			rc = -ENOMEM;
			goto error_free_payloads;
		}

		for (j = 0; j < cmds[i].msg.tx_len; j++)
			payload[j] = data[7 + j];

		cmds[i].msg.tx_buf = payload;
		data += (7 + cmds[i].msg.tx_len);
	}

	return rc;
error_free_payloads:
	for (i = i - 1; i >= 0; i--) {
		cmd--;
		kfree(cmds->msg.tx_buf);
	}

	return rc;
}

void dsi_panel_destroy_cmd_packets(struct dsi_panel_cmd_set *set)
{
	u32 i = 0;
	struct dsi_cmd_desc *cmd;

	for (i = 0; i < set->count; i++) {
		cmd = &set->cmds[i];
		kfree(cmd->msg.tx_buf);
	}
}

int dsi_panel_alloc_cmd_packets(struct dsi_panel_cmd_set *cmd,
					u32 packet_count)
{
	u32 size;

	size = packet_count * sizeof(*cmd->cmds);
	cmd->cmds = kzalloc(size, GFP_KERNEL);
	if (!cmd->cmds)
		return -ENOMEM;

	cmd->count = packet_count;
	return 0;
}

int lge_dsi_panel_parse_cmd_sets_sub(struct dsi_panel_cmd_set *cmd,
					const char *data,
					u32 length)
{
	int rc = 0;
	u32 packet_count = 0;

	rc = dsi_panel_get_cmd_pkt_count(data, length, &packet_count);
	if (rc) {
		pr_err("commands failed, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_panel_alloc_cmd_packets(cmd, packet_count);
	if (rc) {
		pr_err("failed to allocate cmd packets, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_panel_create_cmd_packets(data, length, packet_count, cmd);
	if (rc) {
		pr_err("failed to create cmd packets, rc=%d\n", rc);
		goto error_free_mem;
	}

	return rc;
error_free_mem:
	kfree(cmd->cmds);
	cmd->cmds = NULL;
error:
	return rc;
}

int lge_dsi_panel_tx_cmd_set(struct mtk_dsi *dsi,
				struct dsi_panel_cmd_set *cmd, unsigned int need_lock)
{
	int rc = 0, i = 0, j=0;
	struct dsi_cmd_desc *cmds;
	u32 count;
	enum dsi_cmd_set_state state;

	if (!dsi || !dsi->ext)
		return -EINVAL;

	cmds = cmd->cmds;
	count = cmd->count;
	state = cmd->state;

	if (count == 0) {
		pr_debug("[%s] No commands to be sent\n",
			 __func__);
		goto error;
	}

	for (i = 0; i < count; i++) {
		/* TODO:  handle last command */
		if (state == DSI_CMD_SET_STATE_LP)
			cmds[i].msg.flags |= MIPI_DSI_MSG_USE_LPM;

		if (cmds[i].last_command)
			cmds[i].msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;

		lge_ddic_dsi_send_cmd_msg(&cmds[i].msg, true, need_lock);

		if (cmds[i].post_wait_ms)
			usleep_range(cmds[i].post_wait_ms * 1000, ((cmds[i].post_wait_ms * 1000) + 10));
		for(j=0; j < cmd->cmds[i].msg.tx_len; j++)
		{
			pr_debug("0x%02x send\n", (*(u8 *)(cmd->cmds[i].msg.tx_buf+j)));
		}
		cmds++;
	}
error:
	return rc;
}

int lge_dsi_panel_cmd_read(struct mtk_dsi *dsi, u8 cmd, int cnt, char* ret_buf, unsigned int need_lock)
{
	u8 rx_buf[256] = {0x0};
	int i = 0, checksum = 0;
	struct mtk_panel_ext *panel = NULL;
	struct dsi_cmd_desc *cmds = NULL;

	cmds = kzalloc(sizeof(struct dsi_cmd_desc), GFP_KERNEL);
	if(!cmds) {
		pr_err("alloc memory for struct dsi_cmd_desc failed\n");
		return -EINVAL;
	}

	cmds->msg.channel = 0;
	cmds->msg.type = MIPI_DSI_DCS_READ;
	cmds->msg.tx_buf = &cmd;
	cmds->msg.tx_len = 1;
	cmds->msg.rx_buf = &rx_buf[0];
	cmds->msg.rx_len = cnt;
	cmds->msg.flags = MIPI_DSI_MSG_USE_LPM | MIPI_DSI_MSG_REQ_ACK | MIPI_DSI_MSG_LASTCOMMAND;
	cmds->last_command = false;
	cmds->post_wait_ms = 0;

	/* TO DO : panel connection check */
	/* if (not_connected) return -EINVAL */

	if (!dsi || !dsi->ext) {
		pr_err("panel is NULL\n");
		kfree(cmds);
		return -EINVAL;
	}

	panel = dsi->ext;
	panel->lge.read_cmds.cmds = cmds;

	lge_ddic_dsi_read_cmd_msg(&cmds->msg, need_lock);

	for (i = 0; i < cnt; i++)
		checksum += rx_buf[i];

	pr_info("[Reg:0x%02x] checksum=%d\n", cmd, checksum);

	memcpy(ret_buf, rx_buf, cnt);
	kfree(cmds);

	return checksum;
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_REFRESH_RATE_DIV)
extern void drm_crtc_wait_one_vblank(struct drm_crtc *crtc);
void lge_skip_vblank(void)
{
	struct drm_crtc *crtc = lge_get_crtc();
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_panel_ext *panel = NULL;
	int skip = 0, i = 0;

	if (crtc == NULL)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
        panel = mtk_crtc->panel_ext;

	if (panel == NULL)
		return;

	skip = panel->lge.refresh_rate_div - 1;
	for (i = 0; i < skip; ++i) {
		drm_crtc_wait_one_vblank(crtc);
	}
}
#endif
