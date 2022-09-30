#ifndef __CUSTOM_BATTERY_TABLE__
#define __CUSTOM_BATTERY_TABLE__

#define BAT_NTC_100 1
#define RBAT_PULL_UP_R             100000
#define RBAT_PULL_UP_VOLT          2800
#define BIF_NTC_R 16000

/* 68K NTC values from data sheet */
struct FUELGAUGE_TEMPERATURE Fg_Temperature_Table[21] = {
		{-20, 1142175},
		{-15, 841598},
		{-10, 626342},
		{-5, 470422},
		{0, 356454},
		{5, 272290},
		{10, 209632},
		{15, 162617},
		{20, 127071},
		{25, 100000},
		{30, 79232},
		{35, 63181},
		{40, 50687},
		{45, 40908},
		{50, 33199},
		{55, 27094},
		{60, 22221},
		{65, 18320},
		{70, 15175},
		{75, 12627},
		{80, 10557},
};
#endif
