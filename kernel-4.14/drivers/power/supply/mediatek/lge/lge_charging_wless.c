#define pr_fmt(fmt) "[LGECHG][WLESS] %s: " fmt, __func__

#include <linux/device.h>
#include <linux/of.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <mt-plat/mtk_battery.h>
#include <mtk_battery_internal.h>
#include "charger_class.h"
#include "mtk_charger_intf.h"
#include "mtk_charger_init.h"
#include "lge_charging.h"

#ifdef CONFIG_LGE_PM_CHARGER_CONTROLLER
#include <linux/power/charger_controller.h>
#endif

#define WLESS_TX_MIN_POWER (5000000)

static inline int to_power(struct wless_profile *p)
{
	if (!p)
		return -1;

	return (p->input_current / 1000) * (p->vbus / 1000);
}

static inline int min_power(int a, int b)
{
	if (a < 0 && b < 0) return -1;

	if (a < 0) return b;
	if (b < 0) return a;

	return min(a, b);
}

static inline int wless_get_temperature(struct charger_manager *info)
{
	return get_mtk_battery()->tbat_precise;
}

static inline int wless_get_capacity(struct charger_manager *info)
{
	return battery_get_soc();
}

static inline bool wless_use_bpp_profile(struct charger_manager *info)
{
	if (info->wless.type == WLESS_TYPE_BPP)
		return true;

	/* tx power not updated yet */
	if (info->wless.tx_power <= 0)
		return false;

	if (info->wless.tx_power < info->data.wless_fastchg_power)
		return true;

	return false;
}

static struct wless_profile *wless_get_profile(struct charger_manager *info,
						int power)
{
	struct wless_profile *profile = info->data.wless_epp_profile;
	int maxidx = info->data.wless_epp_num_profile;
	int idx;

	if (wless_use_bpp_profile(info)) {
		profile = info->data.wless_bpp_profile;
		maxidx = info->data.wless_bpp_num_profile;
	}

	if (!profile || maxidx <= 0)
		goto out;
	if (power < 0)
		goto out;

	for (idx = 0; idx < maxidx; idx++) {
		if (to_power(&profile[idx]) > power)
			continue;
		return &profile[idx];
	}

	return &profile[maxidx-1];

out:
	if (wless_use_bpp_profile(info))
		return &info->data.wless_bpp;
	return &info->data.wless_epp;
}

static int wless_set_vbus_ovp(struct charger_manager *info, int vbus)
{
	bool enable_vbus_ovp = true;

	if (info->wless.state != WLESS_STATE_STABLE)
		enable_vbus_ovp = false;
	if (vbus > info->data.max_charger_voltage_setting)
		enable_vbus_ovp = false;
	if (vbus == 0)
		enable_vbus_ovp = false;

	charger_enable_vbus_ovp(info, enable_vbus_ovp);

	return 0;
}

static int wless_set_vbus(struct charger_manager *info,
			  struct wless_profile *profile, bool changed)
{
	u32 vbus_now = info->wless.vbus;
	int vbus, mivr;
	int ret = 0;

	if (info->wless.state != WLESS_STATE_STABLE) {
		info->wless.vbus = 0;
		return -EPERM;
	}

	ret = charger_dev_get_constant_voltage(info->wless.dev, &vbus_now);
	if (ret)
		vbus_now = info->wless.vbus;

	if (!profile) {
		pr_warn("no wless profile selected\n");
		info->wless.vbus = 0;
		return -EINVAL;
	}

	vbus = profile->vbus;
	mivr = profile->mivr;
	if (!info->enable_hv_charging) {
		vbus = 5000000;
		mivr = info->data.min_charger_voltage;
	}

	if (vbus != vbus_now)
		changed = true;
	if (vbus != info->wless.vbus)
		changed = true;

	if (!changed)
		return 0;

	pr_info("vbus: %dmV, mivr: %dmV", vbus / 1000, mivr / 1000);

	charger_dev_set_mivr(info->chg1_dev, mivr);
	ret = charger_dev_set_constant_voltage(info->wless.dev, vbus);
	if (ret) {
		pr_info("failed to set wless vbus\n");
		info->wless.vbus = 0;
		return ret;
	}

	info->wless.vbus = vbus;

	return 0;
}

static int wless_power_by_c_region(struct charger_manager *info)
{
	int region = info->wless.c_region;
	struct wless_region_data *data;

	if (region < 0)
		return -1;

	data = &info->data.wless_c_region[region];

	if (wless_use_bpp_profile(info))
		return data->bpp_power;
	return data->epp_power;
}

static int wless_power_by_t_region(struct charger_manager *info)
{
	int region = info->wless.t_region;
	struct wless_region_data *data;

	if (region < 0)
		return -1;

	data = &info->data.wless_t_region[region];

	if (wless_use_bpp_profile(info))
		return data->bpp_power;
	return data->epp_power;
}

static int wless_power_by_tx(struct charger_manager *info)
{
	int power = info->wless.tx_power;

	if (power <= 0)
		return -1;

	/* reduce margin for stable connection */
	power -= info->data.wless_margin_power;
	if (power < WLESS_TX_MIN_POWER)
		power = WLESS_TX_MIN_POWER;

	return power;
}

static int wless_select_power(struct charger_manager *info)
{
	int power = to_power(wless_get_profile(info, -1));

	/* tx power */
	power = min_power(power, wless_power_by_tx(info));

	/* skip power restriction before stable state */
	if (!wless_is_stable(info))
		return power;

	/* temperature region power restriction */
	power = min_power(power, wless_power_by_t_region(info));

	/* capacity region power restriction */
	power = min_power(power, wless_power_by_c_region(info));

#ifdef CONFIG_LGE_PM_CHARGER_CONTROLLER
	/* charger-controller power restriction */
	power = min_power(power, chgctrl_get_wless_pwr());
#endif

	return power;
}

static int wless_check_c_region(struct charger_manager *info)
{
	int soc = wless_get_capacity(info);
	int i;

	for (i = info->data.wless_num_c_region - 1; i >= 0; i--) {
		if (info->data.wless_c_region[i].trigger < 0)
			continue;
		if (soc >= info->data.wless_c_region[i].trigger)
			break;
	}

	return i;
}

static int wless_check_t_region(struct charger_manager *info)
{
	int temp = wless_get_temperature(info);
	int i;

	for (i = info->data.wless_num_t_region - 1; i >= 0; i--) {
		if (temp >= info->data.wless_t_region[i].trigger)
			break;
	}

	for (; i < info->wless.t_region; i++) {
		if (temp < info->data.wless_t_region[i+1].clear)
			break;
	}

	pr_info("bat_temp : %d\n", temp);

	return i;
}

static bool wless_get_ept_status(struct charger_manager *info)
{
	u32 ept_status = 0;
	int ret = 0;

	ret = charger_dev_get_status(info->wless.dev, WLESS_CHG_STAT_EPT,
			&ept_status);
	if (ret)
		return false;

	return ept_status ? true : false;
}

static void wless_update_ept_status(struct charger_manager *info)
{
	bool ept_status = wless_get_ept_status(info);

	if (ept_status != info->wless.ept_status) {
		pr_info("prev ept_status = %d, current ept_status = %d\n",
				info->wless.ept_status, ept_status);
		info->wless.ept_status = ept_status;

		if (info->wless.ept_status)
			wless_plug_out(info);
		else
			wless_plug_in(info);
	}
}

static void wless_check_ept_status(struct charger_manager *info)
{
	int temperature = wless_get_temperature(info);

	if (temperature >= info->data.wless_overheat_temp) {
		charger_dev_set_status(info->wless.dev, WLESS_CHG_STAT_EPT,
				WLESS_EPT_OVER_TEMPERATURE);
	}
}

static enum wless_state wless_check_state(struct charger_manager *info)
{
	u32 cali = 0;
	int ret;

	if (!upmu_get_rgs_chrdet() || info->wless.type == WLESS_TYPE_UNKNOWN)
		return WLESS_STATE_OFF;

	ret = charger_dev_get_status(info->wless.dev,
			WLESS_CHG_STAT_CALI, &cali);
	if (!ret && cali)
		return WLESS_STATE_CALI;

	return WLESS_STATE_STABLE;
}

static enum wless_type wless_check_type(struct charger_manager *info)
{
	u32 epp = 0;
	int ret;

	ret = charger_dev_get_status(info->wless.dev, WLESS_CHG_STAT_EPP, &epp);
	if (ret)
		return WLESS_TYPE_UNKNOWN;

	return epp ? WLESS_TYPE_EPP : WLESS_TYPE_BPP;
}

static int wless_check_tx_power(struct charger_manager *info)
{
	int power = -1;
	int ret;

	ret = charger_dev_get_power(info->wless.dev, &power);
	if (ret)
		return -1;

	/* tx power not renewed yet. use previous */
	if (power <= 0)
		return info->wless.tx_power;

	return power;
}

bool wless_is_fastchg(struct charger_manager *info)
{
	if (info->chr_type != WIRELESS_CHARGER)
		return false;

	if (info->wless.type != WLESS_TYPE_EPP)
		return false;

	if (info->wless.tx_power < info->data.wless_fastchg_power)
		return false;

	return true;
}

int wless_get_tx_power(struct charger_manager *info)
{
	int power = info->wless.tx_power;

	if (power <= 0)
		return -1;

	return power;
}

bool wless_is_stable(struct charger_manager *info)
{
	if (info->chr_type != WIRELESS_CHARGER)
		return true;

	return (info->wless.state == WLESS_STATE_STABLE) ? true : false;
}

void wless_select_current(struct charger_manager *info, int *ichg, int *aicr)
{
	struct wless_profile *profile = info->wless.profile;

	if (!profile) {
		*aicr = info->data.wless_base_input_current;
		*ichg = info->data.wless_base_charging_current;
		return;
	}

	*aicr = profile->input_current;
	*ichg = profile->charging_current;
}

int wless_algorithm(struct charger_manager *info)
{
	const char *state_str[] = {
		[WLESS_STATE_OFF] = "off",
		[WLESS_STATE_CALI] = "cali",
		[WLESS_STATE_STABLE] = "stable",
	};
	struct wless_profile *profile = info->wless.profile;
	enum wless_state state = info->wless.state;
	bool changed = false;

	if (info->chr_type != WIRELESS_CHARGER)
		return 0;

	info->wless.type = wless_check_type(info);
	info->wless.state = wless_check_state(info);
	if (info->wless.state != state)
		changed = true;

	if (upmu_get_rgs_chrdet())
		wless_check_ept_status(info);

	wless_update_ept_status(info);
	if (info->wless.ept_status)
		return 0;

	info->wless.tx_power = wless_check_tx_power(info);
	info->wless.t_region = wless_check_t_region(info);
	info->wless.c_region = wless_check_c_region(info);

	info->wless.power = wless_select_power(info);
	pr_info("power: %dmW (state: %s, t_region: %d, c_region: %d)\n",
			info->wless.power / 1000,
			state_str[info->wless.state],
			info->wless.t_region, info->wless.c_region);

	info->wless.profile = wless_get_profile(info, info->wless.power);
	if (info->wless.profile != profile)
		changed = true;

	wless_set_vbus(info, info->wless.profile, changed);
	wless_set_vbus_ovp(info, info->wless.vbus);

	return changed ? 1 : 0;
}

void wless_plug_in(struct charger_manager *info)
{
	if (info->chr_type != WIRELESS_CHARGER)
		return;

	charger_enable_vbus_ovp(info, false);
	info->wless.type = WLESS_TYPE_UNKNOWN;
	info->wless.tx_power = -1;
	info->wless.state = WLESS_STATE_OFF;
	info->wless.t_region = -1;
	info->wless.c_region = -1;
	info->wless.vbus = 0;
	info->wless.power = -1;
	info->wless.profile = NULL;
}

void wless_plug_out(struct charger_manager *info)
{
	info->wless.type = WLESS_TYPE_UNKNOWN;
	info->wless.tx_power = -1;
	info->wless.state = WLESS_STATE_OFF;
	info->wless.t_region = -1;
	info->wless.c_region = -1;
	info->wless.vbus = 0;
	info->wless.power = -1;
	info->wless.profile = NULL;
	info->wless.ept_status = false;
}

int wless_init(struct charger_manager *info)
{
	info->wless.type = WLESS_TYPE_UNKNOWN;
	info->wless.tx_power = -1;
	info->wless.state = WLESS_STATE_OFF;
	info->wless.t_region = -1;
	info->wless.c_region = -1;
	info->wless.vbus = 0;
	info->wless.power = -1;
	info->wless.profile = NULL;
	info->wless.ept_status = false;

	return 0;
}

int wless_notifier_call(struct charger_manager *info, unsigned long event,
			void *data)
{
	switch (event) {
	case CHARGER_DEV_NOTIFY_MODE_CHANGE:
		_wake_up_charger(info);
		pr_info("%s: mode change\n", __func__);
		return NOTIFY_DONE;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int wless_parse_dt_profile(struct device_node *np, const char *name,
				   struct wless_profile *p)
{
	char prop_name[32]; /* 32 is max size of property name */
	int ret;

	snprintf(prop_name, 32, "%s_%s", name, "input_current");
	ret = of_property_read_u32(np, prop_name, &p->input_current);
	if (ret < 0)
		return ret;

	snprintf(prop_name, 32, "%s_%s", name, "charging_current");
	ret = of_property_read_u32(np, prop_name, &p->charging_current);
	if (ret < 0)
		return ret;

	snprintf(prop_name, 32, "%s_%s", name, "vbus");
	ret = of_property_read_u32(np, prop_name, &p->vbus);
	if (ret < 0)
		return ret;

	snprintf(prop_name, 32, "%s_%s", name, "mivr");
	ret = of_property_read_u32(np, prop_name, &p->mivr);
	if (ret < 0)
		return ret;

	return 0;
}

static void wless_parse_dt_bpp_profile(struct charger_manager *info,
				       struct device_node *np)
{
	const char *prop_name = "wless_bpp_profile";
	const char *name = NULL;
	struct wless_profile *profile = NULL;
	int idx, maxidx;
	int ret;

	maxidx = of_property_count_strings(np, prop_name);
	if (maxidx <= 0)
		goto out;

	profile = devm_kzalloc(&info->pdev->dev, sizeof(*profile) * maxidx,
			GFP_KERNEL);
	if (!profile)
		goto out;

	for (idx = 0; idx < maxidx; idx++) {
		ret = of_property_read_string_index(np, prop_name, idx, &name);
		if (ret)
			goto out;

		ret = wless_parse_dt_profile(np, name, &profile[idx]);
		if (ret)
			goto out;
	}

	info->data.wless_bpp_profile = profile;
	info->data.wless_bpp_num_profile = maxidx;
	pr_info("%d bpp profile found\n", maxidx);

	return;

out:
	if (profile)
		devm_kfree(&info->pdev->dev, profile);
	info->data.wless_bpp_profile = NULL;
	info->data.wless_bpp_num_profile = 0;
}

static void wless_parse_dt_epp_profile(struct charger_manager *info,
				       struct device_node *np)
{
	const char *prop_name = "wless_epp_profile";
	const char *name = NULL;
	struct wless_profile *profile = NULL;
	int idx, maxidx;
	int ret;

	maxidx = of_property_count_strings(np, prop_name);
	if (maxidx <= 0)
		goto out;

	profile = devm_kzalloc(&info->pdev->dev, sizeof(*profile) * maxidx,
			GFP_KERNEL);
	if (!profile)
		goto out;

	for (idx = 0; idx < maxidx; idx++) {
		ret = of_property_read_string_index(np, prop_name, idx, &name);
		if (ret)
			goto out;

		ret = wless_parse_dt_profile(np, name, &profile[idx]);
		if (ret)
			goto out;
	}

	info->data.wless_epp_profile = profile;
	info->data.wless_epp_num_profile = maxidx;
	pr_info("%d epp profile found\n", maxidx);

	return;

out:
	if (profile)
		devm_kfree(&info->pdev->dev, profile);
	info->data.wless_epp_profile = NULL;
	info->data.wless_epp_num_profile = 0;
}

static void wless_parse_dt_t_region_data(struct device_node *np, int idx,
				         struct wless_region_data *data)
{
	char prop_name[32]; /* 32 is max size of property name */
	int ret;

	snprintf(prop_name, 32, "wless_region_t%d_%s", idx, "trig_temp");
	ret = of_property_read_u32(np, prop_name, &data->trigger);
	if (ret < 0)
		data->trigger = 550;

	snprintf(prop_name, 32, "wless_region_t%d_%s", idx, "clr_temp");
	ret = of_property_read_u32(np, prop_name, &data->clear);
	if (ret < 0)
		data->clear = 540;

	snprintf(prop_name, 32, "wless_region_t%d_%s", idx, "bpp_power");
	ret = of_property_read_u32(np, prop_name, &data->bpp_power);
	if (ret < 0)
		data->bpp_power = -1;

	snprintf(prop_name, 32, "wless_region_t%d_%s", idx, "epp_power");
	ret = of_property_read_u32(np, prop_name, &data->epp_power);
	if (ret < 0)
		data->epp_power = -1;
}

static int wless_parse_dt_get_num_t_region(struct charger_manager *info,
					   struct device_node *np)
{
	char prop_name[32]; /* 32 is max size of property name */
	int i;

	for (i = 0; i < 10; i++) {
		snprintf(prop_name, 32, "wless_region_t%d_trig_temp", i);
		if (!of_find_property(np, prop_name, NULL))
			break;
		snprintf(prop_name, 32, "wless_region_t%d_clr_temp", i);
		if (!of_find_property(np, prop_name, NULL))
			break;
	}

	return i;
}

static void wless_parse_dt_t_region(struct charger_manager *info,
				    struct device_node *np)
{
	int maxidx, idx;

	maxidx = wless_parse_dt_get_num_t_region(info, np);
	if (maxidx <= 0) {
		info->data.wless_num_t_region = 0;
		pr_info("no t_region found\n");
		return;
	}

	info->data.wless_t_region = devm_kzalloc(&info->pdev->dev,
			sizeof(*info->data.wless_t_region) * maxidx,
			GFP_KERNEL);
	if (!info->data.wless_t_region) {
		info->data.wless_num_t_region = 0;
		pr_err("failed to alloc for t_region\n");
		return;
	}

	info->data.wless_num_t_region = maxidx;

	pr_info("%d t_region found\n", maxidx);
	for (idx = 0; idx < maxidx; idx++) {
		wless_parse_dt_t_region_data(np, idx,
				&info->data.wless_t_region[idx]);
	}
}

static void wless_parse_dt_c_region_data(struct device_node *np, int idx,
				         struct wless_region_data *data)
{
	char prop_name[32]; /* 32 is max size of property name */
	int ret;

	snprintf(prop_name, 32, "wless_region_c%d_%s", idx, "soc");
	ret = of_property_read_u32(np, prop_name, &data->trigger);
	if (ret < 0)
		data->trigger = -1;

	snprintf(prop_name, 32, "wless_region_c%d_%s", idx, "bpp_power");
	ret = of_property_read_u32(np, prop_name, &data->bpp_power);
	if (ret < 0)
		data->bpp_power = -1;

	snprintf(prop_name, 32, "wless_region_c%d_%s", idx, "epp_power");
	ret = of_property_read_u32(np, prop_name, &data->epp_power);
	if (ret < 0)
		data->epp_power = -1;
}

static int wless_parse_dt_get_num_c_region(struct charger_manager *info,
					   struct device_node *np)
{
	char prop_name[32]; /* 32 is max size of property name */
	int i;

	for (i = 0; i < 10; i++) {
		snprintf(prop_name, 32, "wless_region_c%d_soc", i);
		if (!of_find_property(np, prop_name, NULL))
			break;
	}

	return i;
}

static void wless_parse_dt_c_region(struct charger_manager *info,
				    struct device_node *np)
{
	int maxidx, idx;

	maxidx = wless_parse_dt_get_num_c_region(info, np);
	if (maxidx <= 0) {
		info->data.wless_num_c_region = 0;
		pr_info("no c_region found\n");
		return;
	}

	info->data.wless_c_region = devm_kzalloc(&info->pdev->dev,
			sizeof(*info->data.wless_c_region) * maxidx,
			GFP_KERNEL);
	if (!info->data.wless_c_region) {
		info->data.wless_num_c_region = 0;
		pr_err("failed to alloc for c_region\n");
		return;
	}

	info->data.wless_num_c_region = maxidx;

	pr_info("%d c_region found\n", maxidx);
	for (idx = 0; idx < maxidx; idx++) {
		wless_parse_dt_c_region_data(np, idx,
				&info->data.wless_c_region[idx]);
	}
}

static const struct wless_profile bpp_default = {
	.input_current = WLESS_BPP_INPUT_CURRENT,
	.charging_current = WLESS_BPP_CHARGING_CURRENT,
	.vbus = WLESS_BPP_VBUS,
	.mivr = WLESS_BPP_MIVR,
};

static const struct wless_profile epp_default = {
	.input_current = WLESS_EPP_INPUT_CURRENT,
	.charging_current = WLESS_EPP_CHARGING_CURRENT,
	.vbus = WLESS_EPP_VBUS,
	.mivr = WLESS_EPP_MIVR,
};

void wless_parse_dt(struct charger_manager *info, struct device_node *np)
{
	u32 val;
	int ret;

	ret = wless_parse_dt_profile(np, "wless_bpp", &info->data.wless_bpp);
	if (ret < 0) {
		pr_err("failed to read wless_bpp. use default\n");
		memcpy(&info->data.wless_bpp, &bpp_default,
				sizeof(info->data.wless_bpp));
	}

	ret = wless_parse_dt_profile(np, "wless_epp", &info->data.wless_epp);
	if (ret < 0) {
		pr_err("failed to read wless_epp. use default\n");
		memcpy(&info->data.wless_epp, &epp_default,
				sizeof(info->data.wless_epp));
	}

	if (of_property_read_u32(np, "wless_margin_power", &val) >= 0) {
		info->data.wless_margin_power = val;
	} else {
		pr_info("use default WLESS_MARGIN_POWER:%d\n",
			WLESS_MARGIN_POWER);
		info->data.wless_margin_power = WLESS_MARGIN_POWER;
	}

	if (of_property_read_u32(np, "wless_fastchg_power", &val) >= 0) {
		info->data.wless_fastchg_power = val;
	} else {
		pr_info("use default WLESS_FASTCHG_POWER:%d\n",
			WLESS_FASTCHG_POWER);
		info->data.wless_fastchg_power = WLESS_FASTCHG_POWER;
	}

	if (of_property_read_u32(np, "wless_overheat_temp", &val) >= 0) {
		info->data.wless_overheat_temp = val;
	} else {
		pr_info("use default WLESS_OVERHEAT_TEMP:%d\n",
			WLESS_OVERHEAT_TEMP);
		info->data.wless_overheat_temp = WLESS_OVERHEAT_TEMP;
	}

	if (of_property_read_u32(np, "wless_base_input_current", &val) >= 0) {
		info->data.wless_base_input_current = val;
	} else {
		pr_info("use default WLESS_BASE_INPUT_CURRENT:%d\n",
			WLESS_BASE_INPUT_CURRENT);
		info->data.wless_base_input_current = WLESS_BASE_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "wless_base_charging_current", &val) >= 0) {
		info->data.wless_base_charging_current = val;
	} else {
		pr_info("use default WLESS_BASE_CHARGING_CURRENT:%d\n",
			WLESS_BASE_CHARGING_CURRENT);
		info->data.wless_base_charging_current =
			WLESS_BASE_CHARGING_CURRENT;
	}

	wless_parse_dt_bpp_profile(info, np);
	wless_parse_dt_epp_profile(info, np);
	wless_parse_dt_t_region(info, np);
	wless_parse_dt_c_region(info, np);
}
