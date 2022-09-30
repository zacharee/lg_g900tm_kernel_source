#define pr_fmt(fmt) "[LGECHGTYPE] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include "lge_chg_type.h"

int lge_chg_type_get_voltage_max(struct lge_chg_type *lct)
{
	if (!lct)
		return 0;

	if (lct->chg_type == CHARGER_UNKNOWN)
		return 0;

	return lct->voltage_max;
}

int lge_chg_type_get_current_max(struct lge_chg_type *lct)
{
	if (!lct)
		return 0;

	if (lct->chg_type == CHARGER_UNKNOWN)
		return 0;

	return lct->current_max;
}

int lge_chg_type_is_fastchg(struct lge_chg_type *lct)
{
	if (!lct)
		return 0;

	if (lct->chg_type == CHARGER_UNKNOWN)
		return 0;
	if (!lct->fastchg_support)
		return 0;

	return lct->fastchg;
}

int lge_chg_type_is_fastchg_support(struct lge_chg_type *lct)
{
	if (!lct)
		return 0;

	return lct->fastchg_support;
}

int lge_chg_type_get_vzw_chg(struct lge_chg_type *lct)
{
	if (!lct)
		return VZW_NO_CHARGER;

	return lct->vzw_chg;
}

static bool lge_chg_type_update_vzw_chg(struct lge_chg_type *lct)
{
	const static char *vzw_chg_str[] = {
		[VZW_NO_CHARGER] = "NO_CHARGER",
		[VZW_NORMAL_CHARGING] = "NORMAL_CHARGING",
		[VZW_INCOMPATIBLE_CHARGING] = "INCOMPATIBLE_CHARGING",
		[VZW_UNDER_CURRENT_CHARGING] = "UNDER_CURRENT_CHARGING",
		[VZW_USB_DRIVER_UNINSTALLED] = "USB_DRIVER_UNINSTALLED",
	};
	int vzw_chg = VZW_NORMAL_CHARGING;

	if (lct->chg_type == STANDARD_CHARGER) {
		if (lct->current_max && lct->current_max < 450000)
			vzw_chg = VZW_UNDER_CURRENT_CHARGING;
	}

	if (lct->floated)
		vzw_chg = VZW_INCOMPATIBLE_CHARGING;

	if (lct->fastchg)
		vzw_chg = VZW_NORMAL_CHARGING;

	if (lct->chg_type == CHARGER_UNKNOWN)
		vzw_chg = VZW_NO_CHARGER;

	if (vzw_chg == lct->vzw_chg)
		return false;

	pr_info("vzw_chg: %s\n", vzw_chg_str[vzw_chg]);
	lct->vzw_chg = vzw_chg;

	return true;
}

int lge_chg_type_is_floated(struct lge_chg_type *lct)
{
	if (!lct)
		return 0;

	if (lct->chg_type == CHARGER_UNKNOWN)
		return 0;

	return lct->floated;
}

static bool lge_chg_type_floated_ignore(struct lge_chg_type *lct)
{
	int rp;

	if (lct->chg_type != NONSTANDARD_CHARGER)
		return true;
	if (lct->fastchg)
		return true;

	if (lct->tcpc) {
		rp = tcpm_inquire_typec_remote_rp_curr(lct->tcpc);
		if (rp == 3000 || rp == 1500)
			return true;
	}

	return false;
}

static void lge_chg_type_floated_work(struct work_struct *work)
{
	struct lge_chg_type *lct = container_of(to_delayed_work(work),
			struct lge_chg_type, floated_dwork);

	if (lge_chg_type_floated_ignore(lct))
		return;

	lct->floated_triggered = 1;
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
	if (lct->bc12)
		charger_manager_retry_chg_type_det(lct->bc12);
#else
	mtk_pmic_enable_chr_type_det(true);
#endif
}

static bool lge_chg_type_floated_check(struct lge_chg_type *lct)
{
	if (lct->floated_retry_ms <= 0)
		return false;

	if (lge_chg_type_floated_ignore(lct)) {
		cancel_delayed_work(&lct->floated_dwork);
		lct->floated_triggered = 0;
		lct->floated = 0;
		return false;
	}

	if (!lct->floated_triggered) {
		/* retry charger type detection */
		pr_info("floated charger detected. retry after %dms\n",
				lct->floated_retry_ms);
		schedule_delayed_work(&lct->floated_dwork,
				msecs_to_jiffies(lct->floated_retry_ms));
		return false;
	}

	if (lct->floated) {
		pr_info("floated charger already detected\n");
		return false;
	}

	pr_info("floated charger detected.\n");
	lct->floated = 1;
	return true;
}

static void lge_chg_type_clr(struct lge_chg_type *lct)
{
	lct->chg_type = CHARGER_UNKNOWN;
	lct->voltage_max = 5000000;
	lct->current_max = 0;
	lct->fastchg = 0;
	lct->vzw_chg = VZW_NO_CHARGER;
	lct->floated_triggered = 0;
	lct->floated = 0;
}

int lge_chg_type_set_property(struct lge_chg_type *lct,
			      enum power_supply_property psp,
			      const union power_supply_propval *val)
{
	bool changed = false;

	if (!lct)
		return -EINVAL;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		lct->chg_type = val->intval;
		if (lct->chg_type == CHARGER_UNKNOWN)
			lge_chg_type_clr(lct);

		if (lge_chg_type_floated_check(lct))
			changed = true;
		if (lge_chg_type_update_vzw_chg(lct))
			changed = true;
		break;
	case POWER_SUPPLY_PROP_FASTCHG:
		if (lct->chg_type == CHARGER_UNKNOWN)
			break;
		if (lct->fastchg == val->intval)
			break;

		pr_info("fast charger %s.\n", val->intval ?
				"detected" : "undetected");
		lct->fastchg = val->intval;
		changed = true;
		lge_chg_type_update_vzw_chg(lct);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (lct->chg_type == CHARGER_UNKNOWN)
			break;
		if (lct->voltage_max == val->intval)
			break;

		lct->voltage_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (lct->chg_type == CHARGER_UNKNOWN)
			break;
		if (lct->current_max == val->intval)
			break;

		lct->current_max = val->intval;
		changed = lge_chg_type_update_vzw_chg(lct);

		break;
	default:
		return -EINVAL;
	}

	if (changed && lct->psy)
		power_supply_changed(lct->psy);

	return 0;
}

static void lge_chg_type_parse_dt(struct lge_chg_type *lct,
				  struct device_node *np)
{
	int ret = 0;

	ret = of_property_read_u32(np, "floated-retry-ms",
			&lct->floated_retry_ms);
	if (ret)
		lct->floated_retry_ms = 0;

	ret = of_property_read_u32(np, "fastchg-support",
			&lct->fastchg_support);
	if (ret)
		lct->fastchg_support = 0;
}

int lge_chg_type_init(struct lge_chg_type *lct)
{
	lge_chg_type_parse_dt(lct, lct->dev->of_node);
	lge_chg_type_clr(lct);
	INIT_DELAYED_WORK(&lct->floated_dwork, lge_chg_type_floated_work);

	return 0;
}
