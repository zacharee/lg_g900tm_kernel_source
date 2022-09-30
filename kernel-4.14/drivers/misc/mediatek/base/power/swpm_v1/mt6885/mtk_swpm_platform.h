/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_SWPM_PLATFORM_H__
#define __MTK_SWPM_PLATFORM_H__

#include <mtk_gpu_swpm_plat.h>
#include <mtk_isp_swpm_plat.h>

#define SWPM_TEST (0)

#define MAX_RECORD_CNT				(64)
#define MAX_APHY_PWR				(11)
#define DEFAULT_LOG_INTERVAL_MS			(1000)
/* VPROC2 + VPROC1 + VDRAM + VGPU + VCORE */
#define DEFAULT_LOG_MASK			(0x1F)

#define POWER_INDEX_CHAR_SIZE			(4096)

#define NR_CPU_OPP				(16)
#define NR_CPU_CORE				(8)
#define NR_CPU_L_CORE				(4)

#define ALL_METER_TYPE				(0xFFFF)
#define EN_POWER_METER_ONLY			(0x1)
#define EN_POWER_METER_ALL			(0x3)

/* data shared w/ SSPM */
enum profile_point {
	MON_TIME,
	CALC_TIME,
	REC_TIME,
	TOTAL_TIME,

	NR_PROFILE_POINT
};

enum power_meter_type {
	CPU_POWER_METER,
	GPU_POWER_METER,
	CORE_POWER_METER,
	MEM_POWER_METER,
	ISP_POWER_METER,

	NR_POWER_METER
};

enum power_rail {
	VPROC2,
	VPROC1,
	VGPU,
	VCORE,
	VDRAM,
	VIO18_DDR,
	VIO18_DRAM,

	NR_POWER_RAIL
};

enum ddr_freq {
	DDR_400,
	DDR_600,
	DDR_800,
	DDR_933,
	DDR_1200,
	DDR_1600,
	DDR_1866,

	NR_DDR_FREQ
};

enum aphy_pwr_type {
	APHY_VCORE,
	APHY_VDDQ_0P6V,
	APHY_VM_0P75V,
	APHY_VIO_1P2V,
	APHY_VIO_1P8V,

	NR_APHY_PWR_TYPE
};

enum dram_pwr_type {
	DRAM_VDD1_1P8V,
	DRAM_VDD2_1P1V,
	DRAM_VDDQ_0P6V,

	NR_DRAM_PWR_TYPE
};

enum cpu_lkg_type {
	CPU_L_LKG,
	CPU_B_LKG,
	DSU_LKG,

	NR_CPU_LKG_TYPE
};

enum pmu_idx {
	PMU_IDX_L3DC,
	PMU_IDX_INST_SPEC,
	PMU_IDX_CYCLES,
	PMU_IDX_NON_WFX_COUNTER, /* put non-WFX counter here */

	MAX_PMU_CNT
};

enum cpu_core_power_state {
	CPU_CORE_ACTIVE,
	CPU_CORE_IDLE,
	CPU_CORE_POWER_OFF,

	NR_CPU_CORE_POWER_STATE
};

enum cpu_cluster_power_state {
	CPU_CLUSTER_ACTIVE,
	CPU_CLUSTER_IDLE,
	CPU_CLUSTER_POWER_OFF,

	NR_CPU_CLUSTER_POWER_STATE
};

enum mcusys_power_state {
	MCUSYS_ACTIVE,
	MCUSYS_IDLE,
	MCUSYS_POWER_OFF,

	NR_MCUSYS_POWER_STATE
};

/* TODO: cpu power index structure */
struct cpu_swpm_index {
	unsigned int core_state_ratio[NR_CPU_CORE_POWER_STATE][NR_CPU_CORE];
	unsigned int cpu_stall_ratio[NR_CPU_CORE];
	unsigned int cluster_state_ratio[NR_CPU_CLUSTER_POWER_STATE];
	unsigned int mcusys_state_ratio[NR_MCUSYS_POWER_STATE];
	unsigned int pmu_val[MAX_PMU_CNT][NR_CPU_CORE];
	unsigned int l3_bw;
	unsigned int cpu_emi_bw;
};

/* TODO: infra power state for core power */
enum infra_power_state {
	INFRA_DATA_ACTIVE,
	INFRA_CMD_ACTIVE,
	INFRA_IDLE,
	INFRA_DCM,

	NR_INFRA_POWER_STATE
};

/* sync with mt6885 emi in sspm */
#define MAX_EMI_NUM (2)
/* TODO: core power index structure */
struct core_swpm_index {
	unsigned int infra_state_ratio[NR_INFRA_POWER_STATE];
	unsigned int read_bw[MAX_EMI_NUM];
	unsigned int write_bw[MAX_EMI_NUM];
};

/* TODO: dram power index structure */
struct mem_swpm_index {
	unsigned int read_bw[MAX_EMI_NUM];
	unsigned int write_bw[MAX_EMI_NUM];
	unsigned int srr_pct;			/* self refresh rate */
	unsigned int pdir_pct[MAX_EMI_NUM];	/* power-down idle rate */
	unsigned int phr_pct[MAX_EMI_NUM];	/* page-hit rate */
	unsigned int acc_util[MAX_EMI_NUM];	/* accumulate EMI utilization */
	unsigned int mr4;
};

struct share_index {
	struct cpu_swpm_index cpu_idx;
	struct core_swpm_index core_idx;
	struct mem_swpm_index mem_idx;
	struct gpu_swpm_index gpu_idx;
	struct isp_swpm_index isp_idx;
	unsigned int window_cnt;
};

struct share_ctrl {
	unsigned int lock;
	unsigned int clear_flag;
};

struct share_wrap {
	unsigned int share_index_addr;
	unsigned int share_ctrl_addr;
};

struct aphy_bw_data {
	unsigned short bw[MAX_APHY_PWR];
};

struct aphy_pwr {
	unsigned short read_coef[MAX_APHY_PWR];
	unsigned short write_coef[MAX_APHY_PWR];
};

/* unit: uW / V^2 */
struct aphy_pwr_data {
	struct aphy_pwr pwr[NR_DDR_FREQ];
	unsigned short coef_idle[NR_DDR_FREQ];
};

/* unit: uA */
struct dram_pwr_conf {
	unsigned int i_dd0;
	unsigned int i_dd2p;
	unsigned int i_dd2n;
	unsigned int i_dd4r;
	unsigned int i_dd4w;
	unsigned int i_dd5;
	unsigned int i_dd6;
};

struct swpm_rec_data {
	/* 8 bytes */
	unsigned int cur_idx;
	unsigned int profile_enable;

	/* 2(short) * 7(ddr_opp) = 14 bytes */
	unsigned short ddr_opp_freq[NR_DDR_FREQ];

	/* 4(int) * 64(rec_cnt) * 9 = 2304 bytes */
	unsigned int pwr[NR_POWER_RAIL][MAX_RECORD_CNT];

	/* 8(long) * 5(prof_pt) * 3 = 120 bytes */
	unsigned long long avg_latency[NR_PROFILE_POINT];
	unsigned long long max_latency[NR_PROFILE_POINT];
	unsigned long long prof_cnt[NR_PROFILE_POINT];

	/* 2(short) * 11(sample point) * 7(opp_num) = 154 bytes */
	struct aphy_bw_data aphy_bw_tbl[NR_DDR_FREQ];

	/* 2(short) * 5(pwr_type) * 161(r/w_coef + idle) = 1610 bytes */
	struct aphy_pwr_data aphy_pwr_tbl[NR_APHY_PWR_TYPE];

	/* 4(int) * 3(pwr_type) * 7 = 84 bytes */
	struct dram_pwr_conf dram_conf[NR_DRAM_PWR_TYPE];

	/* 4(int) * 3(lkg_type) * 16 = 192 bytes */
	unsigned int cpu_lkg_pwr[NR_CPU_LKG_TYPE][NR_CPU_OPP];

	/* 4(int) * 15 = 60 bytes */
	unsigned int gpu_reserved[GPU_SWPM_RESERVED_SIZE];

	/* 4(int) * 256 = 1024 bytes */
	unsigned int isp_reserved[ISP_SWPM_RESERVED_SIZE];

	/* remaining size = 588 bytes */
};

extern struct swpm_rec_data *swpm_info_ref;

#ifdef CONFIG_MTK_CACHE_CONTROL
extern int ca_force_stop_set_in_kernel(int val);
#endif

#endif

