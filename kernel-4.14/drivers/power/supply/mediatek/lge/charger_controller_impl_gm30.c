#define pr_fmt(fmt) "[CC][GM30]%s: " fmt, __func__

#include <linux/power_supply.h>
#include <linux/power/charger_controller.h>
#include <mt-plat/mtk_charger.h>
#include "mtk_charger_intf.h"

#ifdef CONFIG_LGE_PM_USB_ID
#include <linux/power/lge_usb_id.h>
#endif
#ifdef CONFIG_MTK_BOOT
#include <mt-plat/mtk_boot_common.h>
#endif

#include "charger_controller.h"
#include "lge_charging.h"

struct chgctrl_gm30 {
	struct chgctrl *chip;
	struct charger_consumer *consumer;
	struct charger_manager *manager;
};

static struct charger_consumer *chgctrl_gm30_get_consumer(struct chgctrl_gm30 *gm30)
{
	struct chgctrl *chip = gm30->chip;

	if (!chip)
		return NULL;

	if (gm30->consumer)
		return gm30->consumer;

	gm30->consumer = charger_manager_get_by_name(chip->dev,
			"charger_controller");

	return gm30->consumer;
}

static struct charger_manager *chgctrl_gm30_get_manager(struct chgctrl_gm30 *gm30)
{
	if (gm30->manager)
		return gm30->manager;

	if (gm30->consumer) {
		gm30->manager = gm30->consumer->cm;
		return gm30->manager;
	}

	/* udpate consumer */
	if (chgctrl_gm30_get_consumer(gm30)) {
		gm30->manager = gm30->consumer->cm;
		return gm30->manager;
	}

	return NULL;
}

const char *chgctrl_get_charger_name(struct chgctrl *chip)
{
	struct chgctrl_gm30 *gm30 = chip->impl;
	struct charger_manager *info;
#ifdef CONFIG_LGE_PM_WIRELESS_CHARGER
	int power;
#endif

	info = chgctrl_gm30_get_manager(gm30);
	if (info == NULL)
		return "Unknown";

	if (info->chr_type == CHARGER_UNKNOWN)
		return "Unknown";

	if (mtk_pe50_get_is_connect(info))
		return "DIRECT";

	if (mtk_is_TA_support_pd_pps(info))
		return "DIRECT";

	if (is_typec_adapter(info))
		return "USB_C";

	if (mtk_pdc_check_charger(info))
		return "USB_PD";

	switch (info->chr_type) {
	case STANDARD_HOST:
		return "USB";
	case CHARGING_HOST:
		return "USB_CDP";
	case NONSTANDARD_CHARGER:
		return "NON_STD";
	case STANDARD_CHARGER:
		if (mtk_pe20_get_is_connect(info))
			return "PE20";
		if (mtk_pe_get_is_connect(info))
			return "PE";
		return "USB_DCP";
	case APPLE_2_1A_CHARGER:
		return "APPLE_2_1A";
	case APPLE_1_0A_CHARGER:
		return "APPLE_1_0A";
	case APPLE_0_5A_CHARGER:
		return "APPLE_0_5A";
#ifdef CONFIG_LGE_PM_WIRELESS_CHARGER
	case WIRELESS_CHARGER:
		power = wless_get_tx_power(info);

		if (power >= 15000000)
			return "WLC_15W";
		if (power >= 10000000)
			return "WLC_10W";
		if (power >= 9000000)
			return "WLC_9W";
		if (power >= 5000000)
			return "WLC_5W";

		return "WLC";
#endif
	default:
		break;
	}

	return "Unknown";
}

int chgctrl_get_gauge_current_now(struct chgctrl *chip)
{
	return chgctrl_battery_current_now() / 10;
}

int chgctrl_get_charger_voltage_now(struct chgctrl *chip)
{

	static struct power_supply *psy = NULL;
	union power_supply_propval val;
	int ret;

	if (!psy)
		psy = power_supply_get_by_name("charger");
	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW,
			&val);
	if (!ret)
		return val.intval;

out:
	return 0;
}

int chgctrl_get_boot_mode(void)
{
#ifdef CONFIG_MTK_BOOT
	int boot_mode = get_boot_mode();

	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT)
		return CC_BOOT_MODE_CHARGER;
	if (boot_mode == LOW_POWER_OFF_CHARGING_BOOT)
		return CC_BOOT_MODE_CHARGER;
#endif

#ifdef CONFIG_LGE_PM_USB_ID
	if (lge_is_factory_cable_boot())
		return CC_BOOT_MODE_FACTORY;
#endif

	return CC_BOOT_MODE_NORMAL;
}

void chgctrl_fastchg_changed(void *args)
{
	struct chgctrl *chip = args;
	struct chgctrl_gm30 *gm30 = chip->impl;
	struct charger_consumer *consumer;
	int fastchg;
	bool hv_charging_disabled = false;
	int ret;

	fastchg = chgctrl_vote_active_value(&chip->fastchg);
	hv_charging_disabled = fastchg ? false : true;

	consumer = chgctrl_gm30_get_consumer(gm30);
	if (!consumer)
		return;

	if (hv_charging_disabled == consumer->hv_charging_disabled)
		return;

	ret = charger_manager_enable_high_voltage_charging(consumer,
			hv_charging_disabled ? false : true);
	if (ret) {
		consumer->hv_charging_disabled = hv_charging_disabled;
		chgctrl_changed(chip);
	}
}

int chgctrl_impl_init(struct chgctrl *chip)
{
	struct chgctrl_gm30 *gm30;

	gm30 = devm_kzalloc(chip->dev, sizeof(struct chgctrl_gm30),
			GFP_KERNEL);
	if (!gm30)
		return -ENOMEM;

	gm30->chip = chip;

	chip->impl = gm30;

	return 0;
}

/* mtk charger driver support */
int chgctrl_get_icl_boost(void)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int icl = -1;

	/* charger-contoller not ready. return */
	if (!chip)
		return -1;

	icl = chgctrl_vote_active_value(&chip->icl_boost);
	if (icl > 0)
		icl *= 1000;

	return icl;
}
EXPORT_SYMBOL(chgctrl_get_icl_boost);

int chgctrl_get_icl(void)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int icl = -1;

	/* charger-contoller not ready. return */
	if (!chip)
		return -1;

	icl = chgctrl_vote_active_value(&chip->icl);
	if (icl > 0)
		icl *= 1000;

	return icl;
}
EXPORT_SYMBOL(chgctrl_get_icl);

int chgctrl_get_fcc(void)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int fcc = -1;

	/* charger-contoller not ready. return */
	if (!chip)
		return -1;

	fcc = chgctrl_vote_active_value(&chip->fcc);
	if (fcc > 0)
		fcc *= 1000;

	return fcc;
}
EXPORT_SYMBOL(chgctrl_get_fcc);

int chgctrl_get_vfloat(void)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int vol = -1;

	/* charger-contoller not ready. return */
	if (!chip)
		return -1;

	vol = chgctrl_vote_active_value(&chip->vfloat);
	if (vol > 0)
		vol *= 1000;

	return vol;
}
EXPORT_SYMBOL(chgctrl_get_vfloat);

bool chgctrl_get_input_suspend(void)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	bool input_suspend = false;

	/* charger-contoller not ready. return */
	if (!chip)
		return false;

	if (chgctrl_vote_active_value(&chip->input_suspend))
		input_suspend = true;

	return input_suspend;
}
EXPORT_SYMBOL(chgctrl_get_input_suspend);

int chgctrl_get_wless_pwr(void)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int power = -1;

	/* charger-contoller not ready. return */
	if (!chip)
		return -1;

	power = chgctrl_vote_active_value(&chip->wless_pwr);
	if (power > 0)
		power *= 1000;

	return power;
}
EXPORT_SYMBOL(chgctrl_get_wless_pwr);
