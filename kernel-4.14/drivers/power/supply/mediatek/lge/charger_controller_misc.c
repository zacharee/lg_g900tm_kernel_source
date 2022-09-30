#define pr_fmt(fmt) "[CC]%s: " fmt, __func__

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>

#include "charger_controller.h"

/* display */
static void chgctrl_fb_work(struct work_struct *work)
{
	struct chgctrl_fb *fb = container_of(to_delayed_work(work),
			struct chgctrl_fb, dwork);

	__pm_relax(&fb->ws);
}

static int chgctrl_fb_trigger(struct chgctrl *chip, int trig)
{
	struct chgctrl_fb *fb = &chip->fb;

	if (!fb->enabled)
		return 0;

	if (chip->boot_mode != CC_BOOT_MODE_NORMAL)
		return 0;

	if (trig == CHARGER_TRIGGER) {
		__pm_stay_awake(&fb->ws);
		schedule_delayed_work(&fb->dwork, msecs_to_jiffies(3000));
	}

	if (trig == FB_TRIGGER) {
		chgctrl_vote(&chip->fcc, FCC_VOTER_DISPLAY,
				chip->display_on ? fb->fcc : -1);

		if (chip->display_on) {
			cancel_delayed_work_sync(&fb->dwork);
			__pm_relax(&fb->ws);
		}
	}

	return 0;
}

static int chgctrl_fb_init(struct chgctrl *chip)
{
	struct chgctrl_fb *fb = &chip->fb;

	wakeup_source_init(&fb->ws, "chgctrl fb");
	INIT_DELAYED_WORK(&fb->dwork, chgctrl_fb_work);

	pr_info("fb: fcc=%dmA\n", fb->fcc);

	fb->enabled = true;

	return 0;
}

static int chgctrl_fb_parse_dt(struct chgctrl *chip, struct device_node *np)
{
	struct chgctrl_fb *fb = &chip->fb;

	of_property_read_u32(np, "fb-fcc", &fb->fcc);

	return 0;
}

static void chgctrl_fb_init_default(struct chgctrl *chip)
{
	struct chgctrl_fb *fb = &chip->fb;

	fb->fcc = -1;
}

struct chgctrl_feature chgctrl_feature_fb = {
	.name = "fb",
	.init_default = chgctrl_fb_init_default,
	.parse_dt = chgctrl_fb_parse_dt,
	.init = chgctrl_fb_init,
	.trigger = chgctrl_fb_trigger,
};

/* restricted charging */
enum {
	RESTRICTED_VOTER_LCD,
	RESTRICTED_VOTER_CALL,
	RESTRICTED_VOTER_TDMB,
	RESTRICTED_VOTER_UHDREC,
	RESTRICTED_VOTER_WFD,
	RESTRICTED_VOTER_MAX
};

static char *restricted_voters[] = {
	[RESTRICTED_VOTER_LCD] = "LCD",
	[RESTRICTED_VOTER_CALL] = "CALL",
	[RESTRICTED_VOTER_TDMB] = "TDMB",
	[RESTRICTED_VOTER_UHDREC] = "UHDREC",
	[RESTRICTED_VOTER_WFD] = "WFD",
};

static int chgctrl_restricted_get_voter(struct chgctrl *chip,
					const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(restricted_voters); i++) {
		if (!strcmp(name, restricted_voters[i]))
			break;
	}
	if (i >= ARRAY_SIZE(restricted_voters))
		return -ENODEV;

	return i;
}

static int chgctrl_restricted_get_value(struct chgctrl *chip,
					const char *name, const char *mode)
{
	struct device_node *np = chip->dev->of_node;
	char propname[30];
	int limit;
	int ret;

	if (!strcmp(mode, "OFF"))
		return -1;

	snprintf(propname, sizeof(propname), "restricted-%s-%s",
			name, mode);

	ret = of_property_read_u32(np, propname, &limit);
	if (ret)
		return -ENOTSUPP;

	return limit;
}

static int param_set_restricted(const char *val,
				const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	char name[10], mode[10];
	int voter, value;
	int ret;

	if (!chip) {
		pr_info("not ready yet. ignore\n");
		return -ENODEV;
	}

	ret = sscanf(val, "%s %s", name, mode);
	if (ret != 2)
		return -EINVAL;

	voter = chgctrl_restricted_get_voter(chip, name);
	if (voter == -ENODEV)
		return -ENODEV;

	value = chgctrl_restricted_get_value(chip, name, mode);
	if (value == -ENOTSUPP)
		return -ENOTSUPP;

	chgctrl_vote(&chip->restricted, voter, value);

	return 0;
}

static int param_get_restricted(char *buffer,
				const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int voter;

	if (!chip)
		return scnprintf(buffer, PAGE_SIZE, "not ready");

	voter = chgctrl_vote_active_voter(&chip->restricted);
	if (voter < 0)
		return scnprintf(buffer, PAGE_SIZE, "none");

	return scnprintf(buffer, PAGE_SIZE, "%s", restricted_voters[voter]);
}

static struct kernel_param_ops restricted_ops = {
	.set = param_set_restricted,
	.get = param_get_restricted,
};
module_param_cb(restricted_charging, &restricted_ops, NULL, CC_RW_PERM);

static void chgctrl_restricted_changed(void *args)
{
	struct chgctrl *chip = args;
	int value;

	value = chgctrl_vote_active_value(&chip->restricted);
	chgctrl_vote(&chip->fcc, FCC_VOTER_RESTRICTED, value);
}

static int chgctrl_restricted_init(struct chgctrl *chip)
{
	chip->restricted.name = "restricted";
	chip->restricted.type = CC_VOTER_TYPE_MIN;
	chip->restricted.voters = restricted_voters;
	chip->restricted.size = RESTRICTED_VOTER_MAX;
	chip->restricted.function = chgctrl_restricted_changed;
	chip->restricted.args = chip;

	return chgctrl_vote_init(&chip->restricted, chip->dev, -1);
}

struct chgctrl_feature chgctrl_feature_restricted = {
	.name = "restricted",
	.init = chgctrl_restricted_init,
};


/* Battery Care Charging */
static int param_set_bcc(const char *val,
				const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	struct chgctrl_bcc *bcc = &chip->bcc;
	int value;
	int ret;

	if (!chip) {
		pr_info("not ready yet. ignore\n");
		return -ENODEV;
	}

	ret = sscanf(val, "%d", &value);
	if (ret != 1)
		return -EINVAL;

	if (value > 0) {
		bcc->bcc_enabled = 1;
		chgctrl_vote(&chip->fcc, FCC_VOTER_BCC, bcc->bcc_current);
		chgctrl_feature_trigger(chip, BCC_TRIGGER);
	} else {
		if (bcc->bcc_enabled)
			chgctrl_feature_trigger(chip, BCC_TRIGGER);
		bcc->bcc_enabled = 0;
		chgctrl_vote(&chip->fcc, FCC_VOTER_BCC, -1);
	}

	chgctrl_changed(chip);

	return 0;
}

static int param_get_bcc(char *buffer,
				const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	struct chgctrl_bcc *bcc = &chip->bcc;

	if (!chip)
		return scnprintf(buffer, PAGE_SIZE, "not ready");

	return 	scnprintf(buffer, PAGE_SIZE, "%d",
		bcc->bcc_enabled ? bcc->bcc_current : 0);
}

static struct kernel_param_ops bcc_ops = {
	.set = param_set_bcc,
	.get = param_get_bcc,
};
module_param_cb(bcc, &bcc_ops, NULL, CC_RW_PERM);

static int chgctrl_bcc_parse_dt(struct chgctrl *chip,
				    struct device_node *np)
{
	struct chgctrl_bcc *bcc = &chip->bcc;

	of_property_read_u32(np, "bcc-current", &bcc->bcc_current);

	return 0;
}

static void chgctrl_bcc_init_default(struct chgctrl *chip)
{
	struct chgctrl_bcc *bcc = &chip->bcc;

	bcc->bcc_enabled = 0;
	bcc->bcc_current = 1800;
}

struct chgctrl_feature chgctrl_feature_bcc = {
	.name = "bcc",
	.init_default = chgctrl_bcc_init_default,
	.parse_dt = chgctrl_bcc_parse_dt,
};

/* store demo */
static void chgctrl_llk_work(struct work_struct *work)
{
	struct chgctrl_llk *llk = container_of(work, struct chgctrl_llk, work);
	struct chgctrl *chip = container_of(llk, struct chgctrl, llk);
	int capacity_now;

	static int store_demo_mode = 0;
	static int capacity = -1;

	if (!llk->store_demo_mode) {
		if (!store_demo_mode)
			return;

		store_demo_mode = 0;
		capacity = -1;

		/* clear vote */
		chgctrl_vote(&chip->icl, ICL_VOTER_LLK, -1);
		chgctrl_vote(&chip->fcc, FCC_VOTER_LLK, -1);

		return;
	}

	store_demo_mode = 1;

	capacity_now = chgctrl_get_battery_capacity(chip);
	if (capacity == capacity_now)
		return;

	capacity = capacity_now;

	/* limit input current */
	if (capacity > llk->soc_max)
		chgctrl_vote(&chip->icl, ICL_VOTER_LLK, 0);
	else
		chgctrl_vote(&chip->icl, ICL_VOTER_LLK, -1);

	/* limit charge current */
	if (capacity >= llk->soc_max)
		chgctrl_vote(&chip->fcc, FCC_VOTER_LLK, 0);
	if (capacity <= llk->soc_min)
		chgctrl_vote(&chip->fcc, FCC_VOTER_LLK, -1);
}

static int chgctrl_llk_trigger(struct chgctrl *chip, int trig)
{
	struct chgctrl_llk *llk = &chip->llk;

	if (!llk->enabled)
		return 0;

	if (trig != BATTERY_TRIGGER && trig != LLK_TRIGGER)
		return 0;

	schedule_work(&llk->work);

	return 0;
}

static int chgctrl_llk_exit(struct chgctrl *chip)
{
	struct chgctrl_llk *llk = &chip->llk;

	if (!llk->enabled)
		return 0;

	llk->enabled = false;

	cancel_work(&llk->work);

	return 0;
}

static int chgctrl_llk_init(struct chgctrl *chip)
{
	struct chgctrl_llk *llk = &chip->llk;

	INIT_WORK(&llk->work, chgctrl_llk_work);

	if (llk->soc_min <= 0)
		return -ENODEV;

	if (llk->soc_max <= 0)
		return -ENODEV;

	/* dump llk */
	pr_info("llk: min=%d%%, max=%d%%\n", llk->soc_min, llk->soc_max);

	llk->enabled = true;

	return 0;
}

static int chgctrl_llk_parse_dt(struct chgctrl *chip, struct device_node *np)
{
	struct chgctrl_llk *llk = &chip->llk;

	of_property_read_u32(np, "llk-soc-min", &llk->soc_min);
	of_property_read_u32(np, "llk-soc-max", &llk->soc_max);

	return 0;
}

static void chgctrl_llk_init_default(struct chgctrl *chip)
{
	struct chgctrl_llk *llk = &chip->llk;

	llk->soc_min = 45;
	llk->soc_max = 50;
}

struct chgctrl_feature chgctrl_feature_llk = {
	.name = "llk",
	.init_default = chgctrl_llk_init_default,
	.parse_dt = chgctrl_llk_parse_dt,
	.init = chgctrl_llk_init,
	.exit = chgctrl_llk_exit,
	.trigger = chgctrl_llk_trigger,
};

/* chargerlogo */
static int chgctrl_chargerlogo_trigger(struct chgctrl *chip, int trig)
{
	struct chgctrl_chargerlogo *chargerlogo = &chip->chargerlogo;

	if (chip->boot_mode != CC_BOOT_MODE_CHARGER)
		return 0;

	if (trig != INIT_DONE_TRIGGER)
		return 0;

	if (chargerlogo->icl >= 0)
		chgctrl_vote(&chip->icl, ICL_VOTER_DEFAULT, chargerlogo->icl);

	if (chargerlogo->fcc >= 0)
		chgctrl_vote(&chip->fcc, FCC_VOTER_DEFAULT, chargerlogo->fcc);

	return 0;
}

static int chgctrl_chargerlogo_parse_dt(struct chgctrl *chip,
					struct device_node *np)
{
	struct chgctrl_chargerlogo *chargerlogo = &chip->chargerlogo;

	of_property_read_u32(np, "chargerlogo-icl", &chargerlogo->icl);
	of_property_read_u32(np, "chargerlogo-fcc", &chargerlogo->fcc);

	return 0;
}

static void chgctrl_chargerlogo_init_default(struct chgctrl *chip)
{
	struct chgctrl_chargerlogo *chargerlogo = &chip->chargerlogo;

	chargerlogo->icl = -1;
	chargerlogo->fcc = -1;
}

struct chgctrl_feature chgctrl_feature_chargerlogo = {
	.name = "chargerlogo",
	.init_default = chgctrl_chargerlogo_init_default,
	.parse_dt = chgctrl_chargerlogo_parse_dt,
	.trigger = chgctrl_chargerlogo_trigger,
};

/* battery id */
static int chgctrl_battery_id_trigger(struct chgctrl *chip, int trig)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int fcc = -1, vfloat = -1;
	int ret;

	if (trig != BATTERY_ID_TRIGGER)
		return 0;

	psy = power_supply_get_by_name("battery_id");
	if (!psy)
		return 0;

#ifdef CONFIG_LGE_PM_BATTERY_ID
	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_BATT_ID, &val);
	if (!ret)
		fcc = val.intval ? -1 : 0;

	if (chip->boot_mode == CC_BOOT_MODE_FACTORY)
		fcc = -1;
#endif

	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	if (!ret)
		vfloat = val.intval / 1000;

	power_supply_put(psy);

	chgctrl_vote(&chip->fcc, FCC_VOTER_BATTERY_ID, fcc);
	chgctrl_vote(&chip->vfloat, VFLOAT_VOTER_BATTERY_ID, vfloat);

	return 0;
}

struct chgctrl_feature chgctrl_feature_battery_id = {
	.name = "battery_id",
	.trigger = chgctrl_battery_id_trigger,
};

/* factory */
static int chgctrl_factory_trigger(struct chgctrl *chip, int trig)
{
	struct chgctrl_factory *factory = &chip->factory;

	if (chip->boot_mode != CC_BOOT_MODE_FACTORY)
		return 0;

	if (trig != INIT_DONE_TRIGGER)
		return 0;

	chgctrl_vote(&chip->icl_boost, ICL_BOOST_VOTER_FACTORY,
			factory->icl);
	chgctrl_vote(&chip->fcc, FCC_VOTER_FACTORY,
			factory->fcc);
	chgctrl_vote(&chip->fastchg, FASTCHG_VOTER_FACTORY,
			factory->fastchg);
	return 0;
}

static int chgctrl_factory_parse_dt(struct chgctrl *chip,
				    struct device_node *np)
{
	struct chgctrl_factory *factory = &chip->factory;

	of_property_read_u32(np, "factory-icl", &factory->icl);
	of_property_read_u32(np, "factory-fcc", &factory->fcc);
	of_property_read_u32(np, "factory-fastchg", &factory->fastchg);

	return 0;
}

static void chgctrl_factory_init_default(struct chgctrl *chip)
{
	struct chgctrl_factory *factory = &chip->factory;

	factory->icl = 1500;
	factory->fcc = 500;
	factory->fastchg = 0;
}

struct chgctrl_feature chgctrl_feature_factory = {
	.name = "factory",
	.init_default = chgctrl_factory_init_default,
	.parse_dt = chgctrl_factory_parse_dt,
	.trigger = chgctrl_factory_trigger,
};
