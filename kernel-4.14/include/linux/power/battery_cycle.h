#ifndef __BATTERY_CYCLE_H__
#define __BATTERY_CYCLE_H__

int battery_cycle_get_count(void);
int battery_cycle_set_count(unsigned int cycle);

void battery_cycle_update(int soc);

void battery_cycle_set_battery_removed(void);
void battery_cycle_initial(void);

#ifdef CONFIG_LGE_PM_BATTERY_AGING_FACTOR
int battery_aging_get_capacity(void);
int battery_aging_get_factor(void);
int battery_aging_get_factor_ten_multiple(void);
int battery_aging_get_factor_tri_level(void);
void battery_aging_update_by_full(int capacity, int aging_factor, int temp);

void battery_aging_initial(void);
#endif

#endif
