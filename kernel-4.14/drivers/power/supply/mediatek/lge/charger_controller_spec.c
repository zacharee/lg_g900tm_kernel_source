#define pr_fmt(fmt) "[CC][SPEC]%s: " fmt, __func__

#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>

#include "charger_controller.h"

static bool chgctrl_spec_is_index_valid(struct chgctrl_spec *spec, int idx)
{
	if (idx < 0)
		return false;
	if (idx >= spec->data_size)
		return false;

	return true;
}

static void chgctrl_spec_vote_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct chgctrl_spec *spec = container_of(dwork, struct chgctrl_spec,
			vote_work);
	struct chgctrl *chip = container_of(spec, struct chgctrl, spec);
	struct chgctrl_spec_data *data = spec->data;
	int target_idx, idx = spec->step_idx;
	int target_fcc, fcc = 0;
	int volt;

	mutex_lock(&spec->lock);
	target_idx = spec->idx;
	mutex_unlock(&spec->lock);

	/* spec out. update immediately */
	if (!chgctrl_spec_is_index_valid(spec, target_idx)) {
		target_fcc = 0;
		goto vote;
	}

	target_fcc = data[target_idx].curr;
	fcc = chgctrl_vote_get_value(&chip->fcc, FCC_VOTER_SPEC);

	/* spec in, update immediately */
	if (!chgctrl_spec_is_index_valid(spec, idx)) {
		fcc = target_fcc;
		goto vote;
	}

	/* charger not present. update immediately */
	if (!chip->charger_online) {
		fcc = target_fcc;
		goto vote;
	}

	/* temperature spec changed. update immediately */
	if (data[idx].tmin != data[target_idx].tmin &&
			data[idx].tmax != data[target_idx].tmax) {
		fcc = target_fcc;
		goto vote;
	}

	/* fcc can be increased. update immediately */
	if (fcc <= target_fcc)
		goto vote;

	volt = chgctrl_get_battery_voltage_now(chip) / 1000;
	if (volt > spec->vfloat)
		volt = spec->vfloat;

	/* voltage decreased enough */
	if (volt < data[idx].volt)
		goto vote;

	/* step down fcc */
	fcc -= spec->step_fcc;
	if (fcc < target_fcc)
		fcc = target_fcc;

vote:
	chgctrl_vote(&chip->fcc, FCC_VOTER_SPEC, fcc);
	if (fcc <= target_fcc)
		spec->step_idx = target_idx;

	/* step meets the target. stop monitoring */
	if (spec->step_idx == target_idx)
		return;

	schedule_delayed_work(dwork, msecs_to_jiffies(spec->step_ms));
}

static bool chgctrl_spec_in_hysterysis(struct chgctrl_spec *spec,
		int next, int volt, int temp)
{
	struct chgctrl_spec_data *data = spec->data;
	int present = spec->idx;

	/* if temperature spec is same, check only voltage */
	if (data[next].tmin == data[present].tmin &&
			data[next].tmax == data[present].tmax)
		goto check_voltage;

	if (temp > data[next].tmax - 2)
		return true;
	if (temp < data[next].tmin + 2)
		return true;

	return false;

check_voltage:
	if (volt > data[next].volt - 200)
		return true;

	return false;
}

static void chgctrl_spec_work(struct work_struct *work)
{
	struct chgctrl_spec *spec = container_of(work, struct chgctrl_spec,
			work);
	struct chgctrl *chip = container_of(spec, struct chgctrl, spec);
	struct chgctrl_spec_data *data = spec->data;
	int size = spec->data_size;
	int volt, temp;
	int fcc = 0;
	int idx;

	/* spec data not exist. do not go further */
	if (!data)
		return;

	/* update battery data */
	temp = chgctrl_get_battery_temperature(chip) / 10;
	volt = chgctrl_get_battery_voltage_now(chip) / 1000;
	if (volt > spec->vfloat)
		volt = spec->vfloat;

	for (idx = 0; idx < size; idx++) {
		if (temp < data[idx].tmin || temp >= data[idx].tmax)
			continue;
		if (volt > data[idx].volt)
			continue;

		/* found spec */
		fcc = data[idx].curr;
		break;
	}

	/* same spec selected, ignore */
	if (idx == spec->idx)
		return;

	/* spec fcc first selected. update immediately */
	if (!chgctrl_spec_is_index_valid(spec, spec->idx))
		goto update;

	/* fcc must be decreased. update immediately */
	if (fcc <= data[spec->idx].curr)
		goto update;

	/* charger not present. update immediately */
	if (!chip->charger_online)
		goto update;

	/* check hysterisis range */
	if (chgctrl_spec_in_hysterysis(spec, idx, volt, temp))
		return;

update:
	/* update selected spec */
	mutex_lock(&spec->lock);
	spec->idx = idx;
	mutex_unlock(&spec->lock);

	if (spec->step_fcc > 0 && spec->step_ms > 0) {
		schedule_delayed_work(&spec->vote_work, 0);
		return;
	}

	chgctrl_vote(&chip->fcc, FCC_VOTER_SPEC, fcc);
}

static int chgctrl_spec_trigger(struct chgctrl *chip, int trig)
{
	struct chgctrl_spec *spec = &chip->spec;

	if (trig != BATTERY_TRIGGER)
		return 0;

	if (!spec->enabled)
		return 0;

	schedule_work(&spec->work);

	return 0;
}

static int chgctrl_spec_exit(struct chgctrl *chip)
{
	struct chgctrl_spec *spec = &chip->spec;

	if (!spec->enabled)
		return 0;

	spec->enabled = false;
	cancel_work(&spec->work);

	return 0;
}

static int chgctrl_spec_init(struct chgctrl *chip)
{
	struct chgctrl_spec *spec = &chip->spec;
	int i;

	spec->idx = -1;
	spec->step_idx = -1;

	mutex_init(&spec->lock);

	INIT_WORK(&spec->work, chgctrl_spec_work);
	INIT_DELAYED_WORK(&spec->vote_work, chgctrl_spec_vote_work);

	if (spec->data_size <= 0)
		return 0;

	if (!spec->data)
		return 0;

	/* dump spec */
	for (i = 0; i < spec->data_size; i++) {
		pr_info("%2d~%2ddegC %4dmV %4dmA\n",
			spec->data[i].tmin, spec->data[i].tmax,
			spec->data[i].volt, spec->data[i].curr);
	}
	pr_info("vfloat = %4dmV\n", spec->vfloat);

	spec->enabled = true;

	return 0;
}

static int chgctrl_spec_parse_dt(struct chgctrl *chip, struct device_node *np)
{
	struct chgctrl_spec *spec = &chip->spec;
	struct property *prop = NULL;
	const __be32 *data = NULL;
	int size;
	int ret = 0;
	int i;

	prop = of_find_property(np, "spec", &size);
	if (!prop)
		return -ENODATA;

	/* invalid data size */
	if (!size || size % sizeof(struct chgctrl_spec_data))
		return -EINVAL;

	/* drop previous data */
	if (spec->data) {
		devm_kfree(chip->dev, spec->data);
		spec->data = NULL;
		spec->data_size = 0;
	}

	spec->data_size = size / sizeof(struct chgctrl_spec_data);
	spec->data = devm_kmalloc(chip->dev, size, GFP_KERNEL);
	if (!spec->data)
		return -ENOMEM;

	spec->vfloat = 0;
	for (i = 0; i < spec->data_size; i++) {
		data = of_prop_next_u32(prop, data, &spec->data[i].tmin);
		data = of_prop_next_u32(prop, data, &spec->data[i].tmax);
		data = of_prop_next_u32(prop, data, &spec->data[i].volt);
		data = of_prop_next_u32(prop, data, &spec->data[i].curr);

		if (spec->vfloat < spec->data[i].volt)
			spec->vfloat = spec->data[i].volt;
	}

	of_property_read_u32(np, "spec-step-fcc", &spec->step_fcc);
	of_property_read_u32(np, "spec-step-ms", &spec->step_ms);

	return ret;
}

static void chgctrl_spec_init_default(struct chgctrl *chip)
{
	struct chgctrl_spec *spec = &chip->spec;

	spec->data = NULL;
	spec->data_size = 0;
	spec->step_fcc = 0;
	spec->step_ms = 0;
}

struct chgctrl_feature chgctrl_feature_spec = {
	.name = "spec",
	.init_default = chgctrl_spec_init_default,
	.parse_dt = chgctrl_spec_parse_dt,
	.init = chgctrl_spec_init,
	.exit = chgctrl_spec_exit,
	.trigger = chgctrl_spec_trigger,
};
