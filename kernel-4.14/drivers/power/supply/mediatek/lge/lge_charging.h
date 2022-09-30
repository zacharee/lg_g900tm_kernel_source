#ifndef _LGE_CHARGING_H
#define _LGE_CHARGING_H

#include <linux/power_supply.h>

/*****************************************************************************
 *  LGE Charging State
 ****************************************************************************/

struct lge_charging_alg_data {
	/* charging state. this must be a first item */
	int state;

	bool disable_charging;
	struct mutex lock;
	struct mutex slave;

	/* time */
	unsigned int total_charging_time;
	struct timespec charging_begin_time;
	struct timespec aicl_done_time;

	/* charger setting */
	int input_current_limit;
	int charging_current_limit;
	int constant_voltage;
	int input_voltage;

	/* status */
	int charging_current_tuned;
	bool recharging;

	/* power supply */
	struct power_supply *charger_psy;
	struct power_supply *battery_psy;

	/* policy */
	int ieoc;
	int aicl_interval;

	/* case charging */
	int normal_fcc;
	int chargerlogo_fcc;

	/* slave charger */
	int slave_trig_vbus;
	int slave_trig_ichg;
	int slave_ichg_percent;
	int slave_step_ieoc;
	int slave_step_ichg;
	int slave_vfloat_offset;

	/* divider charger */
	int dv2_min_icl;
};

int lge_charging_init(struct charger_manager *info);

bool hvdcp_is_connect(struct charger_manager *info);
void hvdcp_set_enable(struct charger_manager *info);
void hvdcp_set_disable(struct charger_manager *info);
void hvdcp_set_check_chr_type(struct charger_manager *info);
void hvdcp_select_current(struct charger_manager *info, int *ichg, int *aicr);
int hvdcp_algorithm(struct charger_manager *info);
void hvdcp_plug_in(struct charger_manager *info);
void hvdcp_plug_out(struct charger_manager *info);
int hvdcp_init(struct charger_manager *info);

#ifdef CONFIG_LGE_PM_WIRELESS_CHARGER
int wless_get_tx_power(struct charger_manager *info);
bool wless_is_fastchg(struct charger_manager *info);
bool wless_is_stable(struct charger_manager *info);
void wless_select_current(struct charger_manager *info, int *ichg, int *aicr);
int wless_algorithm(struct charger_manager *info);
void wless_plug_in(struct charger_manager *info);
void wless_plug_out(struct charger_manager *info);
int wless_init(struct charger_manager *info);
int wless_notifier_call(struct charger_manager *info, unsigned long event,
			void *data);
void wless_parse_dt(struct charger_manager *info, struct device_node *np);
#endif

#endif /* End of _LGE_CHARGING_H */
