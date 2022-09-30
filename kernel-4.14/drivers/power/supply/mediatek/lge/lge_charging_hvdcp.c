#define pr_fmt(fmt) "[LGECHG][HVDCP] %s: " fmt, __func__

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/mutex.h>

#include <mt-plat/mtk_battery.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/mtk_boot_common.h>
#include "mtk_charger_intf.h"
#include "lge_charging.h"

bool hvdcp_is_connect(struct charger_manager *info)
{
	if (info->chr_type != STANDARD_CHARGER)
		return false;

	if (mtk_pe20_get_is_connect(info))
		return true;

	if (mtk_pe_get_is_connect(info))
		return true;

	return false;
}

void hvdcp_set_enable(struct charger_manager *info)
{
	if (info->enable_pe_2) {
		if (!mtk_pe20_get_is_enable(info)) {
			mtk_pe20_set_is_enable(info, true);
			mtk_pe20_set_to_check_chr_type(info, true);
		}
	}

	if (info->enable_pe_plus) {
		if (!mtk_pe_get_is_enable(info)) {
			mtk_pe_set_is_enable(info, true);
			mtk_pe_set_to_check_chr_type(info, true);
		}
	}
}

void hvdcp_set_disable(struct charger_manager *info)
{
	if (info->enable_pe_2) {
		if (mtk_pe20_get_is_enable(info)) {
			mtk_pe20_set_is_enable(info, false);
			if (mtk_pe20_get_is_connect(info))
				mtk_pe20_reset_ta_vchr(info);
		}
	}

	if (info->enable_pe_plus) {
		if (mtk_pe_get_is_enable(info)) {
			mtk_pe_set_is_enable(info, false);
			if (mtk_pe_get_is_connect(info))
				mtk_pe_reset_ta_vchr(info);
		}
	}
}

void hvdcp_set_check_chr_type(struct charger_manager *info)
{
	if (info->enable_pe_2)
		mtk_pe20_set_to_check_chr_type(info, true);
	if (info->enable_pe_plus)
		mtk_pe_set_to_check_chr_type(info, true);
}

void hvdcp_select_current(struct charger_manager *info, int *ichg, int *aicr)
{
	if (info->chr_type != STANDARD_CHARGER)
		return;

	if (mtk_pe20_get_is_connect(info))
		mtk_pe20_set_charging_current(info, ichg, aicr);

	if (mtk_pe_get_is_connect(info))
		mtk_pe_set_charging_current(info, ichg, aicr);
}

static int hvdcp_check_charger(struct charger_manager *info)
{
	struct lge_charging_alg_data *data = info->algorithm_data;

	/* do not enable hvdcp if once charging done unless actually discharging */
	if (data->recharging && battery_get_uisoc() > 99)
		return 0;

	mutex_lock(&data->lock);

	if (info->enable_pe_2)
		mtk_pe20_check_charger(info);
	if (info->enable_pe_plus)
		mtk_pe_check_charger(info);

	mutex_unlock(&data->lock);

	return 0;
}

static int hvdcp_start_algorithm(struct charger_manager *info)
{
	struct lge_charging_alg_data *data = info->algorithm_data;
	int vbus = 5000000;
	int ret;

	if (info->chr_type != STANDARD_CHARGER)
		return 0;

	mutex_lock(&data->lock);

	if (mtk_pe20_get_is_connect(info)) {
		ret = mtk_pe20_start_algorithm(info);
		if (!ret)
			vbus = info->pe2.vbus;

		data->input_voltage = vbus;
	}

	if (mtk_pe_get_is_connect(info)) {
		ret = mtk_pe_start_algorithm(info);
		if (!ret) {
			vbus = 9000000;
			if (info->data.ta_12v_support)
				vbus = 12000000;
		}

		data->input_voltage = vbus;
	}

	mutex_unlock(&data->lock);

	return 0;
}

int hvdcp_algorithm(struct charger_manager *info)
{
	struct lge_charging_alg_data *data = info->algorithm_data;
	bool hvdcp;

	if (info->chr_type != STANDARD_CHARGER)
		return 0;

	if (data->state != CHR_CC)
		return 0;

	if (info->pe5.online)
		return 0;
	if (mtk_pdc_check_charger(info))
		return 0;
	if (mtk_is_TA_support_pd_pps(info))
		return 0;

	if (is_typec_adapter(info)) {
		if (adapter_dev_get_property(info->pd_adapter,
				TYPEC_RP_LEVEL) == 3000)
			return 0;
	}

	hvdcp = hvdcp_is_connect(info);

	hvdcp_check_charger(info);

	/* return here to notify faster */
	if (hvdcp_is_connect(info) != hvdcp)
		return 1;

	hvdcp_start_algorithm(info);

	if (hvdcp_is_connect(info) != hvdcp)
		return 1;

	return 0;
}

void hvdcp_plug_in(struct charger_manager *info)
{
}

void hvdcp_plug_out(struct charger_manager *info)
{
	mtk_pe20_set_is_cable_out_occur(info, true);
	mtk_pe_set_is_cable_out_occur(info, true);
}

int hvdcp_init(struct charger_manager *info)
{
	return 0;
}
