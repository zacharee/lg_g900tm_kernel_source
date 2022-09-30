#define pr_fmt(fmt) "[CC][OTP]%s: " fmt, __func__

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>

#include "charger_controller.h"

/* otp */
enum {
	OTP_VERSION_UNKNOWN = -1,
	OTP_VERSION_1_8,
	OTP_VERSION_1_8_SPR,
	OTP_VERSION_1_9,
	OTP_VERSION_2_0,
	OTP_VERSION_2_1,
};

enum {
	OTP_STATE_COLD,
	OTP_STATE_COOL,
	OTP_STATE_NORMAL,
	OTP_STATE_HIGH,
	OTP_STATE_OVERHEAT,
};

enum {
	OTP_STATE_VOLTAGE_LOW,
	OTP_STATE_VOLTAGE_HIGH,
};

static char *otp_versions[] = {
	"1.8",
	"1.8-sprint",
	"1.9",
	"2.0",
	"2.1",
};

static char *otp_states[] = {
	"cold",
	"cool",
	"normal",
	"high",
	"overheat",
};

struct temp_state {
	int state;
	int start;
	int release;
};

static int chgctrl_otp_get_temp(struct chgctrl_otp *otp)
{
	struct chgctrl *chip = container_of(otp, struct chgctrl, otp);

	return chgctrl_get_battery_temperature(chip);
}

static int chgctrl_otp_get_volt(struct chgctrl_otp *otp)
{
	struct chgctrl *chip = container_of(otp, struct chgctrl, otp);

	return chgctrl_get_battery_voltage_now(chip);
}

/* OTP Version 1.8 */
static int chgctrl_otp_v18_get_temp_state(struct chgctrl_otp *otp)
{
	const struct temp_state high[] = {
		{OTP_STATE_OVERHEAT,	550,	-30},
		{OTP_STATE_HIGH,	450,	-20},
	};
	const struct temp_state low[] = {
		{OTP_STATE_COLD,	-100,	50},
	};
	int state = otp->temp_state;
	int temp = chgctrl_otp_get_temp(otp);
	int i;

	/* check high temperature */
	for (i = 0; i < ARRAY_SIZE(high); i++) {
		if (temp > high[i].start)
			state = max(state, high[i].state);
		if (state != high[i].state)
			continue;
		if (temp < high[i].start + high[i].release)
			state--;
	}

	/* check low temperature */
	for (i = 0; i < ARRAY_SIZE(low); i++) {
		if (temp < low[i].start)
			state = min(state, low[i].state);
		if (state != low[i].state)
			continue;
		if (temp > low[i].start + low[i].release)
			state++;
	}

	/* otp version 1.8 does not support cool state */
	if (state == OTP_STATE_COOL)
		state = OTP_STATE_NORMAL;

	return state;
}

static int chgctrl_otp_v18_get_volt_state(struct chgctrl_otp *otp)
{
	int voltage = chgctrl_otp_get_volt(otp);

	voltage /= 1000;
	if (voltage > otp->vfloat)
		return OTP_STATE_VOLTAGE_HIGH;

	return OTP_STATE_VOLTAGE_LOW;
}

static bool chgctrl_otp_v18_get_state(struct chgctrl_otp *otp)
{
	int volt_state, temp_state;
	bool changed = false;

	temp_state = chgctrl_otp_v18_get_temp_state(otp);
	if (temp_state != otp->temp_state) {
		otp->temp_state = temp_state;

		changed = true;
	}

	volt_state = chgctrl_otp_v18_get_volt_state(otp);
	if (volt_state != otp->volt_state) {
		otp->volt_state = volt_state;

		/* need to check only in high state */
		if (temp_state == OTP_STATE_HIGH)
			changed = true;
	}

	return changed;
}

static void chgctrl_otp_v18(struct work_struct *work)
{
	struct chgctrl_otp *otp = container_of(work, struct chgctrl_otp, work);
	struct chgctrl *chip = container_of(otp, struct chgctrl, otp);
	int fcc = 0;

	if (!chgctrl_otp_v18_get_state(otp))
		return;

	pr_info("otp state changed to %s\n",
			otp_states[otp->temp_state]);

	/* set fcc refer to state */
	switch (otp->temp_state) {
	case OTP_STATE_COLD:
	case OTP_STATE_OVERHEAT:
		fcc = 0;
		break;
	case OTP_STATE_HIGH:
		fcc = 0;
		if (otp->volt_state == OTP_STATE_VOLTAGE_LOW)
			fcc = otp->fcc;
		break;
	case OTP_STATE_NORMAL:
	default:
		break;
	}

	chgctrl_vote(&chip->fcc, FCC_VOTER_OTP, fcc);
}

/* OTP Version 1.8 Sprint */
static int chgctrl_otp_v18_spr_get_temp_state(struct chgctrl_otp *otp)
{
	const struct temp_state high[] = {
		{OTP_STATE_OVERHEAT,	530,	-10},
		{OTP_STATE_HIGH,	450,	-10},
	};
	const struct temp_state low[] = {
		{OTP_STATE_COLD,	-50,	10},
	};
	int state = otp->temp_state;
	int temp = chgctrl_otp_get_temp(otp);
	int i;

	/* check high temperature */
	for (i = 0; i < ARRAY_SIZE(high); i++) {
		if (temp > high[i].start)
			state = max(state, high[i].state);
		if (state != high[i].state)
			continue;
		if (temp < high[i].start + high[i].release)
			state--;
	}

	/* check low temperature */
	for (i = 0; i < ARRAY_SIZE(low); i++) {
		if (temp < low[i].start)
			state = min(state, low[i].state);
		if (state != low[i].state)
			continue;
		if (temp > low[i].start + low[i].release)
			state++;
	}

	/* otp version 1.8 does not support cool state */
	if (state == OTP_STATE_COOL)
		state = OTP_STATE_NORMAL;

	return state;
}

static int chgctrl_otp_v18_spr_get_volt_state(struct chgctrl_otp *otp)
{
	int voltage = chgctrl_otp_get_volt(otp);

	voltage /= 1000;
	if (voltage > otp->vfloat)
		return OTP_STATE_VOLTAGE_HIGH;

	return OTP_STATE_VOLTAGE_LOW;
}

static bool chgctrl_otp_v18_spr_get_state(struct chgctrl_otp *otp)
{
	int volt_state, temp_state;
	bool changed = false;

	temp_state = chgctrl_otp_v18_spr_get_temp_state(otp);
	if (temp_state != otp->temp_state) {
		otp->temp_state = temp_state;

		changed = true;
	}

	volt_state = chgctrl_otp_v18_spr_get_volt_state(otp);
	if (volt_state != otp->volt_state) {
		otp->volt_state = volt_state;

		/* need to check only in high state */
		if (temp_state == OTP_STATE_HIGH)
			changed = true;
	}

	return changed;
}

static void chgctrl_otp_v18_spr(struct work_struct *work)
{
	struct chgctrl_otp *otp = container_of(work, struct chgctrl_otp, work);
	struct chgctrl *chip = container_of(otp, struct chgctrl, otp);
	int fcc = -1;

	if (!chgctrl_otp_v18_spr_get_state(otp))
		return;

	pr_info("otp state changed to %s\n",
			otp_states[otp->temp_state]);

	/* set fcc refer to state */
	switch (otp->temp_state) {
	case OTP_STATE_COLD:
	case OTP_STATE_OVERHEAT:
		fcc = 0;
		break;
	case OTP_STATE_HIGH:
		fcc = 0;
		if (otp->volt_state == OTP_STATE_VOLTAGE_LOW)
			fcc = otp->fcc;
		break;
	case OTP_STATE_NORMAL:
	default:
		break;
	}

	chgctrl_vote(&chip->fcc, FCC_VOTER_OTP, fcc);
}

/* OTP Version 1.9 */
static int chgctrl_otp_v19_get_temp_state(struct chgctrl_otp *otp)
{
	const struct temp_state high[] = {
		{OTP_STATE_OVERHEAT,	550,	-30},
		{OTP_STATE_HIGH,	450,	-20},
	};
	const struct temp_state low[] = {
		{OTP_STATE_COLD,	0,	30},
		{OTP_STATE_COOL,	100,	20},
	};
	int state = otp->temp_state;
	int temp = chgctrl_otp_get_temp(otp);
	int i;

	/* check high temperature */
	for (i = 0; i < ARRAY_SIZE(high); i++) {
		if (temp > high[i].start)
			state = max(state, high[i].state);
		if (state != high[i].state)
			continue;
		if (temp < high[i].start + high[i].release)
			state--;
	}

	/* check low temperature */
	for (i = 0; i < ARRAY_SIZE(low); i++) {
		if (temp < low[i].start)
			state = min(state, low[i].state);
		if (state != low[i].state)
			continue;
		if (temp > low[i].start + low[i].release)
			state++;
	}

	return state;
}

static int chgctrl_otp_v19_get_volt_state(struct chgctrl_otp *otp)
{
	int voltage = chgctrl_otp_get_volt(otp);

	voltage /= 1000;
	if (voltage > otp->vfloat)
		return OTP_STATE_VOLTAGE_HIGH;

	return OTP_STATE_VOLTAGE_LOW;
}

static bool chgctrl_otp_v19_get_state(struct chgctrl_otp *otp)
{
	int volt_state, temp_state;
	bool changed = false;

	temp_state = chgctrl_otp_v19_get_temp_state(otp);
	if (temp_state != otp->temp_state) {
		otp->temp_state = temp_state;

		changed = true;
	}

	volt_state = chgctrl_otp_v19_get_volt_state(otp);
	if (volt_state != otp->volt_state) {
		otp->volt_state = volt_state;

		/* need to check only in high state */
		if (temp_state == OTP_STATE_HIGH)
			changed = true;
	}

	return changed;
}

static void chgctrl_otp_v19(struct work_struct *work)
{
	struct chgctrl_otp *otp = container_of(work, struct chgctrl_otp, work);
	struct chgctrl *chip = container_of(otp, struct chgctrl, otp);
	int fcc = -1;

	if (!chgctrl_otp_v19_get_state(otp))
		return;

	pr_info("otp state changed to %s\n",
			otp_states[otp->temp_state]);

	/* set fcc refer to state */
	switch (otp->temp_state) {
	case OTP_STATE_COLD:
	case OTP_STATE_OVERHEAT:
		fcc = 0;
		break;
	case OTP_STATE_HIGH:
		fcc = 0;
		if (otp->volt_state == OTP_STATE_VOLTAGE_LOW)
			fcc = otp->fcc;
		break;
	case OTP_STATE_COOL:
		fcc = otp->fcc;
		break;
	case OTP_STATE_NORMAL:
	default:
		break;
	}

	chgctrl_vote(&chip->fcc, FCC_VOTER_OTP, fcc);
}

/* OTP Version 2.0 */
static int chgctrl_otp_v20_get_temp_state(struct chgctrl_otp *otp)
{
	const struct temp_state high[] = {
		{OTP_STATE_OVERHEAT,	550,	-30},
		{OTP_STATE_HIGH,	450,	-20},
	};
	const struct temp_state low[] = {
		{OTP_STATE_COLD,	0,	30},
		{OTP_STATE_COOL,	100,	20},
	};
	int state = otp->temp_state;
	int temp = chgctrl_otp_get_temp(otp);
	int i;

	/* check high temperature */
	for (i = 0; i < ARRAY_SIZE(high); i++) {
		if (temp > high[i].start)
			state = max(state, high[i].state);
		if (state != high[i].state)
			continue;
		if (temp < high[i].start + high[i].release)
			state--;
	}

	/* check low temperature */
	for (i = 0; i < ARRAY_SIZE(low); i++) {
		if (temp < low[i].start)
			state = min(state, low[i].state);
		if (state != low[i].state)
			continue;
		if (temp > low[i].start + low[i].release)
			state++;
	}

	return state;
}

static int chgctrl_otp_v20_get_volt_state(struct chgctrl_otp *otp)
{
	int voltage = chgctrl_otp_get_volt(otp);

	voltage /= 1000;
	if (voltage > otp->vfloat)
		return OTP_STATE_VOLTAGE_HIGH;

	return OTP_STATE_VOLTAGE_LOW;
}

static bool chgctrl_otp_v20_get_state(struct chgctrl_otp *otp)
{
	int volt_state, temp_state;
	bool changed = false;

	temp_state = chgctrl_otp_v20_get_temp_state(otp);
	if (temp_state != otp->temp_state) {
		otp->temp_state = temp_state;

		changed = true;
	}

	volt_state = chgctrl_otp_v20_get_volt_state(otp);
	if (volt_state != otp->volt_state) {
		otp->volt_state = volt_state;

		/* need to check only in high state */
		if (temp_state == OTP_STATE_HIGH)
			changed = true;
	}

	return changed;
}

static void chgctrl_otp_v20(struct work_struct *work)
{
	struct chgctrl_otp *otp = container_of(work, struct chgctrl_otp, work);
	struct chgctrl *chip = container_of(otp, struct chgctrl, otp);
	int fcc = -1;
	int vfloat = -1;

	if (!chgctrl_otp_v20_get_state(otp))
		return;

	pr_info("otp state changed to %s\n",
			otp_states[otp->temp_state]);
	if (otp->temp_state == OTP_STATE_OVERHEAT)
		otp->health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (otp->temp_state == OTP_STATE_COLD)
		otp->health = POWER_SUPPLY_HEALTH_COLD;
	else
		otp->health = POWER_SUPPLY_HEALTH_GOOD;

	/* set fcc, vfloat refer to state */
	switch (otp->temp_state) {
	case OTP_STATE_COLD:
	case OTP_STATE_OVERHEAT:
		fcc = 0;
		break;
	case OTP_STATE_HIGH:
		fcc = 0;
		if (otp->volt_state == OTP_STATE_VOLTAGE_LOW) {
			fcc = otp->fcc;
			vfloat = otp->vfloat;
		}
		break;
	case OTP_STATE_COOL:
		fcc = otp->fcc;
		break;
	case OTP_STATE_NORMAL:
	default:
		break;
	}

	chgctrl_vote(&chip->fcc, FCC_VOTER_OTP, fcc);
	chgctrl_vote(&chip->vfloat, VFLOAT_VOTER_OTP, vfloat);
}

/* OTP Version 2.1 */
static int chgctrl_otp_v21_get_temp_state(struct chgctrl_otp *otp)
{
	const struct temp_state high[] = {
		{OTP_STATE_OVERHEAT,	550,	-30},
		{OTP_STATE_HIGH,	450,	-20},
	};
	const struct temp_state low[] = {
		{OTP_STATE_COLD,	0,	30},
		{OTP_STATE_COOL,	100,	20},
	};
	int state = otp->temp_state;
	int temp = chgctrl_otp_get_temp(otp);
	int i;

	/* check high temperature */
	for (i = 0; i < ARRAY_SIZE(high); i++) {
		if (temp > high[i].start)
			state = max(state, high[i].state);
		if (state != high[i].state)
			continue;
		if (temp < high[i].start + high[i].release)
			state--;
	}

	/* check low temperature */
	for (i = 0; i < ARRAY_SIZE(low); i++) {
		if (temp < low[i].start)
			state = min(state, low[i].state);
		if (state != low[i].state)
			continue;
		if (temp > low[i].start + low[i].release)
			state++;
	}

	return state;
}

static int chgctrl_otp_v21_get_volt_state(struct chgctrl_otp *otp)
{
	int voltage = chgctrl_otp_get_volt(otp);

	voltage /= 1000;
	if (voltage > otp->vfloat)
		return OTP_STATE_VOLTAGE_HIGH;

	return OTP_STATE_VOLTAGE_LOW;
}

static bool chgctrl_otp_v21_get_state(struct chgctrl_otp *otp)
{
	int volt_state, temp_state;
	bool changed = false;

	temp_state = chgctrl_otp_v21_get_temp_state(otp);
	if (temp_state != otp->temp_state) {
		otp->temp_state = temp_state;

		changed = true;
	}

	volt_state = chgctrl_otp_v21_get_volt_state(otp);
	if (volt_state != otp->volt_state) {
		otp->volt_state = volt_state;

		/* need to check only in high state */
		if (temp_state == OTP_STATE_HIGH)
			changed = true;
		if (temp_state == OTP_STATE_COOL)
			changed = true;
	}

	return changed;
}

static void chgctrl_otp_v21(struct work_struct *work)
{
	struct chgctrl_otp *otp = container_of(work, struct chgctrl_otp, work);
	struct chgctrl *chip = container_of(otp, struct chgctrl, otp);
	int fcc = -1;
	int vfloat = -1;

	if (!chgctrl_otp_v21_get_state(otp))
		return;

	pr_info("otp state changed to %s\n",
			otp_states[otp->temp_state]);
	if (otp->temp_state == OTP_STATE_OVERHEAT)
		otp->health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (otp->temp_state == OTP_STATE_COLD)
		otp->health = POWER_SUPPLY_HEALTH_COLD;
	else
		otp->health = POWER_SUPPLY_HEALTH_GOOD;

	/* set fcc, vfloat refer to state */
	switch (otp->temp_state) {
	case OTP_STATE_COLD:
	case OTP_STATE_OVERHEAT:
		fcc = 0;
		break;
	case OTP_STATE_HIGH:
		fcc = 0;
		if (otp->volt_state == OTP_STATE_VOLTAGE_LOW) {
			fcc = otp->fcc;
			vfloat = otp->vfloat;
		}
		break;
	case OTP_STATE_COOL:
		fcc = otp->fcc;
		if (otp->volt_state == OTP_STATE_VOLTAGE_HIGH)
			fcc = 500;
		break;
	case OTP_STATE_NORMAL:
	default:
		break;
	}

	chgctrl_vote(&chip->fcc, FCC_VOTER_OTP, fcc);
	chgctrl_vote(&chip->vfloat, VFLOAT_VOTER_OTP, vfloat);
}

static int chgctrl_otp_trigger(struct chgctrl *chip, int trig)
{
	struct chgctrl_otp *otp = &chip->otp;

	if (trig != BATTERY_TRIGGER)
		return 0;

	if (!otp->enabled)
		return 0;

	schedule_work(&otp->work);

	return 0;
}

static int chgctrl_otp_exit(struct chgctrl *chip)
{
	struct chgctrl_otp *otp = &chip->otp;

	if (!otp->enabled)
		return 0;

	otp->enabled = false;
	cancel_work(&otp->work);

	return 0;
}

static int chgctrl_otp_init(struct chgctrl *chip)
{
	struct chgctrl_otp *otp = &chip->otp;

	otp->health = POWER_SUPPLY_HEALTH_GOOD;
	otp->temp_state = OTP_STATE_NORMAL;
	otp->volt_state = OTP_STATE_VOLTAGE_LOW;

	switch(otp->version) {
	case OTP_VERSION_1_8:
		INIT_WORK(&otp->work, chgctrl_otp_v18);
		break;
	case OTP_VERSION_1_8_SPR:
		INIT_WORK(&otp->work, chgctrl_otp_v18_spr);
		break;
	case OTP_VERSION_1_9:
		INIT_WORK(&otp->work, chgctrl_otp_v19);
		break;
	case OTP_VERSION_2_0:
		INIT_WORK(&otp->work, chgctrl_otp_v20);
		break;
	case OTP_VERSION_2_1:
		INIT_WORK(&otp->work, chgctrl_otp_v21);
		break;
	default:
		return -EINVAL;
	}

	pr_info("otp: version %s\n", otp_versions[otp->version]);
	pr_info("otp: fcc=%dmA, vfloat=%dmV\n", otp->fcc, otp->vfloat);

	otp->enabled = true;

	return 0;
}

static int chgctrl_otp_parse_dt(struct chgctrl *chip, struct device_node *np)
{
	struct chgctrl_otp *otp = &chip->otp;
	const char *version;
	int i;
	int ret = 0;

	ret = of_property_read_u32(np, "otp-fcc", &otp->fcc);
	if (ret)
		otp->fcc = 1000;
	ret = of_property_read_u32(np, "otp-vfloat", &otp->vfloat);
	if (ret)
		otp->vfloat = 4000;

	ret = of_property_read_string(np, "otp-version", &version);
	if (!ret) {
		/* find otp version */
		for (i = 0; i < ARRAY_SIZE(otp_versions); i++) {
			if (!strcmp(version, otp_versions[i])) {
				otp->version = i;
				break;
			}
		}
	}

	return 0;
}

static void chgctrl_otp_init_default(struct chgctrl *chip)
{
	struct chgctrl_otp *otp = &chip->otp;

	otp->fcc = 1000;
	otp->vfloat = 4000;
	otp->version = OTP_VERSION_UNKNOWN;
}

struct chgctrl_feature chgctrl_feature_otp = {
	.name = "otp",
	.init_default = chgctrl_otp_init_default,
	.parse_dt = chgctrl_otp_parse_dt,
	.init = chgctrl_otp_init,
	.exit = chgctrl_otp_exit,
	.trigger = chgctrl_otp_trigger,
};
