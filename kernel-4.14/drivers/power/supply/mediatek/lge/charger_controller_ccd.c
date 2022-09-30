#define pr_fmt(fmt) "[CC][CCD]%s: " fmt, __func__

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/power_supply.h>

#include "charger_controller.h"

struct ccd_desc {
	const char *text;
	int value;
};

static int param_set_icl(const char *val, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int icl = -1;

	if (!chip)
		return -ENODEV;

	if (sscanf(val, "%d", &icl) < 1)
		return -EINVAL;

	if (icl == chgctrl_vote_get_value(&chip->icl, ICL_VOTER_CCD))
		return 0;

	chgctrl_vote(&chip->icl, ICL_VOTER_CCD, icl);

	return 0;
}

static int param_get_icl(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	if (!chip)
		return -ENODEV;

	return scnprintf(buffer, PAGE_SIZE, "%d",
			chgctrl_vote_get_value(&chip->icl, ICL_VOTER_CCD));
}

static struct kernel_param_ops icl_ops = {
	.set = param_set_icl,
	.get = param_get_icl,
};
module_param_cb(ccd_icl, &icl_ops, NULL, CC_RW_PERM);

static int param_set_fcc(const char *val, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int fcc = -1;

	if (!chip)
		return -ENODEV;

	if (sscanf(val, "%d", &fcc) < 1)
		return -EINVAL;

	if (fcc == chgctrl_vote_get_value(&chip->fcc, FCC_VOTER_CCD))
		return 0;

	chgctrl_vote(&chip->fcc, FCC_VOTER_CCD, fcc);

	return 0;
}

static int param_get_fcc(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	if (!chip)
		return -ENODEV;

	return scnprintf(buffer, PAGE_SIZE, "%d",
			chgctrl_vote_get_value(&chip->fcc, FCC_VOTER_CCD));
}

static struct kernel_param_ops fcc_ops = {
	.set = param_set_fcc,
	.get = param_get_fcc,
};
module_param_cb(ccd_fcc, &fcc_ops, NULL, CC_RW_PERM);

static int param_set_vfloat(const char *val, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int vfloat = -1;

	if (!chip)
		return -ENODEV;

	if (sscanf(val, "%d", &vfloat) < 1)
		return -EINVAL;

	if (vfloat == chgctrl_vote_get_value(&chip->vfloat, VFLOAT_VOTER_CCD))
		return 0;

	chgctrl_vote(&chip->vfloat, VFLOAT_VOTER_CCD, vfloat);

	return 0;
}

static int param_get_vfloat(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	if (!chip)
		return -ENODEV;

	return scnprintf(buffer, PAGE_SIZE, "%d",
			chgctrl_vote_get_value(&chip->vfloat,
				VFLOAT_VOTER_CCD));
}

static struct kernel_param_ops vfloat_ops = {
	.set = param_set_vfloat,
	.get = param_get_vfloat,
};
module_param_cb(ccd_vfloat, &vfloat_ops, NULL, CC_RW_PERM);

static int param_set_fastchg(const char *val, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int fastchg = 0;

	if (!chip)
		return -ENODEV;

	fastchg = chip->default_fastchg;

	if (sscanf(val, "%d", &fastchg) < 1)
		return -EINVAL;

	if (chgctrl_vote_get_value(&chip->fastchg, FASTCHG_VOTER_CCD)
			== fastchg)
		return 0;

	chgctrl_vote(&chip->fastchg, FASTCHG_VOTER_CCD, fastchg);

	return 0;
}

static int param_get_fastchg(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	if (!chip)
		return -ENODEV;

	return scnprintf(buffer, PAGE_SIZE, "%d",
			chgctrl_vote_get_value(&chip->fastchg,
				FASTCHG_VOTER_CCD));
}

static struct kernel_param_ops fastchg_ops = {
	.set = param_set_fastchg,
	.get = param_get_fastchg,
};
module_param_cb(ccd_fastchg, &fastchg_ops, NULL, CC_RW_PERM);

static int param_set_wless_pwr(const char *val, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int wless_pwr = 0;

	if (!chip)
		return -ENODEV;

	wless_pwr = chip->default_wless_pwr;

	if (sscanf(val, "%d", &wless_pwr) < 1)
		return -EINVAL;

	if (chgctrl_vote_get_value(&chip->wless_pwr, WLESS_PWR_VOTER_CCD)
			== wless_pwr)
		return 0;

	chgctrl_vote(&chip->wless_pwr, WLESS_PWR_VOTER_CCD, wless_pwr);

	return 0;
}

static int param_get_wless_pwr(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	if (!chip)
		return -ENODEV;

	return scnprintf(buffer, PAGE_SIZE, "%d",
			chgctrl_vote_get_value(&chip->wless_pwr,
				WLESS_PWR_VOTER_CCD));
}

static struct kernel_param_ops wless_pwr_ops = {
	.set = param_set_wless_pwr,
	.get = param_get_wless_pwr,
};
module_param_cb(ccd_wless_pwr, &wless_pwr_ops, NULL, CC_RW_PERM);

static struct ccd_desc health_desc[] = {
	{ .text = "Unknown", .value = POWER_SUPPLY_HEALTH_UNKNOWN },
	{ .text = "Good", .value = POWER_SUPPLY_HEALTH_GOOD },
	{ .text = "Overheat", .value = POWER_SUPPLY_HEALTH_OVERHEAT },
	{ .text = "Cold", .value = POWER_SUPPLY_HEALTH_COLD },
};

static struct ccd_desc *ccd_health = &health_desc[0];
static int param_set_health(const char *val, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	struct ccd_desc *health = NULL;
	char text[20];
	int i;

	if (!chip)
		return -ENODEV;

	if (sscanf(val, "%19s", text) < 1)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(health_desc); i++) {
		if (!strcmp(text, health_desc[i].text))
			health = &health_desc[i];
	}

	if (!health) {
		pr_info("failed to set ccd_health to %s\n", text);
		return 0;
	}

	if (ccd_health == health)
		return 0;

	pr_info("set ccd_health %s\n", health->text);
	ccd_health = health;
	chip->ccd_health = health->value;

	return 0;
}

static int param_get_health(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	if (!chip)
		return -ENODEV;

	return scnprintf(buffer, PAGE_SIZE, "%s", ccd_health->text);
}

static struct kernel_param_ops health_ops = {
	.set = param_set_health,
	.get = param_get_health,
};
module_param_cb(ccd_health, &health_ops, NULL, CC_RW_PERM);

static struct ccd_desc status_desc[] = {
	{ .text = "Unknown", .value = POWER_SUPPLY_STATUS_UNKNOWN },
	{ .text = "Full", .value = POWER_SUPPLY_STATUS_FULL },
};

static struct ccd_desc *ccd_status = &status_desc[0];
static int param_set_status(const char *val, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	struct ccd_desc *status = NULL;
	char text[20];
	int i;

	if (!chip)
		return -ENODEV;

	if (sscanf(val, "%19s", text) < 1)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(status_desc); i++) {
		if (!strcmp(text, status_desc[i].text))
			status = &status_desc[i];
	}

	if (!status) {
		pr_info("failed to set ccd_status to %s\n", text);
		return 0;
	}

	if (ccd_status == status)
		return 0;

	pr_info("set ccd_status %s\n", status->text);
	ccd_status = status;
	chip->ccd_status = status->value;

	return 0;
}

static int param_get_status(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	if (!chip)
		return -ENODEV;

	return scnprintf(buffer, PAGE_SIZE, "%s", ccd_status->text);
}

static struct kernel_param_ops status_ops = {
	.set = param_set_status,
	.get = param_get_status,
};
module_param_cb(ccd_status, &status_ops, NULL, CC_RW_PERM);

static int param_get_chgtype(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	if (!chip)
		return -ENODEV;

	return scnprintf(buffer, PAGE_SIZE, "%s",
			chgctrl_get_charger_name(chip));
}

static struct kernel_param_ops chgtype_ops = {
	.get = param_get_chgtype,
};
module_param_cb(ccd_chgtype, &chgtype_ops, NULL, CC_RO_PERM);

static int param_set_ttf(const char *val, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int hour = 0, min = 0, sec = 0;
	bool notify = false;
	int ttf;

	if (!chip)
		return -ENODEV;

	if (sscanf(val, "%d", &ttf) < 1)
		return -EINVAL;

	if (chip->ccd_ttf == ttf)
		return 0;

	if (ttf < 0) {
		pr_info("set ccd_ttf %d (none)\n", ttf);
		chip->ccd_ttf = ttf;
		return 0;
	}

	if (chip->ccd_ttf < 0)
		notify = true;

	hour = ttf / (60 * 60);
	min = ttf % (60 * 60) / 60;
	sec = ttf % 60;

	pr_info("set ccd_ttf %d (%dhour %02dmin %02dsec)\n",
			ttf, hour, min, sec);
	chip->ccd_ttf = ttf;

	if (notify) {
		pr_info("notify ccd_ttf\n");
		chgctrl_changed(chip);
	}

	return 0;
}

static int param_get_ttf(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	if (!chip)
		return -ENODEV;

	return scnprintf(buffer, PAGE_SIZE, "%d", chip->ccd_ttf);
}

static struct kernel_param_ops ttf_ops = {
	.set = param_set_ttf,
	.get = param_get_ttf,
};
module_param_cb(ccd_ttf, &ttf_ops, NULL, CC_RW_PERM);
