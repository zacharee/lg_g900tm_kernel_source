#define pr_fmt(fmt) "[CC]%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/async.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#ifdef CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY
#include <linux/lge_panel_notify.h>
#endif
#include "charger_controller.h"

/* feature */
static struct chgctrl_feature *chgctrl_features[] = {
	&chgctrl_feature_otp,
	&chgctrl_feature_fb,
	&chgctrl_feature_spec,
	&chgctrl_feature_actm,
	&chgctrl_feature_thermal,
	&chgctrl_feature_restricted,
	&chgctrl_feature_game,
	&chgctrl_feature_chargerlogo,
#ifdef TIME_TO_FULL_MODE
	&chgctrl_feature_ttf,
#endif
	&chgctrl_feature_bcc,
	&chgctrl_feature_battery_id,
//	&chgctrl_feature_llk,
	&chgctrl_feature_factory,
	&chgctrl_feature_info,
	NULL,
};

static void chgctrl_feature_init_default(struct chgctrl *chip)
{
	struct chgctrl_feature **feature;

	for (feature = chgctrl_features; *feature; feature++) {
		if (!(*feature)->init_default)
			continue;

		(*feature)->init_default(chip);
	}
}

static int chgctrl_feature_parse_dt(struct chgctrl *chip,
				    struct device_node *np)
{
	struct chgctrl_feature **feature;

	for (feature = chgctrl_features; *feature; feature++) {
		if (!(*feature)->parse_dt)
			continue;

		(*feature)->parse_dt(chip, np);
	}

	return 0;
}

static int chgctrl_feature_init(struct chgctrl *chip)
{
	struct chgctrl_feature **feature;

	for (feature = chgctrl_features; *feature; feature++) {
		if (!(*feature)->init)
			continue;

		(*feature)->init(chip);
	}

	return 0;
}

static void chgctrl_feature_exit(struct chgctrl *chip)
{
	struct chgctrl_feature **feature;

	for (feature = chgctrl_features; *feature; feature++) {
		if (!(*feature)->exit)
			continue;

		(*feature)->exit(chip);
	}
}

void chgctrl_feature_trigger(struct chgctrl *chip, int trig)
{
	struct chgctrl_feature **feature;

	for (feature = chgctrl_features; *feature; feature++) {
		if (!(*feature)->trigger)
			continue;

		(*feature)->trigger(chip, trig);
	}
}

struct chgctrl* chgctrl_get_drvdata(void)
{
	static struct power_supply *psy = NULL;

	if (!psy)
		psy = power_supply_get_by_name("charger_controller");

	if (psy)
		return power_supply_get_drvdata(psy);

	return NULL;
}

/* override battery properties */
static int chgctrl_override_status(struct chgctrl *chip, int status)
{
	int capacity;

	if (status != POWER_SUPPLY_STATUS_CHARGING &&
			status != POWER_SUPPLY_STATUS_FULL)
		return status;

	if (chip->ccd_status != POWER_SUPPLY_STATUS_UNKNOWN)
		return chip->ccd_status;

	capacity = chgctrl_get_battery_capacity(chip);
	if (capacity >= 100)
		status = POWER_SUPPLY_STATUS_FULL;
	else
		status = POWER_SUPPLY_STATUS_CHARGING;

	return status;
}

static int chgctrl_override_health(struct chgctrl *chip, int health)
{
	if (health != POWER_SUPPLY_HEALTH_GOOD)
		return health;

	if (chip->ccd_health != POWER_SUPPLY_HEALTH_UNKNOWN)
		return chip->ccd_health;

	if (chip->otp.health == POWER_SUPPLY_HEALTH_OVERHEAT)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	if (chip->otp.health == POWER_SUPPLY_HEALTH_COLD)
		return POWER_SUPPLY_HEALTH_COLD;

	return health;
}

static int chgctrl_override_batt_therm(struct chgctrl *chip, int origin_batt_therm)
{
	unsigned int temp_comp = 0;
	int batt_therm = 0;
	int ibat_now = 0;

	if (!chip->charger_online)
		return origin_batt_therm;

	if (chip->display_on)
		return origin_batt_therm;

	ibat_now = chgctrl_get_gauge_current_now(chip);

	temp_comp = ibat_now * ibat_now * chip->batt_therm_comp / 1000000;
	batt_therm = origin_batt_therm - temp_comp;

//	pr_info("report_therm: %d batt_therm ori: %d, temp_comp: %d (%d), ibat: %d\n",
//			batt_therm, origin_batt_therm, temp_comp, chip->batt_therm_comp, ibat_now);

	return batt_therm;
}

void chgctrl_battery_property_override(enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	if (!chip)
		return;

	switch(psp) {
	case POWER_SUPPLY_PROP_STATUS:
		chip->battery_status = val->intval;
		val->intval = chgctrl_override_status(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		chip->battery_present = val->intval;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		chip->battery_health = val->intval;
		val->intval = chgctrl_override_health(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		if (chip->technology == POWER_SUPPLY_TECHNOLOGY_UNKNOWN)
			break;
		val->intval = chip->technology;
		break;
	/* save original battery data */
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->battery_capacity = val->intval;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		chip->battery_voltage = val->intval;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		chip->battery_temperature = val->intval;
		val->intval = chgctrl_override_batt_therm(chip, val->intval);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(chgctrl_battery_property_override);

void chgctrl_charger_property_override(enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	if (!chip)
		return;

	switch(psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (chgctrl_vote_active_value(&chip->input_suspend))
			val->intval = 0;
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(chgctrl_charger_property_override);

/* notifier block */
static void chgctrl_fb_work(struct work_struct *work)
{
	struct chgctrl *chip = container_of(work, struct chgctrl, fb_work);

	chgctrl_feature_trigger(chip, FB_TRIGGER);
}

#ifdef CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY
static int chgctrl_fb_notifier_call(struct notifier_block *nb,
				    unsigned long event, void *v)
{
	struct chgctrl *chip = container_of(nb, struct chgctrl, fb_nb);
	struct lge_panel_notifier *panel = (struct lge_panel_notifier *)v;
	bool display_on = false;

	if (event != LGE_PANEL_EVENT_BLANK)
		return NOTIFY_DONE;

	if (!panel || panel->display_id != 0)
		return NOTIFY_DONE;

	if (panel->state == LGE_PANEL_STATE_UNBLANK)
		display_on = true;

	if (chip->display_on == display_on)
		return NOTIFY_DONE;

	chip->display_on = display_on;
	pr_info("fb %s\n", (display_on ? "on" : "off"));

	schedule_work(&chip->fb_work);

	return NOTIFY_DONE;
}
#else
static int chgctrl_fb_notifier_call(struct notifier_block *nb,
				    unsigned long event, void *v)
{
	struct chgctrl *chip = container_of(nb, struct chgctrl, fb_nb);
	struct fb_event *ev = (struct fb_event *)v;
	bool display_on = false;

	if (event != FB_EVENT_BLANK)
		return NOTIFY_DONE;

	if (!ev || !ev->data)
		return NOTIFY_DONE;

	if (*(int*)ev->data == FB_BLANK_UNBLANK)
		display_on = true;

	if (chip->display_on == display_on)
		return NOTIFY_DONE;

	chip->display_on = display_on;
	pr_info("fb %s\n", (display_on ? "on" : "off"));

	schedule_work(&chip->fb_work);

	return NOTIFY_DONE;
}
#endif

static void chgctrl_charger_work(struct work_struct *work)
{
	struct chgctrl *chip = container_of(work, struct chgctrl,
			charger_work);
	struct power_supply *charger;
	union power_supply_propval val;
	int online = 0;
	int ret;

	charger = chgctrl_get_charger_psy(chip);
	if (charger) {
		ret = power_supply_get_property(charger,
				POWER_SUPPLY_PROP_ONLINE, &val);
		if (!ret)
			online = val.intval;
	}
	if (online == chip->charger_online)
		return;

	chip->charger_online = online;
	pr_info("charger %s\n", online ? "online" : "offline");

	chgctrl_feature_trigger(chip, CHARGER_TRIGGER);
}

static void chgctrl_battery_work(struct work_struct *work)
{
	struct chgctrl *chip = container_of(work, struct chgctrl,
			battery_work);

	/* update battery info */
	chgctrl_get_battery_status(chip);

	chgctrl_feature_trigger(chip, BATTERY_TRIGGER);
}

static void chgctrl_battery_id_work(struct work_struct *work)
{
	struct chgctrl *chip = container_of(work, struct chgctrl,
			battery_id_work);

	chgctrl_feature_trigger(chip, BATTERY_ID_TRIGGER);
}

static int chgctrl_psy_notifier_call(struct notifier_block *nb,
				     unsigned long event, void *v)
{
	struct chgctrl *chip = container_of(nb, struct chgctrl, psy_nb);
	struct power_supply *psy = (struct power_supply *)v;
	union power_supply_propval val;
	struct chgctrl_power_supply *charger;
	int ret;

	pr_debug("notified by %s\n", psy->desc->name);

	if (!strcmp(psy->desc->name, "battery")) {
		/* ignore if battery driver not initialzed */
		ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_TEMP, &val);
		if (ret || val.intval <= -1270)
			return NOTIFY_DONE;

		/* ignore if battery not exist */
		ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_PRESENT, &val);
		if (ret || val.intval == 0)
			return NOTIFY_DONE;

		schedule_work(&chip->battery_work);

		return NOTIFY_DONE;
	}

	if (!strcmp(psy->desc->name, "battery_id")) {
		schedule_work(&chip->battery_id_work);

		return NOTIFY_DONE;
	}

	for (charger = chip->charger; charger && charger->name; charger++) {
		if (!strcmp(psy->desc->name, charger->name)) {
			schedule_work(&chip->charger_work);
			return NOTIFY_DONE;
		}
	}

	return NOTIFY_DONE;
}

static int chgctrl_init_notifier(struct chgctrl *chip)
{
	/* power supply notifier */
	chip->psy_nb.notifier_call = chgctrl_psy_notifier_call;
	power_supply_reg_notifier(&chip->psy_nb);

	/* frame buffer notifier */
	chip->fb_nb.notifier_call = chgctrl_fb_notifier_call;
#ifdef CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY
	lge_panel_notifier_register_client(&chip->fb_nb);
#else
	fb_register_client(&chip->fb_nb);
#endif

	return 0;
}

/* power supply */
static enum power_supply_property chgctrl_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_USB_CURRENT_MAX,
	POWER_SUPPLY_PROP_STORE_DEMO_ENABLED,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
};

static int chgctrl_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct chgctrl *chip = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->battery_status;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->battery_present;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = chgctrl_vote_active_value(&chip->fcc);
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = chgctrl_vote_get_value(&chip->fcc,
				FCC_VOTER_DEFAULT);
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = chgctrl_vote_active_value(&chip->vfloat);
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = chgctrl_vote_get_value(&chip->vfloat,
				VFLOAT_VOTER_DEFAULT);
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = 1;
		if (chgctrl_vote_get_value(&chip->input_suspend,
				INPUT_SUSPEND_VOTER_USER))
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		val->intval = 1;
		if (!chgctrl_vote_get_value(&chip->fcc, FCC_VOTER_USER))
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_USB_CURRENT_MAX:
		val->intval = 0;
		if (!chgctrl_vote_get_value(&chip->icl_boost,
				ICL_BOOST_VOTER_USB_CURRENT_MAX))
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_STORE_DEMO_ENABLED:
		val->intval = chip->llk.store_demo_mode;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
#ifdef TIME_TO_FULL_MODE
		val->intval = chip->time_to_full_now;
#else
		val->intval = -1;
#endif
		if (chip->ccd_ttf >= 0)
			val->intval = chip->ccd_ttf;
		break;
#ifdef CONFIG_LGE_PM_QNOVO_QNS
	case POWER_SUPPLY_PROP_QNS_FCC:
		val->intval = chgctrl_vote_get_value(&chip->fcc,
				FCC_VOTER_QNOVO);
		break;
	case POWER_SUPPLY_PROP_QNS_VFLOAT:
		val->intval = chgctrl_vote_get_value(&chip->vfloat,
				VFLOAT_VOTER_QNOVO);
		break;
#endif
	default:
		break;
	}

	return 0;
}

static int chgctrl_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct chgctrl *chip = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		chgctrl_vote(&chip->input_suspend, INPUT_SUSPEND_VOTER_USER,
				val->intval ? 0 : 1);
		break;
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		chgctrl_vote(&chip->fcc, FCC_VOTER_USER, val->intval ? -1 : 0);
		break;
	case POWER_SUPPLY_PROP_USB_CURRENT_MAX:
		chgctrl_vote(&chip->icl_boost, ICL_BOOST_VOTER_USB_CURRENT_MAX,
				(val->intval ? 900 : -1));
		break;
	case POWER_SUPPLY_PROP_STORE_DEMO_ENABLED:
		if (chip->llk.store_demo_mode == val->intval)
			return 0;
		chip->llk.store_demo_mode = val->intval;
		chgctrl_feature_trigger(chip, LLK_TRIGGER);
		break;
#ifdef CONFIG_LGE_PM_QNOVO_QNS
	case POWER_SUPPLY_PROP_QNS_FCC:
		chgctrl_vote(&chip->fcc, FCC_VOTER_QNOVO, val->intval);
		break;
	case POWER_SUPPLY_PROP_QNS_VFLOAT:
		chgctrl_vote(&chip->vfloat, VFLOAT_VOTER_QNOVO, val->intval);
		break;
#endif
	default:
		break;
	}

	return 0;
}

static int chgctrl_property_is_writeable(struct power_supply *psy,
					 enum power_supply_property psp)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_USB_CURRENT_MAX:
	case POWER_SUPPLY_PROP_STORE_DEMO_ENABLED:
		ret = 1;
		break;
	default:
		break;
	}

	return ret;
}

static int chgctrl_init_power_supply(struct chgctrl *chip)
{
	chip->psy_desc.name = "charger_controller";
	chip->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	chip->psy_desc.properties = chgctrl_properties;
	chip->psy_desc.num_properties =
			ARRAY_SIZE(chgctrl_properties);
	chip->psy_desc.get_property = chgctrl_get_property;
	chip->psy_desc.set_property = chgctrl_set_property;
	chip->psy_desc.property_is_writeable = chgctrl_property_is_writeable;

	chip->psy_cfg.drv_data = chip;

	chip->psy = power_supply_register(chip->dev, &chip->psy_desc,
			&chip->psy_cfg);
	if (!chip->psy)
		return -1;

	return 0;
}

/* vote */
static char *icl_voters[] = {
	[ICL_VOTER_DEFAULT] = "default",
	[ICL_VOTER_USER] = "user",
	[ICL_VOTER_CCD] = "ccd",
	[ICL_VOTER_RESTRICTED] = "restricted",
	[ICL_VOTER_GAME] = "game",
	[ICL_VOTER_LLK] = "llk",
	[ICL_VOTER_PSEUDO_HVDCP] = "pseudo_hvdcp",
};

static char *fcc_voters[] = {
	[FCC_VOTER_DEFAULT] = "default",
	[FCC_VOTER_USER] = "user",
	[FCC_VOTER_CCD] = "ccd",
	[FCC_VOTER_QNOVO] = "qnovo",
	[FCC_VOTER_OTP] = "otp",
	[FCC_VOTER_SPEC] = "spec",
	[FCC_VOTER_ACTM] = "actm",
	[FCC_VOTER_THERMAL] = "thermal",
	[FCC_VOTER_DISPLAY] = "display",
	[FCC_VOTER_RESTRICTED] = "restricted",
	[FCC_VOTER_GAME] = "game",
	[FCC_VOTER_BATTERY_ID] = "battery_id",
	[FCC_VOTER_LLK] = "llk",
	[FCC_VOTER_ATCMD] = "atcmd",
	[FCC_VOTER_FACTORY] = "factory",
	[FCC_VOTER_BCC] = "bcc",
};

static char *vfloat_voters[] = {
	[VFLOAT_VOTER_DEFAULT] = "default",
	[VFLOAT_VOTER_USER] = "user",
	[VFLOAT_VOTER_CCD] = "ccd",
	[VFLOAT_VOTER_QNOVO] = "qnovo",
	[VFLOAT_VOTER_OTP] = "otp",
	[VFLOAT_VOTER_BATTERY_ID] = "battery_id",
};

static char *icl_boost_voters[] = {
	[ICL_BOOST_VOTER_USER] = "user",
	[ICL_BOOST_VOTER_PSEUDO_BATTERY] = "pseudo_battery",
	[ICL_BOOST_VOTER_USB_CURRENT_MAX] = "usb_current_max",
	[ICL_BOOST_VOTER_ATCMD] = "atcmd",
	[ICL_BOOST_VOTER_FACTORY] = "factory",
};

static char *input_suspend_voters[] = {
	[INPUT_SUSPEND_VOTER_USER] = "user",
	[INPUT_SUSPEND_VOTER_WATER_DETECT] = "water_detect",
	[INPUT_SUSPEND_VOTER_CHARGER_OV] = "charger_ov",
};

static char *fastchg_voters[] = {
	[FASTCHG_VOTER_DEFAULT] = "default",
	[FASTCHG_VOTER_USER] = "user",
	[FASTCHG_VOTER_CCD] = "ccd",
	[FASTCHG_VOTER_CAMERA] = "camera",
	[FASTCHG_VOTER_NETWORK] = "network",
	[FASTCHG_VOTER_FACTORY] = "factory",
};

static char *wless_pwr_voters[] = {
	[WLESS_PWR_VOTER_DEFAULT] = "default",
	[WLESS_PWR_VOTER_USER] = "user",
	[WLESS_PWR_VOTER_CCD] = "ccd",
	[WLESS_PWR_VOTER_ACTM] = "actm",
};

static int chgctrl_init_vote(struct chgctrl *chip)
{
	int ret;

	/* Input Current Limit */
	chip->icl.name = "icl";
	chip->icl.type = CC_VOTER_TYPE_MIN;
	chip->icl.voters = icl_voters;
	chip->icl.size = ICL_VOTER_MAX;
	chip->icl.function = chgctrl_icl_changed;
	chip->icl.args = chip;
	ret = chgctrl_vote_init(&chip->icl, chip->dev, chip->default_icl);
	if (ret)
		return ret;

	/* Fast Charge Current */
	chip->fcc.name = "fcc";
	chip->fcc.type = CC_VOTER_TYPE_MIN;
	chip->fcc.voters = fcc_voters;
	chip->fcc.size = FCC_VOTER_MAX;
	chip->fcc.function = chgctrl_fcc_changed;
	chip->fcc.args = chip;
	ret = chgctrl_vote_init(&chip->fcc, chip->dev, chip->default_fcc);
	if (ret)
		return ret;

	/* Floating Voltage */
	chip->vfloat.name = "vfloat";
	chip->vfloat.type = CC_VOTER_TYPE_MIN;
	chip->vfloat.voters = vfloat_voters;
	chip->vfloat.size = VFLOAT_VOTER_MAX;
	chip->vfloat.function = chgctrl_vfloat_changed;
	chip->vfloat.args = chip;
	ret = chgctrl_vote_init(&chip->vfloat, chip->dev, chip->default_vfloat);
	if (ret)
		return ret;

	/* Input Current Limit Boost */
	chip->icl_boost.name = "icl_boost";
	chip->icl_boost.type = CC_VOTER_TYPE_MAX;
	chip->icl_boost.voters = icl_boost_voters;
	chip->icl_boost.size = ICL_BOOST_VOTER_MAX;
	chip->icl_boost.function = chgctrl_icl_boost_changed;
	chip->icl_boost.args = chip;
	ret = chgctrl_vote_init(&chip->icl_boost, chip->dev, -1);
	if (ret)
		return ret;

	/* Input Suspend */
	chip->input_suspend.name = "input_suspend";
	chip->input_suspend.type = CC_VOTER_TYPE_TRIGGER;
	chip->input_suspend.voters = input_suspend_voters;
	chip->input_suspend.size = INPUT_SUSPEND_VOTER_MAX;
	chip->input_suspend.function = chgctrl_input_suspend_changed;
	chip->input_suspend.args = chip;
	ret = chgctrl_vote_init(&chip->input_suspend, chip->dev, -1);
	if (ret)
		return ret;

	/* Fast Charging */
	chip->fastchg.name = "fastchg";
	chip->fastchg.type = CC_VOTER_TYPE_MIN;
	chip->fastchg.voters = fastchg_voters;
	chip->fastchg.size = FASTCHG_VOTER_MAX;
	chip->fastchg.function = chgctrl_fastchg_changed;
	chip->fastchg.args = chip;
	ret = chgctrl_vote_init(&chip->fastchg, chip->dev,
			chip->default_fastchg);
	if (ret)
		return ret;

	/* Wirelesss */
	chip->wless_pwr.name = "wless_pwr";
	chip->wless_pwr.type = CC_VOTER_TYPE_MIN;
	chip->wless_pwr.voters = wless_pwr_voters;
	chip->wless_pwr.size = WLESS_PWR_VOTER_MAX;
	chip->wless_pwr.function = chgctrl_wless_pwr_changed;
	chip->wless_pwr.args = chip;
	ret = chgctrl_vote_init(&chip->wless_pwr, chip->dev,
			chip->default_wless_pwr);
	if (ret)
		return ret;

	return 0;
}

static void chgctrl_init_done_work(struct work_struct *work)
{
	struct chgctrl *chip = container_of(work, struct chgctrl,
			init_done_work);

	chgctrl_feature_trigger(chip, INIT_DONE_TRIGGER);
}

__attribute__((weak)) char *lge_get_model_name(void) { return NULL; }
static struct device_node *chgctrl_find_node_by_model(struct chgctrl *chip)
{
	struct device_node *np = chip->dev->of_node;
	struct property *prop;
	const char *cp;
	char *name;

	name = lge_get_model_name();
	if (!name)
		return NULL;

	np = of_get_child_by_name(np, "model");
	if (!np)
		return NULL;

	prop = of_find_property(np, "group", NULL);
	if (!prop)
		return NULL;

	for (cp = of_prop_next_string(prop, NULL); cp;
			cp = of_prop_next_string(prop, cp)) {
		if (!strcmp(name, cp))
			break;
		cp = of_prop_next_string(prop, cp);
	}
	if (!cp)
		return NULL;

	cp = of_prop_next_string(prop, cp);
	if (!cp)
		return NULL;

	return of_get_child_by_name(np, cp);
}

static int chgctrl_parse_dt_charger(struct chgctrl *chip,
				    struct device_node *np)
{
	int max_idx, idx, sz;
	int ret;

	max_idx = of_property_count_strings(np, "charger");
	if (max_idx <= 0)
		return 0;

	if (chip->charger)
		devm_kfree(chip->dev, chip->charger);

	sz = (max_idx + 1) * sizeof(struct chgctrl_power_supply);
	chip->charger = devm_kzalloc(chip->dev, sz, GFP_KERNEL);
	if (!chip->charger)
		return -ENOMEM;

	for (idx = 0; idx < max_idx; idx++) {
		ret = of_property_read_string_index(np, "charger",
				idx, &chip->charger[idx].name);
		if (ret)
			break;
	}

	return 0;
}

static int chgctrl_parse_dt_default(struct chgctrl *chip,
				    struct device_node *np)
{
	of_property_read_u32(np, "icl", &chip->default_icl);
	of_property_read_u32(np, "fcc", &chip->default_fcc);
	of_property_read_u32(np, "vfloat", &chip->default_vfloat);
	of_property_read_u32(np, "batt_therm_comp", &chip->batt_therm_comp);
	of_property_read_u32(np, "fastchg", &chip->default_fastchg);
	of_property_read_u32(np, "wless_pwr", &chip->default_wless_pwr);
	of_property_read_u32(np, "technology", &chip->technology);

	return 0;
}

static int chgctrl_parse_dt(struct chgctrl *chip)
{
	struct device_node *np = chip->dev->of_node;

	chgctrl_parse_dt_default(chip, np);
	chgctrl_parse_dt_charger(chip, np);
	chgctrl_feature_parse_dt(chip, np);

	np = chgctrl_find_node_by_model(chip);
	if (np) {
		chgctrl_parse_dt_default(chip, np);
		chgctrl_parse_dt_charger(chip, np);
		chgctrl_feature_parse_dt(chip, np);
	}

	return 0;
}

static void chgctrl_init_default(struct chgctrl *chip)
{
	chip->default_icl = -1;
	chip->default_fcc = -1;
	chip->default_vfloat = -1;
	chip->default_fastchg = -1;
	chip->default_wless_pwr = -1;
	chip->technology = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
	chip->charger = NULL;
	chgctrl_feature_init_default(chip);
}

static int chgctrl_probe(struct platform_device *pdev)
{
	struct chgctrl *chip = NULL;
	int ret = 0;

	chip = devm_kzalloc(&pdev->dev, sizeof(struct chgctrl), GFP_KERNEL);
	if (!chip) {
		pr_err("failed to alloc memory\n");
		return -ENOMEM;
	}
	chip->dev = &pdev->dev;
	chip->boot_mode = chgctrl_get_boot_mode();
	platform_set_drvdata(pdev, chip);

	INIT_WORK(&chip->init_done_work, chgctrl_init_done_work);
	INIT_WORK(&chip->battery_work, chgctrl_battery_work);
	INIT_WORK(&chip->charger_work, chgctrl_charger_work);
	INIT_WORK(&chip->fb_work, chgctrl_fb_work);
	INIT_WORK(&chip->battery_id_work, chgctrl_battery_id_work);

	chgctrl_init_default(chip);

	chip->ccd_status = POWER_SUPPLY_STATUS_UNKNOWN;
	chip->ccd_health = POWER_SUPPLY_HEALTH_UNKNOWN;
	chip->ccd_ttf = -1;

	ret = chgctrl_parse_dt(chip);
	if (ret) {
		pr_err("failed to parse dt\n");
		return ret;
	}

	ret = chgctrl_impl_init(chip);
	if (ret) {
		pr_err("failed to init impl\n");
		return ret;
	}

	ret = chgctrl_init_vote(chip);
	if (ret) {
		pr_err("failed to init vote\n");
		return ret;
	}

	ret = chgctrl_feature_init(chip);
	if (ret) {
		pr_err("failed to init feature\n");
		return ret;
	}

	ret = chgctrl_init_power_supply(chip);
	if (ret) {
		pr_err("failed to init power supply\n");
		return ret;
	}

	ret = chgctrl_init_notifier(chip);
	if (ret) {
		pr_err("failed to init notifier\n");
		return ret;
	}

	schedule_work(&chip->init_done_work);

	return 0;
}

static int chgctrl_remove(struct platform_device *pdev)
{
	struct chgctrl *chip = platform_get_drvdata(pdev);

	chgctrl_feature_exit(chip);

	power_supply_unregister(chip->psy);

	return 0;
}

static struct of_device_id chgctrl_match_table[] = {
	{
		.compatible = "lge,charger-controller",
	},
	{ },
};

static struct platform_driver chgctrl_driver = {
	.probe = chgctrl_probe,
	.remove = chgctrl_remove,
	.driver = {
		.name = "charger-controller",
		.owner = THIS_MODULE,
		.of_match_table = chgctrl_match_table,
	},
};

static void chgctrl_init_async(void *data, async_cookie_t cookie)
{
	platform_driver_register(&chgctrl_driver);
}

static int __init chgctrl_init(void)
{
	async_schedule(chgctrl_init_async, NULL);

	return 0;
}

static void __exit chgctrl_exit(void)
{
	platform_driver_unregister(&chgctrl_driver);
}

module_init(chgctrl_init);
module_exit(chgctrl_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Charger IC Current Controller");
MODULE_VERSION("1.2");
