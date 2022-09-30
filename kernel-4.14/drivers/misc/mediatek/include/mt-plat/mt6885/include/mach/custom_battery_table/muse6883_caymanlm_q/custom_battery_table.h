#ifndef __CUSTOM_BATTERY_TABLE__
#define __CUSTOM_BATTERY_TABLE__

#define BAT_NTC_100 1
#define RBAT_PULL_UP_R             100000
#define RBAT_PULL_UP_VOLT          2800
#define BIF_NTC_R 16000

/* 68K NTC values from data sheet */
struct FUELGAUGE_TEMPERATURE Fg_Temperature_Table[21] = {
		{-20, 1107967},
		{-15, 769826},
		{-10, 574512},
		{-5, 444740},
		{0, 337560},
		{5, 258266},
		{10, 199137},
		{15, 154710},
		{20, 121066},
		{25, 95401},
		{30, 75688},
		{35, 60426},
		{40, 49612},
		{45, 40063},
		{50, 32532},
		{55, 26561},
		{60, 22221},
		{65, 18320},
		{70, 15175},
		{75, 12627},
		{80, 10557},
};
#endif
