#define pr_fmt(fmt) "[CC]%s: " fmt, __func__

#include <linux/power_supply.h>
#include <linux/power/charger_controller.h>

#include "charger_controller.h"

__attribute__((weak))
struct power_supply *chgctrl_get_charger_psy(struct chgctrl *chip)
{
	struct chgctrl_power_supply *charger;
	union power_supply_propval val;
	int ret;

	for (charger = chip->charger; charger && charger->name; charger++) {
		if (!charger->psy) {
			charger->psy =
				power_supply_get_by_name(charger->name);
			if (!charger->psy)
				continue;
		}

		ret = power_supply_get_property(charger->psy,
				POWER_SUPPLY_PROP_ONLINE, &val);
		if (!ret && val.intval)
			return charger->psy;
	}

	return NULL;
}

__attribute__((weak))
enum power_supply_type chgctrl_get_charger_type(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_charger_psy(chip);

	if (psy)
		return psy->desc->type;

	return POWER_SUPPLY_TYPE_UNKNOWN;
}

__attribute__((weak))
const char *chgctrl_get_charger_name(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_charger_psy(chip);

	if (!psy)
		goto out;

	switch (psy->desc->type) {
	case POWER_SUPPLY_TYPE_MAINS:
		return "Mains";
	case POWER_SUPPLY_TYPE_USB:
		return "USB";
	case POWER_SUPPLY_TYPE_WIRELESS:
		return "Wireless";
	default:
		break;
	}

out:
	return "Unknown";
}

__attribute__((weak))
int chgctrl_get_charger_voltage_now(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_charger_psy(chip);
	union power_supply_propval val;
	int ret;

	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW,
			&val);
	if (!ret)
		return val.intval;

out:
	return 0;
}

__attribute__((weak))
int chgctrl_get_charger_voltage_max(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_charger_psy(chip);
	union power_supply_propval val;
	int ret;

	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX,
			&val);
	if (!ret)
		return val.intval;

out:
	return 0;
}

__attribute__((weak))
int chgctrl_get_charger_current_max(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_charger_psy(chip);
	union power_supply_propval val;
	int ret;

	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX,
			&val);
	if (!ret)
		return val.intval;

out:
	return 0;
}

__attribute__((weak))
struct power_supply *chgctrl_get_battery_psy(struct chgctrl *chip)
{
	static struct power_supply *psy = NULL;

	if (!psy)
		psy = power_supply_get_by_name("battery");

	return psy;
}

__attribute__((weak))
int chgctrl_get_battery_status(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_battery_psy(chip);
	union power_supply_propval val;
	int ret;

	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (!ret)
		return val.intval;

out:
	return POWER_SUPPLY_STATUS_UNKNOWN;
}

__attribute__((weak))
int chgctrl_get_battery_health(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_battery_psy(chip);
	union power_supply_propval val;
	int ret;

	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
	if (!ret)
		return val.intval;

out:
	return POWER_SUPPLY_HEALTH_UNKNOWN;
}

__attribute__((weak))
int chgctrl_get_battery_present(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_battery_psy(chip);
	union power_supply_propval val;
	int ret;

	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
	if (!ret)
		return val.intval;

out:
	return 1;
}

__attribute__((weak))
int chgctrl_get_battery_voltage_now(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_battery_psy(chip);
	union power_supply_propval val;
	int ret;

	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW,
			&val);
	if (!ret)
		return val.intval;

out:
	return 4000000;	/* 4000000 uV */
}

__attribute__((weak))
int chgctrl_get_battery_current_max(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_battery_psy(chip);
	union power_supply_propval val;
	int ret;

	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX,
			&val);
	if (!ret)
		return val.intval;
out:
	return 0;
}

__attribute__((weak))
int chgctrl_get_battery_current_now(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_battery_psy(chip);
	union power_supply_propval val;
	int ret;

	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW,
			&val);
	if (!ret)
		return val.intval;

out:
	return 0;
}

__attribute__((weak))
int chgctrl_get_battery_capacity(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_battery_psy(chip);
	union power_supply_propval val;
	int ret;

	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (!ret)
		return val.intval;

out:
	return 50;
}

__attribute__((weak))
int chgctrl_get_battery_ttf_capacity(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_battery_psy(chip);
	union power_supply_propval val;
	int ret;

	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TTF_CAPACITY,
			&val);
	if (!ret)
		return val.intval;

out:
	return 500;	/* 50.0 percent */
}

__attribute__((weak))
int chgctrl_get_battery_temperature(struct chgctrl *chip)
{
	struct power_supply *psy = chgctrl_get_battery_psy(chip);
	union power_supply_propval val;
	int ret;

	if (!psy)
		goto out;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (!ret)
		return val.intval;

out:
	return 200;	/* 20.0 degreeC */
}

__attribute__((weak))
int chgctrl_get_boot_mode(void) { return CC_BOOT_MODE_NORMAL; }

__attribute__((weak))
void chgctrl_icl_changed(void *args)
{
	struct chgctrl *chip = args;

	chgctrl_changed(chip);
}

__attribute__((weak))
void chgctrl_fcc_changed(void *args)
{
	struct chgctrl *chip = args;

	chgctrl_changed(chip);
}

__attribute__((weak))
void chgctrl_vfloat_changed(void *args)
{
	struct chgctrl *chip = args;

	chgctrl_changed(chip);
}

__attribute__((weak))
void chgctrl_icl_boost_changed(void *args)
{
	struct chgctrl *chip = args;

	chgctrl_changed(chip);
}

__attribute__((weak))
void chgctrl_input_suspend_changed(void *args)
{
	struct chgctrl *chip = args;

	chgctrl_changed(chip);
}

__attribute__((weak))
void chgctrl_fastchg_changed(void *args)
{
	struct chgctrl *chip = args;

	chgctrl_changed(chip);
}

__attribute__((weak))
void chgctrl_wless_pwr_changed(void *args)
{
	struct chgctrl *chip = args;

	chgctrl_changed(chip);
}

__attribute__((weak))
int chgctrl_impl_init(struct chgctrl *chip) { return 0; }
