#define pr_fmt(fmt) "[CC]%s: " fmt, __func__

#include <linux/device.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/power/charger_controller.h>

#include "charger_controller.h"

static int chgctrl_vote_find_min(struct chgctrl_vote* vote)
{
	int voter = -1;
	int value = INT_MAX;
	int i;

	for (i = 0; i < vote->size; i++) {
		if (vote->values[i] < 0)
			continue;

		if (vote->values[i] < value) {
			voter = i;
			value = vote->values[i];
		}
	}

	return voter;
}

static int chgctrl_vote_find_max(struct chgctrl_vote* vote)
{
	int voter = -1;
	int value = -1;
	int i;

	for (i = 0; i < vote->size; i++) {
		if (vote->values[i] < 0)
			continue;

		if (vote->values[i] > value) {
			voter = i;
			value = vote->values[i];
		}
	}

	return voter;
}

static int chgctrl_vote_find_trigger(struct chgctrl_vote* vote)
{
	int i;

	for (i = 0; i < vote->size; i++) {
		if (vote->values[i] > 0)
			break;
	}

	if (i >= vote->size)
		return -1;

	return i;
}

static bool chgctrl_vote_update_vote(struct chgctrl_vote* vote)
{
	bool changed = false;
	int voter;
	int value;

	switch (vote->type) {
	case CC_VOTER_TYPE_TRIGGER:
		voter = chgctrl_vote_find_trigger(vote);
		value = ((voter < 0) ? 0 : vote->values[voter]);
		break;
	case CC_VOTER_TYPE_MAX:
		voter = chgctrl_vote_find_max(vote);
		value = ((voter < 0) ? -1 : vote->values[voter]);
		break;
	case CC_VOTER_TYPE_MIN:
		/* default voter type. fall through */
	default:
		voter = chgctrl_vote_find_min(vote);
		value = ((voter < 0) ? -1 : vote->values[voter]);
		break;
	}

	if (voter != vote->active_voter) {
		vote->active_voter = voter;
		changed = true;
	}
	if (value != vote->active_value) {
		vote->active_value = value;
		changed = true;
	}

	return changed;
}

static void chgctrl_vote_handler(struct work_struct *work)
{
	struct chgctrl_vote* vote = container_of(work,
			struct chgctrl_vote, handler);

	if (vote->function)
		vote->function(vote->args);
}

int chgctrl_vote(struct chgctrl_vote* vote, int voter, int value)
{
	bool changed = false;

	if (!vote || !vote->values)
		return -ENODEV;

	if (voter < 0 || voter >= vote->size)
		return -EINVAL;

	/* trigger type accept only 0 or 1 */
	if (vote->type == CC_VOTER_TYPE_TRIGGER) {
		if (value < 0)
			value = 0;
		if (value > 1)
			value = 1;
	}

	mutex_lock(&vote->lock);
	if (vote->values[voter] == value) {
		mutex_unlock(&vote->lock);
		return 0;
	}

	if (vote->name && vote->voters) {
		pr_info("%s: %s vote %d\n",
			vote->name, vote->voters[voter], value);
	}
	vote->values[voter] = value;

	changed = chgctrl_vote_update_vote(vote);
	if (!changed) {
		mutex_unlock(&vote->lock);
		return 0;
	}

	if (vote->active_voter >= 0) {
		if (vote->name && vote->voters) {
			pr_info("%s: active vote %d by %s\n",
					vote->name,
					vote->active_value,
					vote->voters[vote->active_voter]);
		}
	}

	mutex_unlock(&vote->lock);

	if (vote->function)
		schedule_work(&vote->handler);

	return 0;
}

void chgctrl_vote_dump(struct chgctrl_vote* vote)
{
	int i;

	if (!vote->name || !vote->voters)
		return;

	mutex_lock(&vote->lock);

	for (i = 0; i < vote->size; i++) {
		if (vote->values[i] < 0) {
			pr_info("%s: %s is not voting\n", vote->name,
					vote->voters[i]);
			continue;
		}
		pr_info("%s: %s is voting %d\n", vote->name,
				vote->voters[i], vote->values[i]);
	}

	mutex_unlock(&vote->lock);
}

int chgctrl_vote_active_value(struct chgctrl_vote* vote)
{
	if (!vote || !vote->values)
		return -ENODEV;

	return vote->active_value;
}

int chgctrl_vote_active_voter(struct chgctrl_vote* vote)
{
	if (!vote || !vote->values)
		return -ENODEV;

	return vote->active_voter;
}

int chgctrl_vote_get_value(struct chgctrl_vote* vote, int voter)
{
	if (!vote || !vote->values)
		return -ENODEV;

	return vote->values[voter];
}

int chgctrl_vote_init(struct chgctrl_vote* vote, struct device *dev, int value)
{
	int i;

	if (!vote)
		return -ENODEV;

	vote->values = (int *)devm_kzalloc(dev,
			sizeof(int) * vote->size, GFP_KERNEL);
	if (!vote->values)
		return -ENOMEM;

	mutex_init(&vote->lock);
	INIT_WORK(&vote->handler, chgctrl_vote_handler);

	/* need to set negative except type is trigger */
	if (vote->type != CC_VOTER_TYPE_TRIGGER) {
		for (i = 0; i < vote->size; i++)
			vote->values[i] = -1;
		vote->active_value = -1;
	}
	vote->active_voter = -1;

	/* set default vote */
	if (value >= 0) {
		vote->values[0] = value;
		vote->active_voter = 0;
		vote->active_value = value;

		if (vote->function)
			schedule_work(&vote->handler);
	}

	return 0;
}

/* pseudo power-supply support */
void chgctrl_set_pseudo_mode(int mode, int en)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int voter;
	int value;

	/* charger-contoller not ready. return */
	if (!chip)
		return;

	if (mode == PSEUDO_BATTERY) {
		/* set icl boost */
		voter = ICL_BOOST_VOTER_PSEUDO_BATTERY;
		value = en ? 900 : -1;

		chgctrl_vote(&chip->icl_boost, voter, value);
	}

	if (mode == PSEUDO_HVDCP) {
		/* set icl to stabilize vbus */
		voter = ICL_VOTER_PSEUDO_HVDCP;
		value = en ? 1000 : -1;

		chgctrl_vote(&chip->icl, voter, value);
	}
}
EXPORT_SYMBOL(chgctrl_set_pseudo_mode);

/* water detect support */
void chgctrl_set_water_detect(bool detected)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	/* charger-contoller not ready. return */
	if (!chip)
		return;

	chgctrl_vote(&chip->input_suspend,
			INPUT_SUSPEND_VOTER_WATER_DETECT,
			detected ? 1 : 0);
}
EXPORT_SYMBOL(chgctrl_set_water_detect);

/* ovp detect support */
void chgctrl_set_charger_ov(bool detected)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	/* charger-contoller not ready. return */
	if (!chip)
		return;

	chgctrl_vote(&chip->input_suspend,
			INPUT_SUSPEND_VOTER_CHARGER_OV,
			detected ? 1 : 0);
}
EXPORT_SYMBOL(chgctrl_set_charger_ov);
