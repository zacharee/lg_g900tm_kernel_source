#define pr_fmt(fmt) "[CC]" fmt

#include <linux/moduleparam.h>
#include <linux/power_supply.h>

#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
#include <linux/power/lge_pseudo_batt.h>
#endif
#ifdef CONFIG_LGE_PM_PSEUDO_HVDCP
#include <linux/power/lge_pseudo_hvdcp.h>
#endif

#include "charger_controller.h"

/* charging step */
enum chgstep {
	CHGSTEP_DISCHG,
	CHGSTEP_TRICLKE,
	CHGSTEP_PRECHARGING,
	CHGSTEP_FAST,
	CHGSTEP_FULLON,
	CHGSTEP_TAPER,
	CHGSTEP_EOC,
	CHGSTEP_INHIBIT,
};

static const char* const chgstep_str[] = {
	[CHGSTEP_DISCHG] = "0 DISCHG",
	[CHGSTEP_TRICLKE] = "1 TRICKLE",
	[CHGSTEP_PRECHARGING] = "2 PRECHARGING",
	[CHGSTEP_FAST] = "3 FAST",
	[CHGSTEP_FULLON] = "4 FULLON",
	[CHGSTEP_TAPER] = "5 TAPER",
	[CHGSTEP_EOC] = "6 EOC",
	[CHGSTEP_INHIBIT] = "7 INHIBIT",
};

static int param_get_chgstep(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int chgstep;

	if (!chip)
		return scnprintf(buffer, PAGE_SIZE, "not ready");

	switch (chip->battery_status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		chgstep = CHGSTEP_FAST;
		break;
	case POWER_SUPPLY_STATUS_FULL:
		chgstep = CHGSTEP_EOC;
		break;
	default:
		chgstep = CHGSTEP_DISCHG;
		break;
	}

	return scnprintf(buffer, PAGE_SIZE, "%s", chgstep_str[chgstep]);
}
static struct kernel_param_ops chgctrl_chgstep_ops = {
	.get = param_get_chgstep,
};
module_param_cb(chgstep, &chgctrl_chgstep_ops, NULL, CC_RO_PERM);

/* charger type */
static int param_get_chgtype(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	const char *chgtype = "Unknown";

	if (!chip)
		return scnprintf(buffer, PAGE_SIZE, "not ready");

	chgtype = chgctrl_get_charger_name(chip);

	return scnprintf(buffer, PAGE_SIZE, "%s", chgtype);
}
static struct kernel_param_ops chgctrl_chgtype_ops = {
	.get = param_get_chgtype,
};
module_param_cb(chgtype, &chgctrl_chgtype_ops, NULL, CC_RO_PERM);

/* voter control */
static int param_set_vote(const char *val, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	struct chgctrl_vote* vote = NULL;
	int voter;
	int value;
	char vote_name[20];
	char voter_name[20];
	int ret;

	if (!chip)
		return -ENODEV;

	ret = sscanf(val, "%19s %19s %d", vote_name, voter_name, &value);
	if (ret != 3)
		return -EINVAL;

	/* find vote */
	if (!strcmp(vote_name, "icl"))
		vote = &chip->icl;
	if (!strcmp(vote_name, "fcc"))
		vote = &chip->fcc;
	if (!strcmp(vote_name, "vfloat"))
		vote = &chip->vfloat;
	if (!strcmp(vote_name, "icl_boost"))
		vote = &chip->icl_boost;
	if (!strcmp(vote_name, "fastchg"))
		vote = &chip->fastchg;
	if (!strcmp(vote_name, "wless_pwr"))
		vote = &chip->wless_pwr;
	if (!strcmp(vote_name, "restricted"))
		vote = &chip->restricted;
	if (!vote)
		return -EINVAL;

	/* find voter */
	for (voter = 0; voter < vote->size; voter++) {
		if (!strcmp(voter_name, vote->voters[voter]))
			break;
	}
	if (voter >= vote->size)
		return -EINVAL;

	return chgctrl_vote(vote, voter, value);
}

static int param_get_vote(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int voter;
	int ret = 0;

	if (!chip)
		return scnprintf(buffer, PAGE_SIZE, "not ready");

	/* show icl */
	voter = chgctrl_vote_active_voter(&chip->icl);
	if (voter >= 0) {
		ret += scnprintf(buffer + ret, PAGE_SIZE, "icl : %d by %s\n",
				chgctrl_vote_active_value(&chip->icl),
				chip->icl.voters[voter]);
	}

	/* show fcc */
	voter = chgctrl_vote_active_voter(&chip->fcc);
	if (voter >= 0) {
		ret += scnprintf(buffer + ret, PAGE_SIZE, "fcc : %d by %s\n",
				chgctrl_vote_active_value(&chip->fcc),
				chip->fcc.voters[voter]);
	}

	/* show restricted if active */
	if (voter == FCC_VOTER_RESTRICTED) {
		voter = chgctrl_vote_active_voter(&chip->restricted);
		ret += scnprintf(buffer + ret, PAGE_SIZE,
				" - restricted : %d by %s\n",
				chgctrl_vote_active_value(&chip->restricted),
				chip->restricted.voters[voter]);
	}

	/* show vfloat */
	voter = chgctrl_vote_active_voter(&chip->vfloat);
	if (voter >= 0) {
		ret += scnprintf(buffer + ret, PAGE_SIZE,
			"vfloat : %d by %s\n",
			chgctrl_vote_active_value(&chip->vfloat),
			chip->vfloat.voters[voter]);
	}

	/* show fastchg */
	voter = chgctrl_vote_active_voter(&chip->fastchg);
	if (voter >= 0) {
		ret += scnprintf(buffer + ret, PAGE_SIZE,
			"fastchg : %d by %s\n",
			chgctrl_vote_active_value(&chip->fastchg),
			chip->fastchg.voters[voter]);
	}

	/* show wless_pwr */
	voter = chgctrl_vote_active_voter(&chip->wless_pwr);
	if (voter >= 0) {
		ret += scnprintf(buffer + ret, PAGE_SIZE,
			"wless_pwr : %d by %s\n",
			chgctrl_vote_active_value(&chip->wless_pwr),
			chip->wless_pwr.voters[voter]);
	}

	/* input suspend */
	voter = chgctrl_vote_active_voter(&chip->input_suspend);
	if (voter >= 0) {
		ret += scnprintf(buffer + ret, PAGE_SIZE,
			"input suspend : triggered by %s\n",
			chip->input_suspend.voters[voter]);
	}

	return ret;
}

static struct kernel_param_ops chgctrl_vote_ops = {
	.set = param_set_vote,
	.get = param_get_vote,
};
module_param_cb(vote, &chgctrl_vote_ops, NULL, CC_RW_PERM);

/* information */
static int info_interval = 60000;
module_param_named(info_interval, info_interval, int, CC_RW_PERM);

static void chgctrl_info_work(struct work_struct *work)
{
	struct chgctrl *chip = container_of(work,
			struct chgctrl, info_work.work);
	char *options = "None";
	int voter;

	__pm_stay_awake(&chip->info_ws);

	/* update data */
	chgctrl_get_battery_temperature(chip);

	if (chip->llk.store_demo_mode)
		options = "StoreDemo";

	if (chgctrl_vote_get_value(&chip->icl_boost,
			ICL_BOOST_VOTER_USB_CURRENT_MAX) > 0)
		options = "USBMax";

#ifdef CONFIG_LGE_PM_PSEUDO_HVDCP
	if (pseudo_hvdcp_is_enabled())
		options = "FakeHVDCP";
#endif

#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
	if (get_pseudo_batt_info(PSEUDO_BATT_MODE))
		options = "FakeBattery";
#endif

	/* print info */
	pr_info("[CC_INFO]BAT(%dp,%dmV,%s%d.%dd,%dmA) "
			"CHG(%s,%dmV,%dmV,%dmA,%dmA) "
			"OPT(%s)\n",
			/* Battery */
			chgctrl_get_battery_capacity(chip),
			chgctrl_get_battery_voltage_now(chip) / 1000,
			chip->battery_temperature < 0 ? "-" : "",
			abs(chip->battery_temperature / 10),
			abs(chip->battery_temperature % 10),
			chgctrl_get_battery_current_now(chip) / 1000,
			/* Charger */
			chgctrl_get_charger_name(chip),
			chgctrl_get_charger_voltage_now(chip) / 1000,
			chgctrl_get_charger_voltage_max(chip) / 1000,
			chgctrl_get_charger_current_max(chip) / 1000,
			chgctrl_get_battery_current_max(chip) / 1000,
			/* Options */
			options);

	/* print vote */
	pr_info("[CC_VOTE]icl:%d fcc:%d vfloat:%d fastchg:%d wless_pwr:%d\n",
			chgctrl_vote_active_value(&chip->icl),
			chgctrl_vote_active_value(&chip->fcc),
			chgctrl_vote_active_value(&chip->vfloat),
			chgctrl_vote_active_value(&chip->fastchg),
			chgctrl_vote_active_value(&chip->wless_pwr));
	if (chgctrl_vote_active_value(&chip->input_suspend)) {
		voter = chgctrl_vote_active_voter(&chip->input_suspend);
		pr_info("[CC_VOTE]input suspended by %s\n",
				chip->input_suspend.voters[voter]);
	}

	schedule_delayed_work(&chip->info_work,
			msecs_to_jiffies(info_interval));

	__pm_relax(&chip->info_ws);
}

static int chgctrl_info_trigger(struct chgctrl *chip, int trig)
{
	if (trig != BATTERY_TRIGGER && trig != CHARGER_TRIGGER)
		return 0;

	schedule_delayed_work(&chip->info_work, 0);

	return 0;
}

static int chgctrl_info_exit(struct chgctrl *chip)
{
	cancel_delayed_work(&chip->info_work);

	return 0;
}

static int chgctrl_info_init(struct chgctrl *chip)
{
	wakeup_source_init(&chip->info_ws, "chgctrl info");
	INIT_DELAYED_WORK(&chip->info_work, chgctrl_info_work);

	return 0;
}

struct chgctrl_feature chgctrl_feature_info = {
	.name = "info",
	.init = chgctrl_info_init,
	.exit = chgctrl_info_exit,
	.trigger = chgctrl_info_trigger,
};
