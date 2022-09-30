#ifndef _CHARGER_CONTROLLER_H
#define _CHARGER_CONTROLLER_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>

#define CC_RW_PERM (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)
#define CC_RO_PERM (S_IRUSR | S_IRGRP | S_IROTH)
#define CC_WO_PERM (S_IWUSR | S_IWGRP)

#define TIME_TO_FULL_MODE

/* boot mode */
enum {
	CC_BOOT_MODE_NORMAL,
	CC_BOOT_MODE_CHARGER,
	CC_BOOT_MODE_FACTORY,
};

/* voter */
enum voter_type {
	CC_VOTER_TYPE_MIN,
	CC_VOTER_TYPE_MAX,
	CC_VOTER_TYPE_TRIGGER,
};

struct chgctrl_vote {
	char *name;
	int type;
	struct mutex lock;

	/* vote information */
	char **voters;
	int *values;
	int size;

	/* active */
	int active_voter;
	int active_value;

	/* handler */
	struct work_struct handler;
	void (*function)(void *);
	void *args;
};

int chgctrl_vote(struct chgctrl_vote* vote, int voter, int value);
void chgctrl_vote_dump(struct chgctrl_vote* vote);
int chgctrl_vote_active_value(struct chgctrl_vote* vote);
int chgctrl_vote_active_voter(struct chgctrl_vote* vote);
int chgctrl_vote_get_value(struct chgctrl_vote* vote, int voter);
int chgctrl_vote_init(struct chgctrl_vote* vote, struct device *dev, int value);

/* core */
enum {
	ICL_VOTER_DEFAULT,
	ICL_VOTER_USER,
	ICL_VOTER_CCD,
	ICL_VOTER_RESTRICTED,
	ICL_VOTER_GAME,
	ICL_VOTER_LLK,
	ICL_VOTER_PSEUDO_HVDCP,
	ICL_VOTER_MAX
};

enum {
	FCC_VOTER_DEFAULT,
	FCC_VOTER_USER,
	FCC_VOTER_CCD,
	FCC_VOTER_QNOVO,
	FCC_VOTER_OTP,
	FCC_VOTER_SPEC,
	FCC_VOTER_ACTM,
	FCC_VOTER_THERMAL,
	FCC_VOTER_DISPLAY,
	FCC_VOTER_RESTRICTED,
	FCC_VOTER_GAME,
	FCC_VOTER_BATTERY_ID,
	FCC_VOTER_LLK,
	FCC_VOTER_ATCMD,
	FCC_VOTER_FACTORY,
	FCC_VOTER_BCC,
	FCC_VOTER_MAX
};

enum {
	VFLOAT_VOTER_DEFAULT,
	VFLOAT_VOTER_USER,
	VFLOAT_VOTER_CCD,
	VFLOAT_VOTER_QNOVO,
	VFLOAT_VOTER_OTP,
	VFLOAT_VOTER_BATTERY_ID,
	VFLOAT_VOTER_MAX
};

enum {
	ICL_BOOST_VOTER_USER,
	ICL_BOOST_VOTER_PSEUDO_BATTERY,
	ICL_BOOST_VOTER_USB_CURRENT_MAX,
	ICL_BOOST_VOTER_ATCMD,
	ICL_BOOST_VOTER_FACTORY,
	ICL_BOOST_VOTER_MAX
};

enum {
	INPUT_SUSPEND_VOTER_USER,
	INPUT_SUSPEND_VOTER_WATER_DETECT,
	INPUT_SUSPEND_VOTER_CHARGER_OV,
	INPUT_SUSPEND_VOTER_MAX
};

enum {
	FASTCHG_VOTER_DEFAULT,
	FASTCHG_VOTER_USER,
	FASTCHG_VOTER_CCD,
	FASTCHG_VOTER_CAMERA,
	FASTCHG_VOTER_NETWORK,
	FASTCHG_VOTER_FACTORY,
	FASTCHG_VOTER_MAX
};

enum {
	WLESS_PWR_VOTER_DEFAULT,
	WLESS_PWR_VOTER_USER,
	WLESS_PWR_VOTER_CCD,
	WLESS_PWR_VOTER_ACTM,
	WLESS_PWR_VOTER_MAX
};

enum {
	INIT_DONE_TRIGGER,
	BATTERY_TRIGGER,
	CHARGER_TRIGGER,
	FB_TRIGGER,
	LLK_TRIGGER,
	BATTERY_ID_TRIGGER,
	BCC_TRIGGER,
};

/* otp */
struct chgctrl_otp {
	bool enabled;
	int version;
	struct work_struct work;
	int temp_state;
	int volt_state;
	int health;
	int fcc;
	int vfloat;
};

/* frame buffer */
struct chgctrl_fb {
	bool enabled;
	int fcc;
	struct wakeup_source ws;
	struct delayed_work dwork;
};

/* store demo */
struct chgctrl_llk {
	bool enabled;
	int store_demo_mode;
	struct work_struct work;
	int soc_max;
	int soc_min;
};

/* spec */
struct chgctrl_spec_data {
	/* temperature range */
	int tmin;
	int tmax;
	/* voltage range */
	int volt;
	/* charge limit */
	int curr;
};

struct chgctrl_spec {
	bool enabled;
	struct mutex lock;
	struct work_struct work;
	struct delayed_work vote_work;
	struct chgctrl_spec_data *data;
	int data_size;
	int idx;
	int vfloat;
	int step_fcc;
	int step_ms;
	int step_idx;
};

/* thermal */
struct chgctrl_thermal_trip {
	/* trigger temperature */
	int trigger;
	int offset;

	/* limit */
	int curr;
};

struct chgctrl_thermal {
	bool enabled;
	struct work_struct work;
	struct chgctrl_thermal_trip *trip;
	int trip_size;
	int idx;
};

/* game */
struct chgctrl_game {
	bool enabled;
	struct delayed_work dwork;
	int fcc;
	int icl;
	int light_fcc;
	int light_icl;
	int light_load;
	int light_sec;
	int lowbatt_fcc;
	int lowbatt_icl;
	int lowbatt_soc;
};

#ifdef TIME_TO_FULL_MODE
struct chgctrl_ttf_cc_step {
	s32 cur;
	s32 soc;
};
struct chgctrl_ttf_cv_slope {
	s32 cur;
	s32 soc;
	s32 time;
};
#endif

/* actm */
enum {
	ACTM_MODE_DISABLE = -1,
	ACTM_MODE_THERMAL,
	ACTM_MODE_BALANCE,
	ACTM_MODE_CHARGE,
	ACTM_MODE_AUTO,
};

enum {
	ACTM_ZONE_COLD = -1,
	ACTM_ZONE_NORMAL,
	ACTM_ZONE_WARM,
	ACTM_ZONE_HOT,
	ACTM_ZONE_MAX,
};

#define ACTM_STEP_MAX 3
#define ACTM_CHGTYPE_MAX 7
#define ACTM_CHG_MAX 2

struct chgctrl_actm_step {
	int delta_t;
	int fcc;
	int epp_pwr;
	int bpp_pwr;
};

struct chgctrl_actm_all_zone {
	int temp;
	int pps_epp;
	int pep_bpp;
	int other;
	int fb;
};

struct chgctrl_actm_value {
	int temp;
	int fcc_pwr;
};

struct chgctrl_actm {
	bool enabled;
	int check;
	unsigned int wired_therm_sensor;
	unsigned int wireless_therm_sensor;

	int active_mode;

	struct chgctrl_actm_value all[ACTM_CHGTYPE_MAX][ACTM_STEP_MAX];
	struct chgctrl_actm_value fb[ACTM_CHG_MAX][ACTM_STEP_MAX];

	struct mutex lock;
	int mode;
	int active_zone;
	int zone_pre;
	int ref_fcc;
	int min_fcc;
	int max_fcc;
	int step_size;
	struct chgctrl_actm_step *step;

	bool profiling;
	bool profiling_done;
	struct delayed_work actm_task;

	int trig_temp;
	bool fb_state;
	bool fb_state_pre;
	// wait_queue_head_t wq;
	// struct task_struct *task;
	// struct wakeup_source ws;

	/* wired */
	int wired_mode;
	int wired_zone_size;
	struct chgctrl_actm_all_zone *wired;
	/* wless */
	int wless_mode;
	int wless_zone_size;
	struct chgctrl_actm_all_zone *wireless;
};

/* chargerlogo */
struct chgctrl_chargerlogo {
	int icl;
	int fcc;
};

/* factory cable */
struct chgctrl_factory {
	int icl;
	int fcc;
	int fastchg;
};

/* battery care charging */
struct chgctrl_bcc {
	int bcc_enabled;
	int bcc_current;
};

struct chgctrl_power_supply {
	const char *name;
	struct power_supply *psy;
};

struct chgctrl {
	struct device *dev;
	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;

	struct notifier_block psy_nb;
	struct notifier_block fb_nb;

	struct work_struct init_done_work;
	struct work_struct battery_work;
	struct work_struct charger_work;
	struct work_struct fb_work;
	struct work_struct battery_id_work;

	/* votes */
	struct chgctrl_vote icl;
	struct chgctrl_vote fcc;
	struct chgctrl_vote vfloat;
	struct chgctrl_vote icl_boost;
	struct chgctrl_vote input_suspend;
	struct chgctrl_vote fastchg;
	struct chgctrl_vote wless_pwr;

	/* internal data */
	struct chgctrl_power_supply *charger;
	int charger_online;

	int battery_present;
	int battery_status;
	int battery_health;
	int battery_capacity;
	int battery_voltage;
	int battery_current;
	int battery_temperature;

	bool display_on;

	int boot_mode;

	/* device-tree data*/
	int default_icl;
	int default_fcc;
	int default_vfloat;
	int default_fastchg;
	int default_wless_pwr;
	int technology;

	/* batt_therm compensation */
	int batt_therm_comp;

	/* information */
	struct delayed_work info_work;
	struct wakeup_source info_ws;

	/* implementation */
	void *impl;

	/* otp */
	struct chgctrl_otp otp;

	/* frame buffer */
	struct chgctrl_fb fb;

	/* store demo */
	struct chgctrl_llk llk;

	/* spec */
	struct chgctrl_spec spec;

	/* restrict charging */
	struct chgctrl_vote restricted;

	/* thermal */
	struct chgctrl_thermal thermal;

	/* game */
	struct chgctrl_game game;

#ifdef TIME_TO_FULL_MODE
	/* Time to full report */
	bool time_to_full_mode;
	struct delayed_work time_to_full_work;
	struct chgctrl_ttf_cc_step *cc_data;
	struct chgctrl_ttf_cc_step *dynamic_cc_data;
	struct chgctrl_ttf_cv_slope *cv_data;
	unsigned int cc_data_length;
	unsigned int cv_data_length;

	unsigned int time_in_step_cc[5];

	int full_capacity;
	int sdp_current;
	int dcp_current;
	int pep_current;
	int direct_current;
	int non_std_current;
	int epp_current;
	int bpp_current;
	int report_ttf_comp;

	int sdp_comp;
	int dcp_comp;
	int cdp_comp;
	int pep_comp;
	int pd_comp;
	int direct_comp;
	int non_std_comp;
	int epp_comp;
	int bpp_comp;
	int min_comp;
	int batt_comp[5];

	int time_to_full_now; //ttf_now

	/* for evaluation */
	int runtime_consumed[101];
	int ttf_remained[101];
	int soc_now_ibat[101];

	long starttime_of_charging;
	long starttime_of_soc;
	int soc_begin;
	int soc_now;
#endif

	/* actm */
	struct chgctrl_actm actm;

	/* chargerlogo */
	struct chgctrl_chargerlogo chargerlogo;

	/* factory cable */
	struct chgctrl_factory factory;

	/* battery care charging */
	struct chgctrl_bcc bcc;

	/* ccd */
	int ccd_health;
	int ccd_status;
	int ccd_ttf;
};

/* common api */
struct chgctrl* chgctrl_get_drvdata(void);
static inline void chgctrl_changed(struct chgctrl* chip)
{
	if (chip->psy)
		power_supply_changed(chip->psy);
}

/* implementation api */
struct power_supply *chgctrl_get_charger_psy(struct chgctrl *chip);
enum power_supply_type chgctrl_get_charger_type(struct chgctrl *chip);
const char *chgctrl_get_charger_name(struct chgctrl *chip);
int chgctrl_get_charger_voltage_now(struct chgctrl *chip);
int chgctrl_get_charger_voltage_max(struct chgctrl *chip);
int chgctrl_get_charger_current_max(struct chgctrl *chip);

int chgctrl_get_gauge_current_now(struct chgctrl *chip);

struct power_supply *chgctrl_get_battery_psy(struct chgctrl *chip);
int chgctrl_get_battery_status(struct chgctrl *chip);
int chgctrl_get_battery_health(struct chgctrl *chip);
int chgctrl_get_battery_present(struct chgctrl *chip);
int chgctrl_get_battery_voltage_now(struct chgctrl *chip);
int chgctrl_get_battery_current_max(struct chgctrl *chip);
int chgctrl_get_battery_current_now(struct chgctrl *chip);
int chgctrl_get_battery_capacity(struct chgctrl *chip);
int chgctrl_get_battery_ttf_capacity(struct chgctrl *chip);
int chgctrl_get_battery_temperature(struct chgctrl *chip);

int chgctrl_get_boot_mode(void);

void chgctrl_icl_changed(void *args);
void chgctrl_fcc_changed(void *args);
void chgctrl_vfloat_changed(void *args);
void chgctrl_icl_boost_changed(void *args);
void chgctrl_input_suspend_changed(void *args);
void chgctrl_fastchg_changed(void *args);
void chgctrl_wless_pwr_changed(void *args);

int chgctrl_impl_init(struct chgctrl *chip);

/* feature */
struct chgctrl_feature {
	const char *name;
	void (*init_default)(struct chgctrl *chip);
	int (*parse_dt)(struct chgctrl *chip, struct device_node *np);
	int (*init)(struct chgctrl *chip);
	int (*exit)(struct chgctrl *chip);
	int (*trigger)(struct chgctrl *chip, int trig);
};

void chgctrl_feature_trigger(struct chgctrl *chip, int trig);
extern struct chgctrl_feature chgctrl_feature_otp;
extern struct chgctrl_feature chgctrl_feature_fb;
extern struct chgctrl_feature chgctrl_feature_spec;
extern struct chgctrl_feature chgctrl_feature_actm;
extern struct chgctrl_feature chgctrl_feature_thermal;
extern struct chgctrl_feature chgctrl_feature_restricted;
extern struct chgctrl_feature chgctrl_feature_game;
extern struct chgctrl_feature chgctrl_feature_chargerlogo;
#ifdef TIME_TO_FULL_MODE
extern struct chgctrl_feature chgctrl_feature_ttf;
#endif
extern struct chgctrl_feature chgctrl_feature_bcc;
extern struct chgctrl_feature chgctrl_feature_battery_id;
extern struct chgctrl_feature chgctrl_feature_llk;
extern struct chgctrl_feature chgctrl_feature_factory;
extern struct chgctrl_feature chgctrl_feature_info;

#endif
