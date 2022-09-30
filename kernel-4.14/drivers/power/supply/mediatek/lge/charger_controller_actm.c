#define pr_fmt(fmt) "[CC][ACTM]%s: " fmt, __func__

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>

#include "charger_controller.h"

enum {
	ACTM_CHARGER_NONE = -1,
	ACTM_CHARGER_WIRED,
	ACTM_CHARGER_WLESS,
	ACTM_CHARGER_MAX,
};

enum {
	ACTM_WIRED_PPS = 0,
	ACTM_WIRED_PEP,
	ACTM_WIRED_DCP,
	ACTM_WIRED_USB,
	ACTM_WIRED_OTHER,
	ACTM_WLESS_EPP,
	ACTM_WLESS_BPP,
	ACTM_CONN_MAX,
};

enum {
	ACTM_POLICY_COOL_DOWN,
	ACTM_POLICY_FREEZING,
	ACTM_POLICY_CHARGING,
};

enum {
	ACTM_SENSOR_BATT = 0,
	ACTM_SENSOR_SKIN,
	ACTM_SENSOR_BOTH,
};

static const char *mode_str[] = {
	"Theraml",
	"Balance",
	"Charge",
	"Auto",
};

static const char *zone_str[] = {
	"Normal",
	"Warm",
	"Hot",
};

static const char *chg_type_str[] = {
	"wired",
	"wireless",
};

static const char *chg_name_str[] = {
	"PPS",
	"PEP",
	"DCP",
	"USB",
	"Other",
	"EPP",
	"BPP",
};

static const char *policy_str[] = {
	"Cool Down",
	"Freezing",
	"Charging",
};

static const char *sensor_str[] = {
	"Batt_therm",
	"Skin_VTS",
	"Both",
};

#define ACTM_UPDATE_MS	(10000)	/* 10 sec */
#define ACTM_NORMAL_MS	(60000)	/* 60 sec */
#define ACTM_WARM_MS	(30000)	/* 30 sec */
#define ACTM_HOT_MS	(20000)	/* 20 sec */

static int actm_monitor_time = ACTM_UPDATE_MS;

static int chgctrl_actm_get_temp(struct chgctrl_actm *actm)
{
	struct chgctrl *chip = container_of(actm, struct chgctrl, actm);

	return chgctrl_get_battery_temperature(chip);
}

static int chgctrl_actm_get_skin_vts_temp(struct chgctrl_actm *actm)
{
	struct thermal_zone_device *tz;
	int temp;
	int rc;

	tz = thermal_zone_get_zone_by_name("lgetsskin");
	rc = thermal_zone_get_temp(tz, &temp);
	if (rc)
		return 365;

	return temp;
}

static int chgctrl_actm_get_charger_type(struct chgctrl_actm *actm)
{
	struct chgctrl *chip = container_of(actm, struct chgctrl, actm);
	enum power_supply_type type = chgctrl_get_charger_type(chip);

	if (type == POWER_SUPPLY_TYPE_UNKNOWN)
		return ACTM_CHARGER_NONE;
	if (type == POWER_SUPPLY_TYPE_WIRELESS)
		return ACTM_CHARGER_WLESS;

	return ACTM_CHARGER_WIRED;
}

static int chgctrl_actm_get_charger_name(struct chgctrl_actm *actm)
{
	struct chgctrl *chip = container_of(actm, struct chgctrl, actm);
	const char *chgtype = chgctrl_get_charger_name(chip);

	if (!strncmp(chgtype, "DIRECT", 6))
		return ACTM_WIRED_PPS;
	if (!strncmp(chgtype, "PE", 2))
		return ACTM_WIRED_PEP;
	if (!strncmp(chgtype, "USB_PD", 6))
		return ACTM_WIRED_PEP;
	if (!strcmp(chgtype, "USB_DCP"))
		return ACTM_WIRED_DCP;
	if (!strcmp(chgtype, "USB"))
		return ACTM_WIRED_USB;

	if (!strcmp(chgtype, "WLC_15W"))
		return ACTM_WLESS_EPP;
	if (!strcmp(chgtype, "WLC_10W"))
		return ACTM_WLESS_EPP;
	if (!strcmp(chgtype, "WLC_9W"))
		return ACTM_WLESS_EPP;
	if (!strcmp(chgtype, "WLC_5W"))
		return ACTM_WLESS_BPP;
	if (!strcmp(chgtype, "WLC"))
		return ACTM_WLESS_BPP;

	return ACTM_WIRED_OTHER;
}

static int chgctrl_actm_reference_get_temp(struct chgctrl_actm *actm)
{
	int ref_temp = 0;
	int batt_therm_now = 0;
	int skin_vts_now = 0;
	int chg_type = chgctrl_actm_get_charger_type(actm);
	unsigned int select_sensor = 0;

	batt_therm_now = chgctrl_actm_get_temp(actm);
	skin_vts_now = chgctrl_actm_get_skin_vts_temp(actm);

	if (chg_type == ACTM_CHARGER_WIRED)
		select_sensor = actm->wired_therm_sensor;
	if (chg_type == ACTM_CHARGER_WLESS)
		select_sensor = actm->wireless_therm_sensor;

	switch (select_sensor){
		case ACTM_SENSOR_BATT:
			ref_temp = batt_therm_now;
			break;
		case ACTM_SENSOR_SKIN:
			ref_temp = skin_vts_now;
			break;
		case ACTM_SENSOR_BOTH:
			if (batt_therm_now >= skin_vts_now)
				ref_temp = batt_therm_now;
			else
				ref_temp = skin_vts_now;
			break;
		default:
			break;
	}

	pr_info("%s (Batt %d mdegC, skin_vts %d mdegC)\n",
			sensor_str[select_sensor], batt_therm_now, skin_vts_now);

	return ref_temp;
}

#define MAX_FCC_NORMAL 500
#define MAX_FCC_WARM_HOT 700
static void chgctrl_actm_find_min_max_fcc(struct chgctrl_actm *actm, int chg_name, int zone)
{
	int fcc_set = 0;
	int chg_type = chgctrl_actm_get_charger_type(actm);

	if (zone == ACTM_ZONE_NORMAL)
		fcc_set = MAX_FCC_NORMAL;
	if (zone > ACTM_ZONE_NORMAL)
		fcc_set = MAX_FCC_WARM_HOT;

	if (actm->fb_state)
		actm->min_fcc = actm->fb[chg_type][zone].fcc_pwr;
	else
		actm->min_fcc = actm->all[chg_name][zone].fcc_pwr;

	actm->max_fcc = actm->min_fcc + fcc_set;

	pr_info("[%s][LCD %s] fcc: min %d ~ max %d\n",
			 chg_name_str[chg_name], (actm->fb_state ? "on" : "off"),
			 actm->min_fcc, actm->max_fcc);
}

static int chgctrl_actm_get_fcc(struct chgctrl_actm *actm, int chg_type, int chg_name, int zone, int temp)
{
	struct chgctrl *chip = container_of(actm, struct chgctrl, actm);
	int fcc = -1, step = 0, sign = -1;
	int min_fcc = actm->min_fcc;
	int max_fcc = actm->max_fcc;
	int fcc_pre = actm->ref_fcc;
	int i;

	if (!actm->step)
		return -1;

	if (chg_type == ACTM_CHARGER_WLESS)
		goto set_fcc;

	sign = temp > 0 ? -1 : 1;
	temp = abs(temp);
	for (i = ACTM_STEP_MAX -1 ; i >= 0; i--) {
		if (temp > actm->step[i].delta_t) {
			step = actm->step[i].fcc;
			//pr_info("step: %d  actm->step[i].fcc: %d\n", step, actm->step[i].fcc);
			break;
		}
	}

	fcc = chgctrl_get_battery_current_max(chip);
	fcc /= 1000;
	// pr_info("fcc: %d (old %d) mA step: %d sign: %d\n",
	// 		fcc, fcc_pre, step, sign);

	if ((fcc > fcc_pre) && (fcc_pre > 0))
		fcc = fcc_pre;

	if (zone != actm->zone_pre) {
		if (zone < actm->zone_pre)
			fcc = max_fcc;
		step = 0;
	}
	if (actm->fb_state != actm->fb_state_pre)
		fcc = max_fcc;

	if (step)
		fcc += step * sign;

set_fcc:
	if (fcc < min_fcc)
		fcc = min_fcc;
	if (fcc > max_fcc)
		fcc = max_fcc;

	pr_info("Change: %s %s, Set_fcc_pwr: %d%s (%d)\n",
			(zone != actm->zone_pre ? "Zone" : ""),
			(actm->fb_state != actm->fb_state_pre ? "FB" : ""),
			fcc, (chg_type == ACTM_CHARGER_WLESS ? "mW" : "mA"), step * sign);

	actm->zone_pre = zone;
	actm->fb_state_pre = actm->fb_state;
	if (fcc_pre != fcc)
		actm->ref_fcc = fcc;

	return fcc;
}

static int chgctrl_actm_get_policy(struct chgctrl_actm *actm, int zone, int temp)
{
	if (temp < 0)
		return ACTM_POLICY_CHARGING;

	if (zone == ACTM_ZONE_HOT)
		return ACTM_POLICY_FREEZING;

	return ACTM_POLICY_COOL_DOWN;
}

static int chgctrl_actm_get_zone(struct chgctrl_actm *actm, int chg_name, int temp)
{
	int zone;
	int mode = actm->active_mode;

	for (zone = ACTM_ZONE_MAX - 1; zone > ACTM_ZONE_COLD; zone--) {
		if (temp >= actm->all[chg_name][zone].temp) {
			// pr_info("find(%d) temp %d, temp %d\n",
			// 		 zone, temp, actm->all[chg_name][zone].temp);
			break;
		}
	}

	/* fix charge mode on chargerlogo */
	if (mode == ACTM_MODE_CHARGE) {
		if (zone < ACTM_ZONE_HOT)
			zone = ACTM_ZONE_COLD;
	}

	return zone;
}

static int chgctrl_actm_get_active_mode(struct chgctrl_actm *actm)
{
	struct chgctrl *chip = container_of(actm, struct chgctrl, actm);
	int mode = ACTM_MODE_AUTO;
	int chg_type = 0;

	if (chip->boot_mode == CC_BOOT_MODE_CHARGER)
		mode = ACTM_MODE_CHARGE;

	chg_type = chgctrl_actm_get_charger_type(actm);

	if (chg_type == ACTM_CHARGER_WLESS)
		mode = ACTM_MODE_CHARGE;

	return mode;
}

static int chgctrl_actm_wait_zone(struct chgctrl_actm *actm, int zone)
{
	int interval_ms;

	switch (zone) {
		case ACTM_ZONE_COLD:
			interval_ms = ACTM_NORMAL_MS; /* 60 sec */
			break;
		case ACTM_ZONE_NORMAL:
			interval_ms = ACTM_NORMAL_MS; /* 60 sec */
			break;
		case ACTM_ZONE_WARM:
			interval_ms = ACTM_WARM_MS; /* 30 sec */
			break;
		case ACTM_ZONE_HOT:
			interval_ms = ACTM_HOT_MS; /* 20 sec */
			break;
		default:
			interval_ms = ACTM_NORMAL_MS; /* 60 sec */
			break;
	}
	return interval_ms;
}

static bool chgctrl_actm_should_stop(struct chgctrl_actm *actm, int chg_type)
{
	struct chgctrl *chip = container_of(actm, struct chgctrl, actm);
	unsigned int chg_name = 0;

	if (chg_type == ACTM_CHARGER_NONE) {
		pr_info("ACTM_CHARGER_NONE\n");
		return true;
	}

	// if (chip->battery_status == POWER_SUPPLY_STATUS_DISCHARGING) {
	// 	pr_info("POWER_SUPPLY_STATUS_DISCHARGING\n");
	// 	return true;
	// }

	if (chip->battery_status == POWER_SUPPLY_STATUS_FULL) {
		pr_info("POWER_SUPPLY_STATUS_FULL\n");
		return true;
	}

	chg_name = chgctrl_actm_get_charger_name(actm);
	if (chg_name == ACTM_WIRED_USB) {
		pr_info("USB plug in\n");
		return true;
	}

	return false;
}

static int chgctrl_actm_clear(struct chgctrl *chip);
static void chgctrl_actm_task(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct chgctrl_actm *actm = container_of(dwork, struct chgctrl_actm,
			actm_task);
	struct chgctrl *chip = container_of(actm, struct chgctrl, actm);

	//struct timespec time_trig, time_now, time;
	struct timespec time_now;
	int temp_now = 0, temp = 0;
	int zone = actm->active_zone;
	int policy;
	int chg_type = chgctrl_actm_get_charger_type(actm);
	unsigned int chg_name = 0;
	unsigned int zone_name = 0;
	int set_fcc = -1;


	if (chgctrl_actm_should_stop(actm, chg_type)) {
		chgctrl_actm_clear(chip);
		return;
	}

	chg_name = chgctrl_actm_get_charger_name(actm);
	if (actm->profiling) {
		pr_info("[Select] chg_name zone temp fcc_pwr\n");
		for (zone_name = 0; zone_name < ACTM_STEP_MAX; zone_name++) {
			pr_info("[Select] %s %s %d %4d\n",
				chg_name_str[chg_name], zone_str[zone_name],
				actm->all[chg_name][zone_name].temp, actm->all[chg_name][zone_name].fcc_pwr);
		}
		actm->profiling = false;
	}

	get_monotonic_boottime(&time_now);

	temp_now = chgctrl_actm_reference_get_temp(actm);
	/* initialize data at actm enable */
	if (actm->active_mode <= ACTM_MODE_DISABLE) {
		actm->trig_temp = temp_now;
//		time_trig = time_now;
		pr_info("Initialize trig_temp(%d)\n",
				actm->trig_temp);
	}

//	time = timespec_sub(time_now, time_trig);
	//pr_info("[Delta] time: %d sec (%d-%d)\n", time.tv_sec, time_now.tv_sec, time_trig.tv_sec);

	actm->active_mode = chgctrl_actm_get_active_mode(actm);

	zone = chgctrl_actm_get_zone(actm, chg_name, temp_now);
	if (zone <= ACTM_ZONE_COLD) {
		pr_info("Zone is Cold\n");
		chgctrl_vote(&chip->fcc, FCC_VOTER_ACTM, -1);
		chgctrl_vote(&chip->wless_pwr,
				WLESS_PWR_VOTER_ACTM, -1);
		goto end;
	}

	temp = temp_now - actm->trig_temp;
	if (zone == ACTM_ZONE_HOT)
		temp = temp_now - actm->all[chg_name][zone].temp;

	pr_info("[Delta] temp: %d mdegC(%d-%d) time: %d\n",
			temp, temp_now,
			(zone == ACTM_ZONE_HOT ? actm->all[chg_name][zone].temp : actm->trig_temp),
			time_now.tv_sec);

	policy = chgctrl_actm_get_policy(actm, zone, temp);

	chgctrl_actm_find_min_max_fcc(actm, chg_name, zone);
	set_fcc = chgctrl_actm_get_fcc(actm, chg_type, chg_name, zone, temp);

	if (chg_type == ACTM_CHARGER_WIRED)
		chgctrl_vote(&chip->fcc, FCC_VOTER_ACTM, set_fcc);
	if (chg_type == ACTM_CHARGER_WLESS)
		chgctrl_vote(&chip->wless_pwr, WLESS_PWR_VOTER_ACTM, set_fcc);

	pr_info("Mode[%s] Chg[%s] Zone[%s] Policy[%s]\n",
			mode_str[actm->active_mode], chg_name_str[chg_name],
			zone_str[zone], policy_str[policy]);

end:
	actm->trig_temp = temp_now;
	//time_trig = time_now;
	//pr_info("trig_temp %d\n", actm->trig_temp);

	actm->active_zone = zone;
	actm_monitor_time = chgctrl_actm_wait_zone(actm, zone);
	//pr_info("End, next checking after %d ms\n", actm_monitor_time);
	schedule_delayed_work(dwork, msecs_to_jiffies(actm_monitor_time));
}

static int chgctrl_actm_clear(struct chgctrl *chip)
{
	struct chgctrl_actm *actm = &chip->actm;

	pr_info("Clear\n");
	actm->profiling = true;
	actm->profiling_done = false;
	actm->zone_pre = -2;
	actm->active_zone = ACTM_ZONE_COLD;
	actm->active_mode = ACTM_MODE_DISABLE;
	actm->trig_temp = 0;
	actm->ref_fcc = 0;
	actm->min_fcc = 0;
	actm->max_fcc = 0;
	actm_monitor_time = ACTM_UPDATE_MS;

	return 0;
}

static int chgctrl_actm_stop(struct chgctrl *chip)
{
	struct chgctrl_actm *actm = &chip->actm;

	pr_info("Stop\n");
	cancel_delayed_work_sync(&actm->actm_task);

	chgctrl_actm_clear(chip);
	chgctrl_vote(&chip->fcc, FCC_VOTER_ACTM, -1);
	chgctrl_vote(&chip->wless_pwr, WLESS_PWR_VOTER_ACTM, -1);

	return 0;
}

static int chgctrl_actm_start(struct chgctrl *chip)
{
	struct chgctrl_actm *actm = &chip->actm;
	unsigned int chg_name = 0;
	unsigned long delay = 0;

	if (!chip->charger_online) {
		pr_info("No charger\n");
		return 0;
	}

	// if (chip->battery_status == POWER_SUPPLY_STATUS_DISCHARGING) {
	// 	pr_info("Discharging\n");
	// 	return 0;
	// }

	chg_name = chgctrl_actm_get_charger_name(actm);
	if (chg_name == ACTM_WIRED_USB) {
		pr_info("USB plug in\n");
		return 0;
	}

	/* wait for settle input current by aicl */
	if (actm_monitor_time == ACTM_UPDATE_MS) {
		pr_info("wait %dms to start\n", actm_monitor_time);
		delay = msecs_to_jiffies(actm_monitor_time);
	}

	schedule_delayed_work(&actm->actm_task, delay);
	return 0;
}


static int chgctrl_actm_trigger(struct chgctrl *chip, int trig)
{
	struct chgctrl_actm *actm = &chip->actm;

	if (!actm->enabled)
		return 0;

	if (trig == CHARGER_TRIGGER) {
		if (chip->charger_online)
			chgctrl_actm_start(chip);
		else
			chgctrl_actm_stop(chip);
	}

	if (trig == FB_TRIGGER) {
		if (chip->display_on) {
			actm->fb_state = true;
		} else {
			actm->fb_state = false;
		}
	}

	return 0;
}

static int chgctrl_actm_exit(struct chgctrl *chip)
{
	struct chgctrl_actm *actm = &chip->actm;

	if (!actm->enabled)
		return 0;

	actm->enabled = false;
	cancel_delayed_work(&actm->actm_task);

	return 0;
}

static int chgctrl_actm_init(struct chgctrl *chip)
{
	struct chgctrl_actm *actm = &chip->actm;
	// int i = 0;
	int chg_name = 0;
	int zone_name = 0;
	int chg_type = 0;

	actm->active_mode = ACTM_MODE_DISABLE;
	actm->active_zone = ACTM_ZONE_COLD;
	actm->trig_temp = 0;
	actm->ref_fcc = 0;
	actm->fb_state = false;
	actm->fb_state_pre = false;

	mutex_init(&actm->lock);

	INIT_DELAYED_WORK(&actm->actm_task, chgctrl_actm_task);

	actm->enabled = true;
	actm->profiling = true;
	actm->profiling_done = false;

	pr_info("actm_check %d\n", actm->check);
	pr_info("wired_therm_sensor %d\n", actm->wired_therm_sensor);
	pr_info("wireless_therm_sensor %d\n", actm->wireless_therm_sensor);

	// pr_info("[step] delta_temp wire_fcc bpp_pwr epp_pwr\n");
	// for (i = 0; i < actm->step_size; i++) {
	// 	pr_info("[step][%d] %d.%d degC %4dmA %4dmW %4dmW\n",
	// 		i, actm->step[i].delta_t / 10, actm->step[i].delta_t % 10,
	// 		actm->step[i].fcc,
	// 		actm->step[i].epp_pwr, actm->step[i].bpp_pwr);
	// }
	// pr_info("[wired_zone] temp min_fcc(pps pep other fb)\n");
	// for (i = 0; i < actm->wired_zone_size; i++) {
	// 	pr_info("[wired_zone][%d] %d.%d degC %4dmA %4dmA %4dmA %4dmA\n",
	// 		i, actm->wired[i].temp / 10, actm->wired[i].temp % 10,
	// 		actm->wired[i].pps_epp, actm->wired[i].pep_bpp,
	// 		actm->wired[i].other, actm->wired[i].fb);
	// }
	// pr_info("[wireless_zone] temp min_fcc(epp bpp other fb)\n");
	// for (i = 0; i < actm->wless_zone_size; i++) {
	// 	pr_info("[wireless_zone][%d] %d.%d degC %4dmA %4dmW %4dmW %4dmW\n",
	// 		i, actm->wireless[i].temp / 10, actm->wireless[i].temp % 10,
	// 		actm->wireless[i].pps_epp, actm->wireless[i].pep_bpp,
	// 		actm->wireless[i].other, actm->wireless[i].fb);
	// }

	pr_info("[all_zone] chg_name zone temp fcc_pwr\n");
	for (chg_name = 0; chg_name < ACTM_CONN_MAX; chg_name++) {
		for (zone_name = 0; zone_name < ACTM_STEP_MAX; zone_name++) {
			pr_info("[all_zone] %s %s %d %4d\n",
				chg_name_str[chg_name], zone_str[zone_name],
				actm->all[chg_name][zone_name].temp, actm->all[chg_name][zone_name].fcc_pwr);
		}
	}

	pr_info("[fb] chg_type zone temp fcc_pwr\n");
	for (chg_type = 0; chg_type < ACTM_CHARGER_MAX; chg_type++) {
		for (zone_name = 0; zone_name < ACTM_STEP_MAX; zone_name++) {
			pr_info("[fb] %s %s %d %4d\n",
				chg_type_str[chg_type], zone_str[zone_name],
				actm->fb[chg_type][zone_name].temp, actm->fb[chg_type][zone_name].fcc_pwr);
		}
	}

	return 0;
}

static int chgctrl_actm_parse_dt(struct chgctrl *chip, struct device_node *np)
{
	struct chgctrl_actm *actm = &chip->actm;
	int size = 0;
	int ret = 0;

	pr_info("chgctrl_actm_parse_dt\n");
	of_property_read_u32(np, "actm_check", &actm->check);
	of_property_read_u32(np, "wired_therm_sensor", &actm->wired_therm_sensor);
	of_property_read_u32(np, "wireless_therm_sensor", &actm->wireless_therm_sensor);

	/* step parsing */
	if (!of_get_property(np, "step", &size)) {
		pr_info("there is not step\n");
		goto err;
	}
	actm->step = devm_kmalloc(chip->dev, size, GFP_KERNEL);
	if (!actm->step) {
		pr_info("failed to alloc step\n");
		goto err;
	}

	actm->step_size = size / sizeof(struct chgctrl_actm_step);
	pr_info("step_size : %d / %d \n", actm->step_size, size);
	ret = of_property_read_u32_array(np, "step",
				(u32 *)actm->step, size/sizeof(u32));
	if (ret) {
		pr_info("failed to read step : %d\n", ret);
		goto err;
	}

	// /* wired_zone parsing */
	// if (!of_get_property(np, "wired", &size)) {
	// 	pr_info("there is not wired_zone\n");
	// 	goto err;
	// }
	// actm->wired = devm_kmalloc(chip->dev, size, GFP_KERNEL);
	// if (!actm->wired) {
	// 	pr_info("failed to alloc wired_zone\n");
	// 	goto err;
	// }

	// actm->wired_zone_size = size / sizeof(struct chgctrl_actm_all_zone);
	// pr_info("wired_zone_size : %d / %d \n", actm->wired_zone_size, size);
	// ret = of_property_read_u32_array(np, "wired",
	// 			(u32 *)actm->wired, size/sizeof(u32));
	// if (ret) {
	// 	pr_info("failed to read wired_zone : %d\n", ret);
	// 	goto err;
	// }

	// /* wireless_zone parsing */
	// if (!of_get_property(np, "wireless", &size)) {
	// 	pr_info("there is not wireless_zone\n");
	// 	goto err;
	// }
	// actm->wireless = devm_kmalloc(chip->dev, size, GFP_KERNEL);
	// if (!actm->wireless) {
	// 	pr_info("failed to alloc wireless_zone\n");
	// 	goto err;
	// }

	// actm->wless_zone_size = size / sizeof(struct chgctrl_actm_all_zone);
	// pr_info("wless_zone_size : %d / %d \n", actm->wless_zone_size, size);
	// ret = of_property_read_u32_array(np, "wireless",
	// 			(u32 *)actm->wireless, size/sizeof(u32));
	// if (ret) {
	// 	pr_info("failed to read wireless_zone : %d\n", ret);
	// 	goto err;
	// }
	return 0;

err:
	if (actm->step) {
		actm->step_size = 0;
		devm_kfree(chip->dev, actm->step);
		actm->step = NULL;
	}

	return -EINVAL;
}

static void chgctrl_actm_init_default(struct chgctrl *chip)
{
	struct chgctrl_actm *actm = &chip->actm;

	actm->zone_pre = -2;

	actm->all[ACTM_WIRED_PPS][ACTM_ZONE_NORMAL].temp = 300;
	actm->all[ACTM_WIRED_PPS][ACTM_ZONE_NORMAL].fcc_pwr = 4000;
	actm->all[ACTM_WIRED_PPS][ACTM_ZONE_WARM].temp = 340;
	actm->all[ACTM_WIRED_PPS][ACTM_ZONE_WARM].fcc_pwr = 1500;
	actm->all[ACTM_WIRED_PPS][ACTM_ZONE_HOT].temp = 380;
	actm->all[ACTM_WIRED_PPS][ACTM_ZONE_HOT].fcc_pwr = 500;

	actm->all[ACTM_WIRED_PEP][ACTM_ZONE_NORMAL].temp = 300;
	actm->all[ACTM_WIRED_PEP][ACTM_ZONE_NORMAL].fcc_pwr = 2700;
	actm->all[ACTM_WIRED_PEP][ACTM_ZONE_WARM].temp = 340;
	actm->all[ACTM_WIRED_PEP][ACTM_ZONE_WARM].fcc_pwr = 1500;
	actm->all[ACTM_WIRED_PEP][ACTM_ZONE_HOT].temp = 380;
	actm->all[ACTM_WIRED_PEP][ACTM_ZONE_HOT].fcc_pwr = 500;

	actm->all[ACTM_WIRED_DCP][ACTM_ZONE_NORMAL].temp = 300;
	actm->all[ACTM_WIRED_DCP][ACTM_ZONE_NORMAL].fcc_pwr = 2000;
	actm->all[ACTM_WIRED_DCP][ACTM_ZONE_WARM].temp = 340;
	actm->all[ACTM_WIRED_DCP][ACTM_ZONE_WARM].fcc_pwr = 1500;
	actm->all[ACTM_WIRED_DCP][ACTM_ZONE_HOT].temp = 380;
	actm->all[ACTM_WIRED_DCP][ACTM_ZONE_HOT].fcc_pwr = 500;

	actm->all[ACTM_WIRED_USB][ACTM_ZONE_NORMAL].temp = 300;
	actm->all[ACTM_WIRED_USB][ACTM_ZONE_NORMAL].fcc_pwr = 500;
	actm->all[ACTM_WIRED_USB][ACTM_ZONE_WARM].temp = 340;
	actm->all[ACTM_WIRED_USB][ACTM_ZONE_WARM].fcc_pwr = 500;
	actm->all[ACTM_WIRED_USB][ACTM_ZONE_HOT].temp = 380;
	actm->all[ACTM_WIRED_USB][ACTM_ZONE_HOT].fcc_pwr = 500;

	actm->all[ACTM_WIRED_OTHER][ACTM_ZONE_NORMAL].temp = 300;
	actm->all[ACTM_WIRED_OTHER][ACTM_ZONE_NORMAL].fcc_pwr = 2000;
	actm->all[ACTM_WIRED_OTHER][ACTM_ZONE_WARM].temp = 340;
	actm->all[ACTM_WIRED_OTHER][ACTM_ZONE_WARM].fcc_pwr = 1500;
	actm->all[ACTM_WIRED_OTHER][ACTM_ZONE_HOT].temp = 380;
	actm->all[ACTM_WIRED_OTHER][ACTM_ZONE_HOT].fcc_pwr = 500;

	actm->all[ACTM_WLESS_EPP][ACTM_ZONE_NORMAL].temp = 300;
	actm->all[ACTM_WLESS_EPP][ACTM_ZONE_NORMAL].fcc_pwr = 9600;
	actm->all[ACTM_WLESS_EPP][ACTM_ZONE_WARM].temp = 340;
	actm->all[ACTM_WLESS_EPP][ACTM_ZONE_WARM].fcc_pwr = 9600;
	actm->all[ACTM_WLESS_EPP][ACTM_ZONE_HOT].temp = 380;
	actm->all[ACTM_WLESS_EPP][ACTM_ZONE_HOT].fcc_pwr = 4400;

	actm->all[ACTM_WLESS_BPP][ACTM_ZONE_NORMAL].temp = 300;
	actm->all[ACTM_WLESS_BPP][ACTM_ZONE_NORMAL].fcc_pwr = 3850;
	actm->all[ACTM_WLESS_BPP][ACTM_ZONE_WARM].temp = 340;
	actm->all[ACTM_WLESS_BPP][ACTM_ZONE_WARM].fcc_pwr = 3850;
	actm->all[ACTM_WLESS_BPP][ACTM_ZONE_HOT].temp = 380;
	actm->all[ACTM_WLESS_BPP][ACTM_ZONE_HOT].fcc_pwr = 3000;

	actm->fb[ACTM_CHARGER_WIRED][ACTM_ZONE_NORMAL].temp = 300;
	actm->fb[ACTM_CHARGER_WIRED][ACTM_ZONE_NORMAL].fcc_pwr = 1200;
	actm->fb[ACTM_CHARGER_WIRED][ACTM_ZONE_WARM].temp = 340;
	actm->fb[ACTM_CHARGER_WIRED][ACTM_ZONE_WARM].fcc_pwr = 500;
	actm->fb[ACTM_CHARGER_WIRED][ACTM_ZONE_HOT].temp = 380;
	actm->fb[ACTM_CHARGER_WIRED][ACTM_ZONE_HOT].fcc_pwr = 500;

	actm->fb[ACTM_CHARGER_WLESS][ACTM_ZONE_NORMAL].temp = 300;
	actm->fb[ACTM_CHARGER_WLESS][ACTM_ZONE_NORMAL].fcc_pwr = 4400;
	actm->fb[ACTM_CHARGER_WLESS][ACTM_ZONE_WARM].temp = 340;
	actm->fb[ACTM_CHARGER_WLESS][ACTM_ZONE_WARM].fcc_pwr = 3000;
	actm->fb[ACTM_CHARGER_WLESS][ACTM_ZONE_HOT].temp = 380;
	actm->fb[ACTM_CHARGER_WLESS][ACTM_ZONE_HOT].fcc_pwr = 3000;
}

struct chgctrl_feature chgctrl_feature_actm = {
	.name = "actm",
	.init_default = chgctrl_actm_init_default,
	.parse_dt = chgctrl_actm_parse_dt,
	.init = chgctrl_actm_init,
	.exit = chgctrl_actm_exit,
	.trigger = chgctrl_actm_trigger,
};
