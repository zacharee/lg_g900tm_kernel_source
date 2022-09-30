/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <mt-plat/mtk_devinfo.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <helio-dvfsrc-opp.h>
#include <helio-dvfsrc-qos.h>

#ifdef CONFIG_MEDIATEK_DRAMC
#include <dramc.h>
#endif
#define SOC_CPE_DETECT

#define AGING_VALUE 12500
static int opp_min_bin_opp0;
static int opp_min_bin_opp3;
static int opp_min_bin_opp3_1;



#ifndef CONFIG_MEDIATEK_DRAMC
static int mtk_dramc_get_steps_freq(unsigned int step)
{
	pr_info("get dram steps_freq fail\n");
	return 4266;
}
#endif


void dvfsrc_opp_level_mapping(void)
{
	set_vcore_opp(VCORE_DVFS_OPP_0, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_1, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_2, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_3, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_4, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_5, VCORE_OPP_2);
	set_vcore_opp(VCORE_DVFS_OPP_6, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_7, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_8, VCORE_OPP_2);
	set_vcore_opp(VCORE_DVFS_OPP_9,  VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_10, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_11, VCORE_OPP_2);
	set_vcore_opp(VCORE_DVFS_OPP_12, VCORE_OPP_3);
	set_vcore_opp(VCORE_DVFS_OPP_13, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_14, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_15, VCORE_OPP_2);
	set_vcore_opp(VCORE_DVFS_OPP_16, VCORE_OPP_3);
	set_vcore_opp(VCORE_DVFS_OPP_17, VCORE_OPP_0);
	set_vcore_opp(VCORE_DVFS_OPP_18, VCORE_OPP_1);
	set_vcore_opp(VCORE_DVFS_OPP_19, VCORE_OPP_2);
	set_vcore_opp(VCORE_DVFS_OPP_20, VCORE_OPP_3);

	set_ddr_opp(VCORE_DVFS_OPP_0, DDR_OPP_0);
	set_ddr_opp(VCORE_DVFS_OPP_1, DDR_OPP_1);
	set_ddr_opp(VCORE_DVFS_OPP_2, DDR_OPP_1);
	set_ddr_opp(VCORE_DVFS_OPP_3, DDR_OPP_2);
	set_ddr_opp(VCORE_DVFS_OPP_4, DDR_OPP_2);
	set_ddr_opp(VCORE_DVFS_OPP_5, DDR_OPP_2);
	set_ddr_opp(VCORE_DVFS_OPP_6, DDR_OPP_3);
	set_ddr_opp(VCORE_DVFS_OPP_7, DDR_OPP_3);
	set_ddr_opp(VCORE_DVFS_OPP_8, DDR_OPP_3);
	set_ddr_opp(VCORE_DVFS_OPP_9,  DDR_OPP_4);
	set_ddr_opp(VCORE_DVFS_OPP_10, DDR_OPP_4);
	set_ddr_opp(VCORE_DVFS_OPP_11, DDR_OPP_4);
	set_ddr_opp(VCORE_DVFS_OPP_12, DDR_OPP_4);
	set_ddr_opp(VCORE_DVFS_OPP_13, DDR_OPP_5);
	set_ddr_opp(VCORE_DVFS_OPP_14, DDR_OPP_5);
	set_ddr_opp(VCORE_DVFS_OPP_15, DDR_OPP_5);
	set_ddr_opp(VCORE_DVFS_OPP_16, DDR_OPP_5);
	set_ddr_opp(VCORE_DVFS_OPP_17, DDR_OPP_6);
	set_ddr_opp(VCORE_DVFS_OPP_18, DDR_OPP_6);
	set_ddr_opp(VCORE_DVFS_OPP_19, DDR_OPP_6);
	set_ddr_opp(VCORE_DVFS_OPP_20, DDR_OPP_6);
}

void dvfsrc_opp_table_init(void)
{
	int i;
	int vcore_opp, ddr_opp;

	for (i = 0; i < VCORE_DVFS_OPP_NUM; i++) {
		vcore_opp = get_vcore_opp(i);
		ddr_opp = get_ddr_opp(i);

		if (vcore_opp == VCORE_OPP_UNREQ || ddr_opp == DDR_OPP_UNREQ) {
			set_opp_table(i, 0, 0);
			continue;
		}
		set_opp_table(i, get_vcore_uv_table(vcore_opp),
		mtk_dramc_get_steps_freq(ddr_opp) * 1000);
	}
}

static int get_vb_volt(int vcore_opp)
{
	int idx;
	int ret = 0;
	int ptpod = get_devinfo_with_index(209);

	switch (vcore_opp) {
	case VCORE_OPP_0:
		idx = (ptpod >> 4) & 0x7;
		if (idx >= opp_min_bin_opp0)
			ret = 1;
		break;
	case VCORE_OPP_3:
		idx = ptpod & 0xF;
		if (idx >= opp_min_bin_opp3_1)
			ret = 2;
		else if (idx >= opp_min_bin_opp3)
			ret = 1;
		break;
	default:
		break;
	}

	return ret * 25000;
}

static int __init dvfsrc_opp_init(void)
{
	struct device_node *dvfsrc_node = NULL;
	int vcore_opp_0_uv, vcore_opp_1_uv, vcore_opp_2_uv, vcore_opp_3_uv;
	int i;
	int is_vcore_ct = 0;
	int ct_test = 0;
	int is_vcore_aging = 0;
	int dvfs_v_mode = 0;

	set_pwrap_cmd(VCORE_OPP_0, 0);
	set_pwrap_cmd(VCORE_OPP_1, 1);
	set_pwrap_cmd(VCORE_OPP_2, 2);
	set_pwrap_cmd(VCORE_OPP_3, 3);

	vcore_opp_0_uv = 725000;
	vcore_opp_1_uv = 650000;
	vcore_opp_2_uv = 600000;
	vcore_opp_3_uv = 575000;

	dvfsrc_node =
		of_find_compatible_node(NULL, NULL, "mediatek,dvfsrc");

	/* For Doe */
	if (dvfsrc_node) {
		of_property_read_u32(dvfsrc_node, "vcore_ct",
			(u32 *) &is_vcore_ct);

		of_property_read_u32(dvfsrc_node, "vcore_ct_test",
			(u32 *) &ct_test);

		of_property_read_u32(dvfsrc_node, "vcore_aging",
			(u32 *) &is_vcore_aging);

		of_property_read_u32(dvfsrc_node, "dvfs_v_mode",
			(u32 *) &dvfs_v_mode);
	}

	if (is_vcore_ct) {
		if (ct_test) {
			opp_min_bin_opp0 = 2;
			opp_min_bin_opp3 = 2;
			opp_min_bin_opp3_1 = 5;
		} else {
			opp_min_bin_opp0 = 3;
			opp_min_bin_opp3 = 3;
			opp_min_bin_opp3_1 = 6;
		}
		vcore_opp_0_uv = vcore_opp_0_uv - get_vb_volt(VCORE_OPP_0);
		vcore_opp_3_uv = vcore_opp_3_uv - get_vb_volt(VCORE_OPP_3);
	}

	if (dvfs_v_mode == 3) {
		/* LV */
		if (vcore_opp_0_uv == 700000)
			vcore_opp_0_uv = 662500;
		else
			vcore_opp_0_uv = 687500;

		vcore_opp_1_uv = 612500;
		vcore_opp_2_uv = 568750;

		if (vcore_opp_3_uv == 525000)
			vcore_opp_3_uv = 493750;
		else if (vcore_opp_3_uv == 550000)
			vcore_opp_3_uv = 518750;
		else
			vcore_opp_3_uv = 543750;
	} else if (dvfs_v_mode == 1) {
		/* HV */
		if (vcore_opp_0_uv == 700000)
			vcore_opp_0_uv = 737500;
		else
			vcore_opp_0_uv = 762500;

		vcore_opp_1_uv = 687500;
		vcore_opp_2_uv = 631250;

		if (vcore_opp_3_uv == 525000)
			vcore_opp_3_uv = 556250;
		else if (vcore_opp_3_uv == 550000)
			vcore_opp_3_uv = 581250;
		else
			vcore_opp_3_uv = 606250;
	} else if (is_vcore_aging) {
		vcore_opp_0_uv -= AGING_VALUE;
		vcore_opp_1_uv -= AGING_VALUE;
		vcore_opp_2_uv -= AGING_VALUE;
		vcore_opp_3_uv -= AGING_VALUE;
	}


	pr_info("%s: CT=%d, AGING=%d, VMODE=%d\n",
		__func__,
		is_vcore_ct,
		is_vcore_aging,
		dvfs_v_mode);

	pr_info("%s: FINAL vcore_opp_uv: %d, %d, %d %d\n",
		__func__,
		vcore_opp_0_uv,
		vcore_opp_1_uv,
		vcore_opp_2_uv,
		vcore_opp_3_uv);

	set_vcore_uv_table(VCORE_OPP_0, vcore_opp_0_uv);
	set_vcore_uv_table(VCORE_OPP_1, vcore_opp_1_uv);
	set_vcore_uv_table(VCORE_OPP_2, vcore_opp_2_uv);
	set_vcore_uv_table(VCORE_OPP_3, vcore_opp_3_uv);

	for (i = 0; i < DDR_OPP_NUM; i++) {
		set_opp_ddr_freq(i,
			mtk_dramc_get_steps_freq(i) * 1000);
	}

	return 0;
}

device_initcall_sync(dvfsrc_opp_init)

