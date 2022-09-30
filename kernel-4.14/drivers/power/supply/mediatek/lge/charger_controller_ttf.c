#define pr_fmt(fmt) "[CC][TTF]%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>

#include "charger_controller.h"

#ifdef TIME_TO_FULL_MODE
/* time to full(TTF) */
#define DISCHARGE			-1
#define FULL				-1
#define EMPTY				0
#define NOT_SUPPORT_STEP_CHARGING	1

#define TTF_START_MS	(8000)	 /* 8 sec */
#define TTF_MONITOR_MS	(10000)

static int ibat_set_max = 0;
static int ttf_monitor_time = TTF_START_MS;
static int pre_soc = 0;
static int batt_comp_value = 0;
static int ibat_now_for_profile = 0;

static int ttf_get_current(struct chgctrl *chip)
{
	const char *chgtype = chgctrl_get_charger_name(chip);
	int iusb_aicl = 0;
	int i;

	if (chip->bcc.bcc_enabled) {
		ibat_set_max = 0;
		pr_info("Set bcc_current (%d)\n", chip->bcc.bcc_current);
		return chip->bcc.bcc_current;
	}

	if (ibat_set_max)
		return ibat_set_max;

	if (!strcmp(chgtype, "WLC_15W")) {
		ibat_set_max = chip->epp_current;

		if (chip->epp_comp)
			ibat_set_max += (ibat_set_max * chip->epp_comp /100);
		pr_info("Set wlc_15w_current (%d)\n", ibat_set_max);
		return ibat_set_max;
	} else if (!strcmp(chgtype, "WLC_10W")) {
		ibat_set_max = chip->epp_current;

		if (chip->epp_comp)
			ibat_set_max += (ibat_set_max * chip->epp_comp /100);
		pr_info("Set wlc_10w_current (%d)\n", ibat_set_max);
		return ibat_set_max;
	} else if (!strcmp(chgtype, "WLC_9W")) {
		ibat_set_max = chip->epp_current;

		if (chip->epp_comp)
			ibat_set_max += (ibat_set_max * chip->epp_comp /100);
		pr_info("Set wlc_9w_current (%d)\n", ibat_set_max);
		return ibat_set_max;
	} else if (!strcmp(chgtype, "WLC_5W")) {
		ibat_set_max = chip->bpp_current;

		if (chip->bpp_comp)
			ibat_set_max += (ibat_set_max * chip->bpp_comp /100);
		pr_info("Set wlc_5w_current (%d)\n", ibat_set_max);
		return ibat_set_max;
	} else if (!strcmp(chgtype, "WLC")) {
		ibat_set_max = chip->bpp_current;

		if (chip->bpp_comp)
			ibat_set_max += (ibat_set_max * chip->bpp_comp /100);
		pr_info("Set No Define wlc_current (%d)\n", ibat_set_max);
		return ibat_set_max;
	}

	if (!strncmp(chgtype, "DIRECT", 6)) {
		ibat_set_max = chip->direct_current;
		if (chip->direct_comp)
			ibat_set_max += (ibat_set_max * chip->direct_comp /100);
		pr_info("Set direct_current (%d)\n", ibat_set_max);
	} else if (!strncmp(chgtype, "PE", 2)) {
		ibat_set_max = chip->pep_current;

		for (i = 0; i < chip->cc_data_length ;i++) {
			if (ibat_set_max >= chip->cc_data[i].cur)
				break;
		}
		i = i >= chip->cc_data_length ? chip->cc_data_length - 1 : i;

		ibat_set_max = chip->cc_data[i].cur;

		if (chip->pep_comp)
			ibat_set_max += (ibat_set_max * chip->pep_comp /100);
		pr_info("Set pep_current (%d)\n", ibat_set_max);
	} else if (!strcmp(chgtype, "USB_DCP")) {
		iusb_aicl = chgctrl_get_charger_current_max(chip);
		iusb_aicl /= 1000;
		iusb_aicl += 100; /* aicl error compensation */
		ibat_set_max = iusb_aicl;

		if (iusb_aicl > chip->dcp_current)
			ibat_set_max = chip->dcp_current;
		else
			ibat_set_max -= 100;
		if (chip->dcp_comp)
			ibat_set_max += (ibat_set_max * chip->dcp_comp /100);
		pr_info("Set dcp_current (%d)\n", ibat_set_max);
	} else if (!strcmp(chgtype, "USB")) {
		ibat_set_max = chip->sdp_current;
		if (chip->sdp_comp)
			ibat_set_max += (ibat_set_max * chip->sdp_comp /100);
		pr_info("Set sdp_current (%d)\n", ibat_set_max);
	} else if (!strcmp(chgtype, "NON_STD")) {
		iusb_aicl = chgctrl_get_charger_current_max(chip);
		iusb_aicl /= 1000;
		ibat_set_max = iusb_aicl;

		if (iusb_aicl > chip->non_std_current)
			ibat_set_max = chip->non_std_current;
		if (chip->non_std_comp)
			ibat_set_max += (ibat_set_max * chip->non_std_comp /100);
		pr_info("Set non_std_current (%d)\n", ibat_set_max);
	} else if (!strcmp(chgtype,"USB_PD")) {
		iusb_aicl = chgctrl_get_charger_current_max(chip);
		iusb_aicl /= 1000;
		ibat_set_max = iusb_aicl;

		if (chip->pd_comp)
			ibat_set_max += (ibat_set_max * chip->pd_comp /100);
		pr_info("Set pd_current (%d)\n", ibat_set_max);
	} else {
		iusb_aicl = chgctrl_get_charger_current_max(chip);
		iusb_aicl /= 1000;
		ibat_set_max = iusb_aicl;

		pr_info("Set No Defined dcp_current (%d)\n", ibat_set_max);
	}

	return ibat_set_max;
}

#define SOC_TO_MHA(x) ((x) * chip->full_capacity)
//#define MHA_TO_TIME(x, y) (((x) * 60 * 60) / y)
static int ttf_time_in_cc(struct chgctrl *chip, int start, int end, int cur)
{
	unsigned int time_in_cc = 0;
	unsigned int mha_to_charge;

	/* calculate time to cc charging */
	mha_to_charge = SOC_TO_MHA(end - start);
	mha_to_charge /= 1000;  /* three figures SOC */
	//time_in_cc = MHA_TO_TIME(mha_to_charge, cur);
	time_in_cc = (mha_to_charge * 60 * 60) / cur;
	pr_info("time_in_cc = %d, mha_to_charge = %d\n",
			time_in_cc, mha_to_charge);

	return time_in_cc;
}

static void ttf_calc_cc_step_time(struct chgctrl *chip, int end)
{
	unsigned int mha_to_charge = 0;
	unsigned int cc_length = chip->cc_data_length;
	int i = 0;

	/* cc time update to new cc step table */
	for (i = 1; i < cc_length; i++) {
		mha_to_charge = SOC_TO_MHA(chip->cc_data[i].soc - chip->cc_data[i-1].soc);
		mha_to_charge /= 1000;
		chip->time_in_step_cc[i-1] = (mha_to_charge * 60 * 60) / chip->cc_data[i-1].cur;
		pr_info("[Step]time_in_step_cc = %d, mha_to_charge = %d\n",
				chip->time_in_step_cc[i-1], mha_to_charge);
	}

	mha_to_charge = SOC_TO_MHA(end - chip->cc_data[cc_length-1].soc);
	mha_to_charge /= 1000;
	chip->time_in_step_cc[cc_length-1] = (mha_to_charge * 60 * 60) / chip->cc_data[cc_length-1].cur;
	pr_info("[Step]time_in_step_cc = %d, mha_to_charge = %d\n",
			chip->time_in_step_cc[cc_length-1], mha_to_charge);

	for (i = 0; i < cc_length; i++)
		pr_info("[Step]CC_Step_Time [%d] : [%d]\n", i, chip->time_in_step_cc[i]);
}

static int ttf_time_in_step_cc(struct chgctrl *chip, int start, int end, int cur)
{
	unsigned int time_in_cc = 0;
	unsigned int mha_to_charge = 0;
	unsigned int cc_length = chip->cc_data_length;
	int i = 0;;

	/* Calculate all of cc step time from Table when charging start once*/
	if (chip->time_in_step_cc[0] == EMPTY)
		ttf_calc_cc_step_time(chip, end);

	/* cc step section check for current soc */
	for (i = 1; i < cc_length ;i++) {
		if (start < (chip->cc_data[i].soc))
			break;
	}
	pr_info("[Step]cc_soc_index = %d\n", i);

	/* Calculate cc_time_soc */
	if (i < cc_length) {
		mha_to_charge = SOC_TO_MHA(chip->cc_data[i].soc - start);
		mha_to_charge /= 1000;
		time_in_cc = (mha_to_charge * 60 * 60) / chip->cc_data[i-1].cur;
		pr_info("[Step][1]time_in_cc = %d, mha_to_charge = %d, ibat = %d\n",
				time_in_cc, mha_to_charge, chip->cc_data[i-1].cur);
	}
	else {
		mha_to_charge = SOC_TO_MHA(end - start);
		mha_to_charge /= 1000;
		time_in_cc = (mha_to_charge * 60 * 60) / chip->cc_data[cc_length-1].cur;
		pr_info("[Step][2]time_in_cc = %d, mha_to_charge = %d, ibat = %d\n",
				time_in_cc, mha_to_charge, chip->cc_data[cc_length-1].cur);
	}
	pr_info("[Step]time_in_cc = %d\n", time_in_cc);
	/* Calculate total cc time */
	for ( ; i < cc_length; i++)
		time_in_cc = time_in_cc + chip->time_in_step_cc[i];

	pr_info("[Step]Total time_in_step_cc = %d\n", time_in_cc);

	return time_in_cc;
}

static int ttf_time_in_cv(struct chgctrl *chip, int start, int end, int i)
{
	unsigned int time_in_cv = 0;

	/* will not enter cv */
	if (end <= start)
		return chip->cv_data[i].time;

	for (i = 0; i < chip->cv_data_length ;i++) {
		if (end <= chip->cv_data[i].soc)
			break;
	}

	if (i >= chip->cv_data_length)
		return 0;

	pr_info("cv_data_index: %d\n", i);

	/* calculate remain time in cv (linearity) */
	time_in_cv = chip->cv_data[i-1].time - chip->cv_data[i].time;
	time_in_cv /= chip->cv_data[i].soc - chip->cv_data[i-1].soc;
	time_in_cv *= chip->cv_data[i].soc - end;
	time_in_cv += chip->cv_data[i].time;

	return time_in_cv;
}

static bool chgctrl_ttf_should_stop(struct chgctrl *chip)
{
	if (chip->battery_status == POWER_SUPPLY_STATUS_DISCHARGING) {
		pr_info("POWER_SUPPLY_STATUS_DISCHARGING\n");
		return true;
	}

	if (chip->battery_status == POWER_SUPPLY_STATUS_FULL) {
		pr_info("POWER_SUPPLY_STATUS_FULL\n");
		return true;
	}

	if (chgctrl_get_battery_ttf_capacity(chip) >= 1000) {
		pr_info("TTF capacity 1000\n");
		return true;
	}

	return false;
}

static int time_to_full_evaluate_work(struct chgctrl *chip, int soc);
static int chgctrl_ttf_stop(struct chgctrl *chip);
static void chgctrl_time_to_full(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct chgctrl *chip = container_of(dwork, struct chgctrl,
			time_to_full_work);
	unsigned int time_total = 0, time_in_cc = 0, time_in_cv = 0;
	int soc, soc_cv;
	int ibat;
	int i;

	if (chgctrl_ttf_should_stop(chip)) {
		chgctrl_ttf_stop(chip);
		return;
	}

	/* get soc */
	soc = chgctrl_get_battery_ttf_capacity(chip);
	/* get charging current in normal setting */
	ibat = ttf_get_current(chip);
	if (ibat <= 0)
		return;

	ibat_now_for_profile = chgctrl_get_battery_current_now(chip);
	ibat_now_for_profile /= 1000;
	ibat_now_for_profile *= -1;

	if ((chip->cc_data_length <= 0) || (chip->cc_data_length > 5))
		return;

	/* Determine cv soc */
	for (i = 0; i < chip->cv_data_length ;i++) {
		if (abs(ibat) >= chip->cv_data[i].cur)
			break;
	}
	i = i >= chip->cv_data_length ? chip->cv_data_length - 1 : i;
	soc_cv = chip->cv_data[i].soc;
	pr_info("before calculate = soc: %d, cv_soc: %d, ibat: %d, index: %d\n",
					soc, soc_cv, ibat, i);

	if (soc >= soc_cv)
		goto cv_time;

	if ((chip->cc_data_length) < 2)
		goto cc_time;

	if (i != 0) /*Now need to cc_step_time*/
		goto cc_time;

/* cc_step_time: */
	time_in_cc = ttf_time_in_step_cc(chip, soc, soc_cv, ibat);
	goto cv_time;

cc_time:
	time_in_cc = ttf_time_in_cc(chip, soc, soc_cv, ibat);

cv_time:
	time_in_cv = ttf_time_in_cv(chip, soc_cv, soc, i);

	time_total = time_in_cc + time_in_cv;

	if (batt_comp_value) /* batt id compensation */
		time_total += (time_total * batt_comp_value / 100);

	if (chip->report_ttf_comp) /* report ttf compensation */
		time_total += (time_total * chip->report_ttf_comp / 100);

	if (chip->min_comp) /* minite compensation */
		time_total += (chip->min_comp * 60);

	pr_info("BATTERY(%d.%d%%, %d(%d)mA) TIME(%d%%)(%dsec, %dsec till %d%% + %dsec)\n",
			soc / 10, soc % 10, ibat, ibat_now_for_profile,
			batt_comp_value,
			time_total, time_in_cc, soc_cv/10, time_in_cv);

	if (soc >= 996)
		time_total = FULL;

	chip->time_to_full_now = time_total;

	time_to_full_evaluate_work(chip, soc);

	/* TTF update */
	if (chip->time_to_full_now != DISCHARGE && ttf_monitor_time == TTF_START_MS) {
		ttf_monitor_time = TTF_MONITOR_MS;
		pr_info("update\n");
		chgctrl_changed(chip);
	}

	schedule_delayed_work(dwork, msecs_to_jiffies(ttf_monitor_time));
}

static int time_to_full_update_work_clear(struct chgctrl *chip)
{
	unsigned int cc_length = chip->cc_data_length;
	int i = 0;

	for (i = 0; i < cc_length ; i++) {
		chip->time_in_step_cc[i] = EMPTY;
	}
	chip->time_to_full_now = DISCHARGE;

	return 0;
}

/* for evaluate */
static int time_to_full_evaluate_work_clear(struct chgctrl *chip)
{
	int i;

	/* Unnecessary clear routine skip */
	if (chip->soc_now == EMPTY)
		return 0;

	for (i = 0; i <= 100 ; i++) {
		chip->runtime_consumed[i] = EMPTY;
		chip->ttf_remained[i] = EMPTY;
	}
	chip->starttime_of_charging = EMPTY;
	chip->starttime_of_soc = EMPTY;
	chip->soc_begin = EMPTY;
	chip->soc_now = EMPTY;

	return 0;
}

static int remains_by_ttf(struct chgctrl *chip, int soc)
{
	int begin_soc = chip->soc_begin;

	if (chip->ttf_remained[soc] == EMPTY) {
		if (begin_soc != EMPTY && begin_soc <= soc)
			chip->ttf_remained[soc] = chip->time_to_full_now;
	}
	return chip->ttf_remained[soc];
}

static int time_to_full_evaluate_report(struct chgctrl *chip, long eoc)
{
	int i, begin_soc = chip->soc_begin;
	int really_remained[100+1] = { 0 };

	/* Do not need evaluation */
	if (begin_soc >= 100)
		return 0;

	really_remained[100] = 0;
	for (i = 99; begin_soc <= i; --i) {
		really_remained[i] = chip->runtime_consumed[i]
				+ really_remained[i+1];
	}

//	pr_info("[Evaluate]Evaluating... charging from %d(%ld) to 100(%ld), (duration %ld)\n",
//		 begin_soc, chip->starttime_of_charging, eoc, eoc-chip->starttime_of_charging);
	pr_info("[Evaluate]soc, consumed"	/* really measured */
			", real, ttf"		/* for comparison */
			", diff, IBAT\n");	/* ttf really diff to min */
	for (i = begin_soc; i <= 100; ++i) {
		pr_info("[Evaluate]%d, %d, %d, %d, %d, %d\n", i,
			chip->runtime_consumed[i], really_remained[i],
			chip->ttf_remained[i],
			(chip->ttf_remained[i] - really_remained[i]) / 60,
			chip->soc_now_ibat[i]);
	}
	return 0;
}

static int time_to_full_evaluate_work(struct chgctrl *chip, int soc)
{
	int remains_ttf = EMPTY;
	int soc_now = (soc + 5) / 10;	/* round-up radix */
	int ibat;
	long now;
	struct timespec	tspec;

	if ((soc >= 1000) && (chip->starttime_of_charging == EMPTY))
		return 0;

	if (soc <= 0)
		return 0;
	if (soc_now <= 0)
		return 0;

	if (soc_now >= 100)
		soc_now = 100;

	if (chip->soc_now == soc_now)
		return 0;

	get_monotonic_boottime(&tspec);
	now = tspec.tv_sec;

	if (chip->starttime_of_charging == EMPTY) {
		// New insertion
		chip->soc_begin = soc_now;
		chip->starttime_of_charging = now;
	}

	/* Soc rasing up */
	chip->runtime_consumed[soc_now-1] = now - chip->starttime_of_soc;

	/* Update time me */
	chip->soc_now = soc_now;
	chip->starttime_of_soc = now;
	ibat = chgctrl_get_battery_current_now(chip);
	chip->soc_now_ibat[soc_now] = (ibat / 1000) * -1;

	remains_ttf = remains_by_ttf(chip, soc_now);

	if (soc >= 995) {
		/* Evaluate NOW! (at the 100% soc) */
		time_to_full_evaluate_report(chip, now);
		time_to_full_evaluate_work_clear(chip);
	}

	return 0;
}
/* for evaluate */

static int chgctrl_ttf_stop(struct chgctrl *chip)
{
	pr_info("stop\n");

	ibat_set_max = 0;
	ttf_monitor_time = TTF_START_MS;
	pre_soc = 0;

	time_to_full_update_work_clear(chip);
	time_to_full_evaluate_work_clear(chip);
	cancel_delayed_work(&chip->time_to_full_work);

	return 0;
}

static int chgctrl_ttf_start(struct chgctrl *chip)
{
	unsigned long delay = 0;
	int soc;

	if (!chip->charger_online)
		return 0;

	if (chip->battery_status == POWER_SUPPLY_STATUS_DISCHARGING)
		return 0;

	/* Prevent duplication trigger by psy_handle_battery */
	soc = chgctrl_get_battery_ttf_capacity(chip);
	if (soc < 0 || pre_soc == soc) {
		return 0;
	}
	pre_soc = soc;

	/* wait for settle input current by aicl */
	if (ttf_monitor_time == TTF_START_MS) {
		pr_info("wait %dms to start\n", ttf_monitor_time);
		delay = msecs_to_jiffies(ttf_monitor_time);
	}

	schedule_delayed_work(&chip->time_to_full_work, delay);

	return 0;
}

static void chgctrl_ttf_update_batt_comp(struct chgctrl *chip)
{
	struct power_supply *psy = power_supply_get_by_name("battery_id");
	union power_supply_propval val;
	int ret;

	if (!psy)
		return;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_MANUFACTURER,
			&val);
	if (ret) {
		pr_info("Get Battery ID failed\n");
		goto out;
	}
	if (!val.strval) {
		pr_info("No Battery ID\n");
		goto out;
	}

	if (!strncmp(val.strval, "LGC", 3))
		batt_comp_value = chip->batt_comp[0];
	else if (!strncmp(val.strval,"TOCAD", 5))
		batt_comp_value = chip->batt_comp[1];
	else if (!strncmp(val.strval, "ATL", 3))
		batt_comp_value = chip->batt_comp[2];
	else if (!strncmp(val.strval, "BYD", 3))
		batt_comp_value = chip->batt_comp[3];
	else if (!strncmp(val.strval, "LISHEN", 6))
		batt_comp_value = chip->batt_comp[4];

	pr_info("%s, batt_comp : %d\n", val.strval, batt_comp_value);

out:
	power_supply_put(psy);
}

static int chgctrl_ttf_trigger(struct chgctrl *chip, int trig)
{
	if (!chip->time_to_full_mode)
		return 0;

	if (trig == BATTERY_ID_TRIGGER)
		chgctrl_ttf_update_batt_comp(chip);

	if (trig == BATTERY_TRIGGER)
		chgctrl_ttf_start(chip);

	if (trig == BCC_TRIGGER) {
		chgctrl_ttf_stop(chip);
		chgctrl_ttf_start(chip);
	}

	if (trig == CHARGER_TRIGGER) {
		if (chip->charger_online)
			chgctrl_ttf_start(chip);
		else
			chgctrl_ttf_stop(chip);
	}

	return 0;
}

static int chgctrl_ttf_exit(struct chgctrl *chip)
{
	if (!chip->time_to_full_mode)
		return 0;

	chgctrl_ttf_stop(chip);

	return 0;
}

static int chgctrl_ttf_init(struct chgctrl *chip)
{
	time_to_full_update_work_clear(chip);
	time_to_full_evaluate_work_clear(chip);

	INIT_DELAYED_WORK(&chip->time_to_full_work, chgctrl_time_to_full);

	return 0;
}

static int chgctrl_ttf_parse_dt(struct chgctrl *chip, struct device_node *np)
{
	int len = 0;
	int rc = 0;
	int i;

	chip->time_to_full_mode = of_property_read_bool(np,
			"time_to_full_mode");
	if (!chip->time_to_full_mode)
		return 0;

	rc = of_property_read_u32(np, "battery_full_capacity",
			&chip->full_capacity);
	if (rc)
		goto err;

	of_property_read_u32(np, "sdp_current", &chip->sdp_current);
	of_property_read_u32(np, "dcp_current", &chip->dcp_current);
	of_property_read_u32(np, "pep_current", &chip->pep_current);
	of_property_read_u32(np, "direct_current", &chip->direct_current);
	of_property_read_u32(np, "non_std_current", &chip->non_std_current);
	of_property_read_u32(np, "epp_current", &chip->epp_current);
	of_property_read_u32(np, "bpp_current", &chip->bpp_current);
	of_property_read_u32(np, "report_ttf_comp", &chip->report_ttf_comp);
	of_property_read_u32(np, "sdp_comp", &chip->sdp_comp);
	of_property_read_u32(np, "dcp_comp", &chip->dcp_comp);
	of_property_read_u32(np, "cdp_comp", &chip->cdp_comp);
	of_property_read_u32(np, "pep_comp", &chip->pep_comp);
	of_property_read_u32(np, "pd_comp", &chip->pd_comp);
	of_property_read_u32(np, "direct_comp", &chip->direct_comp);
	of_property_read_u32(np, "non_std_comp", &chip->non_std_comp);
	of_property_read_u32(np, "epp_comp", &chip->epp_comp);
	of_property_read_u32(np, "bpp_comp", &chip->bpp_comp);
	of_property_read_u32(np, "min_comp", &chip->min_comp);

	/* Batt_id comp Table */
	rc = of_property_read_u32_array(np, "batt_comp", chip->batt_comp, 5);
	if (rc < 0)
		pr_info("failed to read batt_comp : %d\n", rc);

	/* CC Table */
	if (chip->cc_data)
		goto end_profile;

	if (!of_get_property(np, "cc_data", &len)) {
		pr_info("there is not cc_data\n");
		goto err;
	}
	chip->cc_data = kzalloc(len, GFP_KERNEL);
	if (!chip->cc_data) {
		pr_info("failed to alloc cc_data\n");
		goto err;
	}

	chip->cc_data_length = len / sizeof(struct chgctrl_ttf_cc_step);
	pr_info("cc_data_length : %d / %d \n", chip->cc_data_length, len);
	rc = of_property_read_u32_array(np, "cc_data",
				(u32 *)chip->cc_data, len/sizeof(u32));
	if (rc) {
		pr_info("failed to read cc_data : %d\n", rc);
		goto err;
	}

	/* CV Table */
	if (chip->cv_data)
		goto end_profile;

	if (!of_get_property(np, "cv_data", &len)) {
		pr_info("there is not cv_data\n");
		goto err;
	}
	chip->cv_data = kzalloc(len, GFP_KERNEL);
	if (!chip->cv_data) {
		pr_info("failed to alloc cv_data\n");
		goto err;
	}

	chip->cv_data_length = len / sizeof(struct chgctrl_ttf_cv_slope);
	pr_info("cv_data_length : %d / %d \n", chip->cv_data_length, len);
	rc = of_property_read_u32_array(np, "cv_data",
				(u32 *)chip->cv_data, len/sizeof(u32));
	if (rc) {
		pr_info("failed to read cv_data : %d\n", rc);
		goto err;
	}

	/* Debug */
	if (chip->cc_data) {
		pr_info("CC Table\n");
		pr_info("current, soc\n");
		for(i = 0; i < chip->cc_data_length; i++)
			pr_info("%d, %d\n", chip->cc_data[i].cur,
					chip->cc_data[i].soc);
	} else {
		pr_info("CC Table Read Fail\n");
		goto err;
	}

	if (chip->cv_data) {
		pr_info("CV Table\n");
		pr_info("current, soc, time\n");
		for(i = 0; i < chip->cv_data_length; i++)
			pr_info("%d, %d, %d\n", chip->cv_data[i].cur,
					chip->cv_data[i].soc,
					chip->cv_data[i].time);
	} else {
		pr_info("CV Table Read Fail\n");
		goto err;
	}

end_profile:
	pr_info("batt_comp Table\n");
	pr_info("LGC, TOCAD, ATL, BYD, LISHEN\n");
	pr_info("%d %d %d %d %d\n", chip->batt_comp[0], chip->batt_comp[1],
			chip->batt_comp[2], chip->batt_comp[3],
			chip->batt_comp[4]);

	pr_info("full_capacity: %d, report_ttf_comp: %d, min_comp: %d\n",
			chip->full_capacity, chip->report_ttf_comp, chip->min_comp);
	pr_info("comp: sdp %d, cdp %d, dcp %d, "
			"pep %d, pd %d, direct %d, "
			"epp %d, bpp %d\n",
			chip->sdp_comp, chip->cdp_comp, chip->dcp_comp,
			chip->pep_comp, chip->pd_comp, chip->direct_comp,
			chip->epp_comp, chip->bpp_comp);
	/* Debug */

	return 0;

err:
	if (chip->cc_data) {
		chip->cc_data_length = 0;
		devm_kfree(chip->dev, chip->cc_data);
		chip->cc_data = NULL;
	}

	if (chip->cv_data) {
		chip->cv_data_length = 0;
		devm_kfree(chip->dev, chip->cv_data);
		chip->cv_data = NULL;
	}

	return -EINVAL;
}

static void chgctrl_ttf_init_default(struct chgctrl *chip)
{
	ibat_set_max = 0;
	ttf_monitor_time = TTF_START_MS;
	pre_soc = 0;
	batt_comp_value = 0;
	ibat_now_for_profile = 0;

	chip->time_to_full_now = DISCHARGE;

	chip->full_capacity = 0;
	chip->sdp_current = 500;
	chip->dcp_current = 1200;
	chip->pep_current = 2200;
	chip->direct_current = 4000;
	chip->non_std_current = 500;
	chip->epp_current = 1000;
	chip->bpp_current = 800;
	chip->report_ttf_comp = 0;
	chip->sdp_comp = 0;
	chip->dcp_comp = 0;
	chip->cdp_comp = 0;
	chip->pep_comp = 0;
	chip->pd_comp = 0;
	chip->direct_comp = 0;
	chip->non_std_comp = 0;
	chip->min_comp = 0;
}

struct chgctrl_feature chgctrl_feature_ttf = {
	.name = "ttf",
	.init_default = chgctrl_ttf_init_default,
	.parse_dt = chgctrl_ttf_parse_dt,
	.init = chgctrl_ttf_init,
	.exit = chgctrl_ttf_exit,
	.trigger = chgctrl_ttf_trigger,
};
#endif
