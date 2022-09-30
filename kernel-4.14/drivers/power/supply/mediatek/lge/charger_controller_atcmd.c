#define pr_fmt(fmt) "[CC][ATCMD]%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include "charger_controller.h"

/*
 * AT%BATMP
 * - output : xx.x (ex 25.0)
 * - /proc/lge_power/testmode/temp
 */
static int param_get_batmp(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();

	if (!chip)
		return -ENODEV;

	/* update battery info */
	chgctrl_get_battery_temperature(chip);

	return scnprintf(buffer, PAGE_SIZE, "%d.%d",
			chip->battery_temperature / 10,
			chip->battery_temperature % 10);

}
static struct kernel_param_ops atcmd_batmp = {
	.get = param_get_batmp,
};
module_param_cb(atcmd_batmp, &atcmd_batmp, NULL, CC_RO_PERM);

/*
 * AT%CHARGE
 * - input : 1
 * - output : 0 or 1
 * - /proc/lge_power/testmode/charge
 */
static int param_get_charge(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int charge = 0;

	if (!chip)
		return -ENODEV;

	/* update battery info */
	chgctrl_get_battery_status(chip);

	if (chip->battery_status == POWER_SUPPLY_STATUS_CHARGING
			|| chip->battery_status == POWER_SUPPLY_STATUS_FULL)
		charge = 1;

	return scnprintf(buffer, PAGE_SIZE, "%d", charge);
}
static int param_set_charge(const char *val, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int charge = 0;
	int ret;

	if (!chip)
		return -ENODEV;

	ret = sscanf(val, "%d", &charge);
	if (ret <= 0)
		return -EINVAL;

	return chgctrl_vote(&chip->icl_boost, ICL_BOOST_VOTER_ATCMD,
			(charge == 1) ? 1500 : -1);
}
static struct kernel_param_ops atcmd_charge = {
	.get = param_get_charge,
	.set = param_set_charge,
};
module_param_cb(atcmd_charge, &atcmd_charge, NULL, CC_RW_PERM);

/*
 * AT%CHCOMP
 * - output : 0 or 1
 * - /proc/lge_power/testmode/chcomp
 */
static int param_get_chcomp(char *buffer, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int chcomp = 0;

	if (!chip)
		return -ENODEV;

	/* update battery info */
	chgctrl_get_battery_status(chip);
	chgctrl_get_battery_voltage_now(chip);

	if (chip->battery_status == POWER_SUPPLY_STATUS_FULL)
		chcomp = 1;

	if (chip->battery_status == POWER_SUPPLY_STATUS_CHARGING
			&& chip->battery_voltage >= 4250000)
		chcomp = 1;

	return scnprintf(buffer, PAGE_SIZE, "%d", chcomp);
}
static struct kernel_param_ops atcmd_chcomp = {
	.get = param_get_chcomp,
};
module_param_cb(atcmd_chcomp, &atcmd_chcomp, NULL, CC_RO_PERM);

/*
 * AT%CHARGINGMODEOFF
 * - input : 1
 * - /proc/lge_power/testmode/chgmodeoff
 */
static int param_set_chgmodeoff(const char *val, const struct kernel_param *kp)
{
	struct chgctrl *chip = chgctrl_get_drvdata();
	int chgmodeoff = 0;
	int ret;

	if (!chip)
		return -ENODEV;

	ret = sscanf(val, "%d", &chgmodeoff);
	if (ret <= 0)
		return 0;

	return chgctrl_vote(&chip->fcc, FCC_VOTER_ATCMD,
			(chgmodeoff == 1) ? 0 : -1);
}
static struct kernel_param_ops atcmd_chgmodeoff = {
	.set = param_set_chgmodeoff,
};
module_param_cb(atcmd_chgmodeoff, &atcmd_chgmodeoff, NULL, CC_WO_PERM);
