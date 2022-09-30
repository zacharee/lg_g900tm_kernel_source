#define pr_fmt(fmt) "[CC][THERMAL]%s: " fmt, __func__

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>

#include "charger_controller.h"

static int chgctrl_thermal_get_temp(struct chgctrl_thermal *thermal)
{
	struct chgctrl *chip = container_of(thermal, struct chgctrl, thermal);

	return chgctrl_get_battery_temperature(chip);
}

static bool chgctrl_thermal_in_hysterysis(struct chgctrl_thermal *thermal,
					  int idx, int temp)
{
	int exit_temp = thermal->trip[idx].trigger - thermal->trip[idx].offset;

	if (temp > exit_temp)
		return true;

	return false;
}

static int chgctrl_thermal_find_idx(struct chgctrl_thermal *thermal, int temp)
{
	int i;

	for (i = 0; i < thermal->trip_size; i++) {
		if (temp < thermal->trip[i].trigger)
			break;
	}

	return i - 1;
}

static void chgctrl_thermal_work(struct work_struct *work)
{
	struct chgctrl_thermal *thermal = container_of(work,
			struct chgctrl_thermal, work);
	struct chgctrl *chip = container_of(thermal, struct chgctrl, thermal);
	int temp = chgctrl_thermal_get_temp(thermal);
	int fcc = -1;
	int idx;

	/* find initial idx */
	idx = chgctrl_thermal_find_idx(thermal, temp);

	/* same idx. nothing to do */
	if (idx == thermal->idx)
		return;

	/* idx increased. update */
	if (idx > thermal->idx) {
		fcc = thermal->trip[idx].curr;
		goto update;
	}

	/* idx decreased. check temp is in hysterysis range */
	for (idx = thermal->idx; idx >= 0; idx--) {
		if (chgctrl_thermal_in_hysterysis(thermal, idx, temp)) {
			fcc = thermal->trip[idx].curr;
			break;
		}
	}

update:
	thermal->idx = idx;

	chgctrl_vote(&chip->fcc, FCC_VOTER_THERMAL, fcc);
}

static int chgctrl_thermal_trigger(struct chgctrl *chip, int trig)
{
	struct chgctrl_thermal *thermal = &chip->thermal;

	if (trig != BATTERY_TRIGGER)
		return 0;

	if (!thermal->enabled)
		return 0;

	schedule_work(&thermal->work);

	return 0;
}

static int chgctrl_thermal_init(struct chgctrl *chip)
{
	struct chgctrl_thermal *thermal = &chip->thermal;
	int i;

	thermal->idx = -1;
	INIT_WORK(&thermal->work, chgctrl_thermal_work);

	if (!thermal->trip || !thermal->trip_size)
		return 0;

	/* dump thermal */
	for (i = 0; i < thermal->trip_size; i++) {
		pr_info("thermal: %2dd(-%dd) %4dmA",
				thermal->trip[i].trigger,
				thermal->trip[i].offset,
				thermal->trip[i].curr);
	}

	thermal->enabled = true;

	return 0;
}

static int chgctrl_thermal_parse_dt(struct chgctrl *chip,
				    struct device_node *np)
{
	struct chgctrl_thermal *thermal = &chip->thermal;
	struct property *prop = NULL;
	const __be32 *data = NULL;
	int size;
	int trigger = 0;
	int ret = 0;
	int i;

	prop = of_find_property(np, "thermal", &size);
	if (!prop)
		return -ENODATA;

	/* invalid data size */
	if (!size || size % sizeof(struct chgctrl_thermal_trip))
		return -EINVAL;

	/* drop previous data */
	if (thermal->trip) {
		devm_kfree(chip->dev, thermal->trip);
		thermal->trip = NULL;
		thermal->trip_size = 0;
	}

	thermal->trip_size = size / sizeof(struct chgctrl_thermal_trip);
	thermal->trip = devm_kmalloc(chip->dev, size, GFP_KERNEL);
	if (!thermal->trip)
		return -ENOMEM;

	for (i = 0; i < thermal->trip_size; i++) {
		data = of_prop_next_u32(prop, data, &thermal->trip[i].trigger);
		data = of_prop_next_u32(prop, data, &thermal->trip[i].offset);
		data = of_prop_next_u32(prop, data, &thermal->trip[i].curr);

		/* sanity check */
		if (trigger >= thermal->trip[i].trigger)
			goto sanity_check_failed;
		trigger = thermal->trip[i].trigger;
	}

	return ret;

sanity_check_failed:
	pr_err("thermal: invalid data at %d\n", i);
	pr_err("thermal: %d = <%2d, %d, %4d>\n", i,
			thermal->trip[i].trigger,
			thermal->trip[i].offset,
			thermal->trip[i].curr);

	devm_kfree(chip->dev, thermal->trip);
	thermal->trip = NULL;
	thermal->trip_size = 0;

	return -EINVAL;
}

struct chgctrl_feature chgctrl_feature_thermal = {
	.name = "thermal",
	.parse_dt = chgctrl_thermal_parse_dt,
	.init = chgctrl_thermal_init,
	.trigger = chgctrl_thermal_trigger,
};
