#define pr_fmt(fmt)	"[Display][ddic-ops:%s:%d] " fmt, __func__, __LINE__

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <video/mipi_display.h>
#include <linux/lge_panel_notify.h>
#include <drm/drm_panel.h>
#include <drm/drm_mode.h>
#include <drm/drm_mipi_dsi.h>
#include "lge_dsi_panel.h"
#include "../mtk_panel_ext.h"
#include "../mtk_dsi.h"
#include "cm/lge_color_manager.h"
#include "err_detect/lge_err_detect.h"

extern char* get_ddic_name(void);
extern bool is_ddic_name(char *ddic_name);
extern struct lge_ddic_ops sw43103_ops;

extern int dsi_panel_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt);
extern int dsi_panel_create_cmd_packets(const char *data,
					u32 length, u32 count, struct lge_ddic_dsi_panel_cmd_set *cmd);

struct lge_ddic_match {
	char compatible[15];
	struct lge_ddic_ops *ops;
};

static struct lge_ddic_match supported_ddic_list[] = {
	{"sw43103", &sw43103_ops},
};

void lge_ddic_ops_init(struct mtk_panel_ext *panel)
{
	int i;
	int count = sizeof(supported_ddic_list)/sizeof(supported_ddic_list[0]);

	for (i = 0; i < count; ++i) {
		if (is_ddic_name(supported_ddic_list[i].compatible)) {
			panel->lge.ddic_ops = supported_ddic_list[i].ops;
			break;
		}
	}

	if (panel->lge.ddic_ops == NULL)
		pr_warn("no matched ddic ops for %s\n", get_ddic_name());
}

void lge_ddic_feature_init(struct mtk_dsi *dsi)
{
	struct mtk_panel_ext *panel = NULL;

	panel = dsi->ext;

	if (panel->lge.use_bist) {
		panel->lge.bist_on = 0;
		mutex_init(&panel->lge.bist_lock);
	}

	if (panel->lge.use_panel_err_detect) {
		lge_panel_err_detect_init(dsi);
	}

	if ((panel->lge.use_irc_ctrl || panel->lge.use_ace_ctrl) &&
			panel->lge.ddic_ops && panel->lge.ddic_ops->set_irc_default_state) {
		panel->lge.irc_current_state = 0;
		panel->lge.ddic_ops->set_irc_default_state(dsi);
	}
}

bool is_bist_supported(struct mtk_panel_ext *panel, const char type[])
{
	int i = 0;

	if (!panel || !panel->lge.use_bist)
		return false;

	for (i = 0; i < MAX_BIST_USAGE_TYPE; i++) {
		if (!strncmp(panel->lge.bist_usage_type[i], type, 3)) {
			return true;
		}
	}

	return false;
}

int store_aod_area(struct mtk_panel_ext *panel, struct lge_rect *rect)
{
	if (panel->lge.aod_area.y != rect->y)
		panel->lge.partial_area_vertical_changed = true;
	if (panel->lge.aod_area.h != rect->h)
		panel->lge.partial_area_height_changed = true;
	panel->lge.aod_area.x = rect->x;
	panel->lge.aod_area.y = rect->y;
	panel->lge.aod_area.w = rect->w;
	panel->lge.aod_area.h = rect->h;
	return 0;
}

struct dsi_cmd_desc* find_nth_cmd(struct dsi_cmd_desc *cmds, int cmds_count, int addr, int nth)
{
	struct dsi_cmd_desc *ret = NULL;
	int i;
	char *payload;

	for (i = 0; i < cmds_count; ++i) {
		payload = (char*)cmds[i].msg.tx_buf;
		if (payload[0] == addr) {
			if (--nth == 0) {
				ret = &cmds[i];
				break;
			}
		}
	}

	return ret;
}

struct dsi_cmd_desc* find_cmd(struct dsi_cmd_desc *cmds, int cmds_count, int addr)
{
	struct dsi_cmd_desc *ret = NULL;
	int i;
	char *payload;

	for (i = 0; i < cmds_count; ++i) {
		payload = (char*)cmds[i].msg.tx_buf;
		if (payload[0] == addr) {
			ret = &cmds[i];
			break;
		}
	}

	return ret;
}

char *lge_ddic_cmd_set_prop_map[LGE_DDIC_DSI_CMD_SET_MAX] = {
	"lge,mdss-dsi-ie-command",
	"lge,mdss-dsi-bist-on-command",
	"lge,mdss-dsi-bist-off-command",
	"lge,mdss-dsi-wb-default-command",
	"lge,mdss-dsi-cm-dci-p3-command",
	"lge,mdss-dsi-cm-srgb-command",
	"lge,mdss-dsi-cm-adobe-command",
	"lge,mdss-dsi-cm-native-command",
	"lge,mdss-dsi-disp-ctrl-command-1",
	"lge,mdss-dsi-disp-ctrl-command-2",
	"lge,digital-gamma-cmds-dummy",
	"lge,mdss-dsi-bc-dim-command",
	"lge,mdss-dsi-bc-default-command",
	"lge,mdss-dsi-vr-lp-mode-on-command",
	"lge,mdss-dsi-vr-lp-mode-off-command",
	"lge,mdss-dsi-lp1-command",
	"lge,mdss-dsi-lp2-command",
	"lge,mdss-dsi-nolp-command",
	"lge,mdss-dsi-saturation-command",
	"lge,mdss-dsi-hue-command",
	"lge,mdss-dsi-sharpness-command",
	"lge,mdss-dsi-saturation-command",
	"lge,mdss-dsi-hue-command",
	"lge,mdss-dsi-sharpness-command",
	"lge,mdss-dsi-cm-natural",
	"lge,mdss-dsi-cm-vivid",
	"lge,mdss-dsi-cm-cinema",
	"lge,mdss-dsi-cm-sports",
	"lge,mdss-dsi-cm-game",
	"lge,mdss-dsi-cm-photo",
	"lge,mdss-dsi-cm-web",
	"lge,detect-vert-line-restore-command",
	"lge,detect-black-vert-line-command",
	"lge,detect-white-vert-line-command",
	"lge,memory-error-detect-command",
	"lge,esd-detect-command",
	"lge,line-defect-detect-command",
	"lge,selective-color-cmds-dummy-command",
	"lge,ddic-register-lock",
	"lge,ddic-register-unlock",
	"lge,mdss-dsi-ve-on-command",
	"lge,mdss-dsi-ve-off-command",
	"lge,mdss-dsi-hdr-set",
	"lge,mdss-dsi-irc-command",
	"lge,mdss-dsi-ace-tune-command",
	"lge,mdss-dsi-ace-restore-command",
	"lge,digital-gamma-set",
	"lge,mdss-dsi-aod-area-command",
	"lge,color-mode-cmds-dummy",
	"lge,color-mode-set",
	"lge,custom-rgb-hue-lut",
	"lge,saturation-lut",
	"lge,sharpness-lut",
	"lge,trueview-lut",
	"lge,ddic-dsi-br-ctrl-ext-command",
	"lge,mdss-dsi-tc-perf-on-command",
	"lge,mdss-dsi-tc-perf-off-command",
	"lge,rgb-lut",
	"lge,ace-lut",
	"lge,mdss-dsi-fp-lhbm-ready-command",
	"lge,mdss-dsi-fp-lhbm-exit-command",
	"lge,mdss-dsi-fp-lhbm-on-command",
	"lge,mdss-dsi-fp-lhbm-off-command",
	"lge,mdss-dsi-fp-lhbm-aod-to-fps",
	"lge,mdss-dsi-fp-lhbm-fps-to-aod",
	"lge,mdss-dsi-bl-set",
	"lge,mdss-dsi-daylight-on-command",
	"lge,mdss-dsi-daylight-off-command",
	"lge,mdss-dsi-ecc-on-command",
	"lge,mdss-dsi-ecc-off-command",
};

char *lge_ddic_cmd_set_state_map[LGE_DDIC_DSI_CMD_SET_MAX] = {
	"lge,mdss-dsi-ie-command-state",
	"lge,mdss-dsi-bist-control-command-state",
	"lge,mdss-dsi-bist-control-command-state",
	"lge,mdss-dsi-wb-default-command-state",
	"lge,mdss-dsi-cm-dci-p3-command-state",
	"lge,mdss-dsi-cm-srgb-command-state",
	"lge,mdss-dsi-cm-adobe-command-state",
	"lge,mdss-dsi-cm-native-command-state",
	"lge,mdss-dsi-disp-ctrl-command-1-state",
	"lge,mdss-dsi-disp-ctrl-command-2-state",
	"lge,digital-gamma-cmds-dummy-state",
	"lge,mdss-dsi-bc-dim-command-state",
	"lge,mdss-dsi-bc-default-command-state",
	"lge,mdss-dsi-vr-lp-command-state",
	"lge,mdss-dsi-vr-lp-command-state",
	"lge,mdss-dsi-lp1-command-state",
	"lge,mdss-dsi-lp2-command-state",
	"lge,mdss-dsi-nolp-command-state",
	"lge,mdss-dsi-saturation-command-state",
	"lge,mdss-dsi-hue-command-state",
	"lge,mdss-dsi-sharpness-command-state",
	"lge,mdss-dsi-saturation-command-state",
	"lge,mdss-dsi-hue-command-state",
	"lge,mdss-dsi-sharpness-command-state",
	"lge,mdss-dsi-cm-natural-state",
	"lge,mdss-dsi-cm-vivid-state",
	"lge,mdss-dsi-cm-cinema-state",
	"lge,mdss-dsi-cm-sports-state",
	"lge,mdss-dsi-cm-game-state",
	"lge,mdss-dsi-cm-photo-state",
	"lge,mdss-dsi-cm-web-state",
	"lge,detect-vert-line-restore-command-state",
	"lge,detect-black-vert-line-command-state",
	"lge,detect-white-vert-line-command-state",
	"lge,memory-error-detect-command-state",
	"lge,esd-detect-command-state",
	"lge,line-defect-detect-command-state",
	"lge,selective-color-cmds-dummy-command-state",
	"lge,ddic-register-lock-unlock-state",
	"lge,ddic-register-lock-unlock-state",
	"lge,mdss-dsi-ve-on-command-state",
	"lge,mdss-dsi-ve-off-command-state",
	"lge,mdss-dsi-hdr-set-state",
	"lge,mdss-dsi-irc-command-state",
	"lge,mdss-dsi-ace-command-state",
	"lge,mdss-dsi-ace-command-state",
	"lge,digital-gamma-set-state",
	"lge,mdss-dsi-aod-area-command-state",
	"lge,color-mode-cmds-dummy-state",
	"lge,color-mode-set-state",
	"lge,custom-rgb-hue-lut-state",
	"lge,saturation-lut-state",
	"lge,sharpness-lut-state",
	"lge,trueview-lut-state",
	"lge,ddic-dsi-br-ctrl-ext-command-state",
	"lge,mdss-dsi-tc-perf-on-command-state",
	"lge,mdss-dsi-tc-perf-off-command-state",
	"lge,rgb-lut-state",
	"lge,ace-lut-state",
	"lge,mdss-dsi-fp-lhbm-ready-command-state",
	"lge,mdss-dsi-fp-lhbm-exit-command-state",
	"lge,mdss-dsi-fp-lhbm-on-command-state",
	"lge,mdss-dsi-fp-lhbm-off-command-state",
	"lge,mdss-dsi-fp-lhbm-aod-to-fps-state",
	"lge,mdss-dsi-fp-lhbm-fps-to-aod-state",
	"lge,mdss-dsi-bl-set-state",
	"lge,mdss-dsi-daylight-on-command",
	"lge,mdss-dsi-daylight-off-command",
	"lge,mdss-dsi-ecc-on-command-state",
	"lge,mdss-dsi-ecc-off-command-state",
};

/* lge_ddic_dsi_panel_tx_cmd_set for LGE DSI CMD SETS*/
int lge_ddic_dsi_panel_tx_cmd_set(struct mtk_dsi *dsi,
				enum lge_ddic_dsi_cmd_set_type type, unsigned int need_lock)
{
	int rc = 0;
	u32 count;
    struct mtk_panel_ext *panel = NULL;
	struct drm_panel *drm_panel = NULL;

	if (!dsi || !dsi->ext || !dsi->panel) {
		pr_err("panel is NULL\n");
		return -EINVAL;
	}

	panel = dsi->ext;
	drm_panel = dsi->panel;
	count = panel->lge.lge_cmd_sets[type].count;

	if (count == 0) {
		pr_debug("[%s] No commands to be sent for state(%d)\n",
			 __func__, type);
		goto error;
	}

	printk("[%s] type = %s, need_lock = %d\n", __func__, lge_ddic_cmd_set_prop_map[type], need_lock);
	if(need_lock == 0) {
		if(panel->funcs->panel_tx_cmd_set)
			panel->funcs->panel_tx_cmd_set(drm_panel, dsi, type);
	} else
		lge_ddic_dsi_send_cmd(type, true, need_lock);

error:
	return rc;
}

int lge_ddic_dsi_panel_alloc_cmd_packets(struct lge_ddic_dsi_panel_cmd_set *cmd,
					u32 packet_count)
{
	u32 size;

	size = packet_count * sizeof(*cmd->cmds);
	cmd->cmds = kzalloc(size, GFP_KERNEL);
	cmd->para_table = kzalloc(packet_count * sizeof(struct mtk_panel_para_table), GFP_KERNEL);
	if (!cmd->cmds || !cmd->para_table)
		return -ENOMEM;

	cmd->count = packet_count;
	return 0;
}

int lge_ddic_dsi_panel_create_cmd_packets(const char *data,
					u32 length,
					u32 count,
					struct lge_ddic_dsi_panel_cmd_set *cmd)
{
	int rc = 0;
	int i, j;
	u8 *payload;
	struct dsi_cmd_desc *cmds = NULL;
	struct mtk_panel_para_table *para_table;

	cmds = cmd->cmds;
	para_table = cmd->para_table;

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

		for (j = 0; j < cmds[i].msg.tx_len; j++) {
			payload[j] = data[7 + j];
			para_table[i].para_list[j] =  data[7 + j];
		}

		cmds[i].msg.tx_buf = payload;
		para_table[i].count = cmds[i].msg.tx_len;
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

int lge_ddic_dsi_panel_parse_cmd_sets_sub(struct lge_ddic_dsi_panel_cmd_set *cmd,
					enum lge_ddic_dsi_cmd_set_type type,
					struct device_node *of_node)
{
	int rc = 0;
	u32 length = 0;
	const char *data;
	const char *state;
	u32 packet_count = 0;

	data = of_get_property(of_node, lge_ddic_cmd_set_prop_map[type], &length);
	if (!data) {
		pr_debug("%s commands not defined\n", lge_ddic_cmd_set_prop_map[type]);
		rc = -ENOTSUPP;
		goto error;
	}

	rc = dsi_panel_get_cmd_pkt_count(data, length, &packet_count);
	if (rc) {
		pr_err("commands failed, rc=%d\n", rc);
		goto error;
	}
	pr_debug("[%s] packet-count=%d, %d\n", lge_ddic_cmd_set_prop_map[type],
		packet_count, length);

	rc = lge_ddic_dsi_panel_alloc_cmd_packets(cmd, packet_count);
	if (rc) {
		pr_err("failed to allocate cmd packets, rc=%d\n", rc);
		goto error;
	}

	rc = lge_ddic_dsi_panel_create_cmd_packets(data, length, packet_count, cmd);
	if (rc) {
		pr_err("failed to create cmd packets, rc=%d\n", rc);
		goto error_free_mem;
	}

	state = of_get_property(of_node, lge_ddic_cmd_set_state_map[type], NULL);
	if (!state || !strcmp(state, "dsi_lp_mode")) {
		cmd->state = DSI_CMD_SET_STATE_LP;
	} else if (!strcmp(state, "dsi_hs_mode")) {
		cmd->state = DSI_CMD_SET_STATE_HS;
	} else {
		pr_err("[%s] command state unrecognized-%s\n",
		       lge_ddic_cmd_set_state_map[type], state);
		goto error_free_mem;
	}

	return rc;
error_free_mem:
	kfree(cmd->cmds);
	cmd->cmds = NULL;
error:
	return rc;
}

int lge_ddic_dsi_panel_parse_cmd_sets(struct mtk_dsi *dsi,
	struct device_node *of_node)
{
	int rc = 0;
	struct lge_ddic_dsi_panel_cmd_set *set;
	struct mtk_panel_ext *panel = NULL;
	u32 i;

	panel = dsi->ext;

	for(i = 0; i < LGE_DDIC_DSI_CMD_SET_MAX; i++) {
		set = &panel->lge.lge_cmd_sets[i];
		set->type = i;
		set->count = 0;
		rc = lge_ddic_dsi_panel_parse_cmd_sets_sub(set, i, of_node);
		if(rc)
			pr_err("parse set %d is failed or not defined\n", i);
	}
	rc = 0;
	return rc;
}

char* get_payload_addr(struct mtk_panel_ext *panel, enum lge_ddic_dsi_cmd_set_type type, int position)
{
	struct lge_ddic_dsi_panel_cmd_set *cmd_set = NULL;
	struct dsi_cmd_desc *cmd = NULL;
	char *payload = NULL;

	if (type >= LGE_DDIC_DSI_CMD_SET_MAX) {
		pr_err("out of range\n");
		goto exit;
	}

	cmd_set = &(panel->lge.lge_cmd_sets[type]);
	if (cmd_set->count == 0) {
		pr_err("cmd set is not defined\n");
		goto exit;
	}

	cmd = &(panel->lge.lge_cmd_sets[type].cmds[position]);
	if (!cmd) {
		pr_err("empty cmd\n");
		goto exit;
	}

	payload = (char *)cmd->msg.tx_buf;
	if (!payload) {
		pr_err("empty payload\n");
		goto exit;
	}

	pr_debug("find payload\n");

exit:
	return payload;
}

int get_payload_cnt(struct mtk_panel_ext *panel, enum lge_ddic_dsi_cmd_set_type type, int position)
{
	struct lge_ddic_dsi_panel_cmd_set *cmd_set = NULL;
	struct dsi_cmd_desc *cmd = NULL;
	int payload_count = 0;

	if (type >= LGE_DDIC_DSI_CMD_SET_MAX) {
		pr_err("out of range\n");
		goto exit;
	}

	cmd_set = &(panel->lge.lge_cmd_sets[type]);
	if (cmd_set->count == 0) {
		pr_err("cmd set is not defined\n");
		goto exit;
	}

	cmd = &(panel->lge.lge_cmd_sets[type].cmds[position]);
	if (!cmd) {
		pr_err("empty cmd\n");
		goto exit;
	}

	payload_count = (int)cmd->msg.tx_len;

	pr_debug("find payload\n");

exit:
	return payload_count;
}
