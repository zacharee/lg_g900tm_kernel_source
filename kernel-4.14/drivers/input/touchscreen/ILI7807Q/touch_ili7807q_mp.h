/* touch_ili7807q_mp.c
 *
 * Copyright (C) 2015 LGE.
 *
 * Author: BSP-TOUCH@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef LGE_TOUCH_ILI7807Q_MP_H
#define LGE_TOUCH_ILI7807Q_MP_H

#define MP_PASS						1
#define MP_FAIL						-1

#define VALUE						0
#define RETRY_COUNT					3
#define INT_CHECK					0
#define POLL_CHECK					1
#define BENCHMARK					1
#define NODETYPE					1
#define TYPE_BENCHMARK				0
#define TYPE_NO_JUGE				1
#define TYPE_JUGE					2

#define CSV_FILE_SIZE				(1 * 1024 * 1024) /* 1 M Byte */
#define LOG_BUF_SIZE				(256)
#define TIME_STR_LEN				(64)
#define MAX_LOG_FILE_SIZE			(10 * 1024 * 1024) /* 10 M byte */
#define MAX_LOG_FILE_COUNT			(4)

#define PARSER_MAX_CFG_BUF			(512 * 3)
#define PARSER_MAX_KEY_NUM			(600 * 3)
#define PARSER_MAX_KEY_NAME_LEN		100
#define PARSER_MAX_KEY_VALUE_LEN	2000
#define MAX_SECTION_NUM				100
#define BENCHMARK_KEY_NAME			"benchmark_data"
#define NODE_TYPE_KEY_NAME			"node type"
#define INI_ERR_OUT_OF_LINE			-1

#define CSV_PATH					"/sdcard"
#define CSV_PASS_NAME				"touch_mp_pass"
#define CSV_FAIL_NAME				"touch_mp_fail"

#define CMD_MUTUAL_DAC			0x1
#define CMD_MUTUAL_BG			0x2
#define CMD_MUTUAL_SIGNAL		0x3
#define CMD_MUTUAL_NO_BK		0x5
#define CMD_MUTUAL_HAVE_BK		0x8
#define CMD_MUTUAL_BK_DAC		0x10
#define CMD_SELF_DAC			0xC
#define CMD_SELF_BG				0xF
#define CMD_SELF_SIGNAL			0xD
#define CMD_SELF_NO_BK			0xE
#define CMD_SELF_HAVE_BK		0xB
#define CMD_SELF_BK_DAC			0x11
#define CMD_KEY_DAC				0x14
#define CMD_KEY_BG				0x16
#define CMD_KEY_NO_BK			0x7
#define CMD_KEY_HAVE_BK			0x15
#define CMD_KEY_OPEN			0x12
#define CMD_KEY_SHORT			0x13
#define CMD_ST_DAC				0x1A
#define CMD_ST_BG				0x1C
#define CMD_ST_NO_BK			0x17
#define CMD_ST_HAVE_BK			0x1B
#define CMD_ST_OPEN				0x18
#define CMD_TX_SHORT			0x19
#define CMD_RX_SHORT			0x4
#define CMD_RX_OPEN				0x6
#define CMD_TX_RX_DELTA			0x1E
#define CMD_CM_DATA				0x9
#define CMD_CS_DATA				0xA
#define CMD_TRCRQ_PIN			0x20
#define CMD_RESX2_PIN			0x21
#define CMD_MUTUAL_INTEGRA_TIME	0x22
#define CMD_SELF_INTEGRA_TIME	0x23
#define CMD_KEY_INTERGRA_TIME	0x24
#define CMD_ST_INTERGRA_TIME	0x25
#define CMD_PEAK_TO_PEAK		0x1D
#define CMD_GET_TIMING_INFO		0x30
#define CMD_DOZE_P2P			0x32
#define CMD_DOZE_RAW			0x33
#define CMD_PIN_TEST			0x61

#define MP_DATA_PASS			0
#define MP_DATA_FAIL			-1
#define MP_DBG_MSG			0
#define MP_SHOW_LOG			0

#define MP_TEST_ITEM			17

#define ILI9881_CHIP				0x9881
#define ILI7807_CHIP				0x7807
#define ILI9881N_AA					0x98811700
#define ILI9881O_AA					0x98811800
#define ILI9882_CHIP				0x9882
#define ILI9883_CHIP				0x9883

#define Mathabs(x) ({					\
		long ret;				\
		if (sizeof(x) == sizeof(long)) {	\
		long __x = (x);				\
		ret = (__x < 0) ? -__x : __x;		\
		} else {				\
		int __x = (x);				\
		ret = (__x < 0) ? -__x : __x;		\
		}					\
		ret;					\
	})

#define DUMP(fmt, arg...)		\
	do {				\
		if(MP_SHOW_LOG)		\
		  TOUCH_I(fmt, ##arg);	\
	} while (0)

enum TP_IC_TYPE {
	ILI_A = 0x0A,
	ILI_B,
	ILI_C,
	ILI_D,
	ILI_E,
	ILI_F,
	ILI_G,
	ILI_H,
	ILI_I,
	ILI_J,
	ILI_K,
	ILI_L,
	ILI_M,
	ILI_N,
	ILI_O,
	ILI_P,
	ILI_Q,
	ILI_R,
	ILI_S,
	ILI_T,
	ILI_U,
	ILI_V,
	ILI_W,
	ILI_X,
	ILI_Y,
	ILI_Z,
};

enum MP_ERR_CODE {
	EMP_CMD = 100,
	EMP_PROTOCOL,
	EMP_FILE,
	EMP_INI,
	EMP_TIMING_INFO,
	EMP_INVAL,
	EMP_PARSE,
	EMP_NOMEM,
	EMP_GET_CDC,
	EMP_INT,
	EMP_CHECK_BUY,
	EMP_MODE,
	EMP_FW_PROC,
	EMP_FORMUL_NULL,
};

enum open_test_node_type {
	NO_COMPARE = 0x00,	/* Not A Area, No Compare  */
	AA_Area = 0x01,		/* AA Area, Compare using Charge_AA  */
	Border_Area = 0x02,	/* Border Area, Compare using Charge_Border  */
	Notch = 0x04,		/* Notch Area, Compare using Charge_Notch  */
	Round_Corner = 0x08,	/* Round Corner, No Compare */
	Skip_Micro = 0x10,	/* Skip_Micro, No Compare */
};

enum mp_test_catalog {
	MUTUAL_TEST = 0,
	OPEN_TEST,
	PEAK_TO_PEAK_TEST,
	SHORT_TEST
};

struct ini_file_data {
	char section_name[PARSER_MAX_KEY_NAME_LEN];
	char key_name[PARSER_MAX_KEY_NAME_LEN];
	char key_value[PARSER_MAX_KEY_VALUE_LEN];
	int section_len;
	int key_name_len;
	int key_val_len;
};

struct mp_test_P540_open {
	int32_t *tdf_700;
	int32_t *tdf_250;
	int32_t *tdf_200;
	int32_t *cbk_700;
	int32_t *cbk_250;
	int32_t *cbk_200;
	int32_t *charg_rate;
	int32_t *full_Open;
	int32_t *dac;
};

struct mp_test_open_c {
	int32_t *cap_dac;
	int32_t *cap_raw;
	int32_t *dcl_cap;
};

struct mp_test_items {
	/* The description must be the same as ini's section name */
	char *desp;
	char *result;
	int catalog;
	u8 cmd;
	u8 spec_option;
	u8 type_option;
	bool run;
	bool lcm;
	int max;
	int max_res;
	int item_result;
	int min;
	int min_res;
	int frame_count;
	int trimmed_mean;
	int lowest_percentage;
	int highest_percentage;
	int v_tdf_1;
	int v_tdf_2;
	int h_tdf_1;
	int h_tdf_2;
	int goldenmode;
	int32_t *result_buf;
	int32_t *buf;
	int32_t *max_buf;
	int32_t *min_buf;
	int32_t *bench_mark_max;
	int32_t *bench_mark_min;
	int32_t *node_type;
	int (*do_test)(int index);
};

struct core_mp_test_data {
	struct device *dev;
	u8 chip_type;
	u8 chip_ver;
	char ini_date[128];
	char ini_ver[64];
	bool retry;
	bool isLongV;
	bool lost_benchmark;
	u16 chip_id;
	int run_num;
	int run_index[MP_TEST_ITEM];
	int cdc_len;
	int xch_len;
	int ych_len;
	int frame_len;
	int tdf;
	int busy_cdc;
	int op_tvch;
	int op_tvcl;
	int op_gain;
	int cbk_step;
	int cint;

	/**************short*****/
	int short_tvch;
	int short_tvcl;
	int short_variation;
	int short_cint;
	int short_rinternal;

	int no_bk_shift;
	u32 chip_pid;
	u32 fw_ver;
	u32 protocol_ver;
	u32 core_ver;
};

int ili7807q_mp_register_sysfs(struct device *dev);

#endif
