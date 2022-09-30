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
#include "gps_dl_config.h"
#include "gps_dl_log.h"
#include "gps_dl_hw_api.h"
#include "gps_dsp_fsm.h"

#define GPS_DSP_REG_POLL_MAX (8)
const unsigned int c_gps_dsp_reg_list[GPS_DATA_LINK_NUM][GPS_DSP_REG_POLL_MAX] = {
	/* DSP HW tick H/L, BG tick H/L, TX_END/TX_RD, RX_END/RX_WR */
	{0x5028, 0x5029, 0x0100, 0x0101, 0x4882, 0x4883, 0x4886, 0x4887},
	{0x5014, 0x5015, 0x0100, 0x0101, 0x4882, 0x4883, 0x4886, 0x4887}
};

struct gps_each_dsp_reg_read_context {
	bool poll_ongoing;
	int poll_index;
	unsigned int poll_addr;
};

struct gps_each_dsp_reg_read_context g_gps_each_dsp_reg_read_context[GPS_DATA_LINK_NUM];


enum GDL_RET_STATUS gps_each_dsp_reg_read_request(
	enum gps_dl_link_id_enum link_id, unsigned int reg_addr)
{
	enum GDL_RET_STATUS ret;

	ASSERT_LINK_ID(link_id, GDL_FAIL_INVAL);
	ret = gps_dl_hw_mcub_dsp_read_request(link_id, reg_addr);

	if (ret == GDL_OKAY) {
		g_gps_each_dsp_reg_read_context[link_id].poll_addr = reg_addr;
		g_gps_each_dsp_reg_read_context[link_id].poll_ongoing = true;
	}

	return ret;
}

void gps_each_dsp_reg_gourp_read_next(enum gps_dl_link_id_enum link_id, bool restart)
{
	unsigned int reg_addr;
	enum GDL_RET_STATUS ret;
	int i;

	ASSERT_LINK_ID(link_id, GDL_VOIDF());
	if (restart)
		g_gps_each_dsp_reg_read_context[link_id].poll_index = 0;
	else {
		g_gps_each_dsp_reg_read_context[link_id].poll_index++;
		if (g_gps_each_dsp_reg_read_context[link_id].poll_index >= GPS_DSP_REG_POLL_MAX)
			return;
	}

	i = g_gps_each_dsp_reg_read_context[link_id].poll_index;
	reg_addr = c_gps_dsp_reg_list[link_id][i];
	ret = gps_each_dsp_reg_read_request(link_id, reg_addr);
	GDL_LOGXD(link_id, "i = %d, addr = 0x%04x, status = %s",
		i, reg_addr, gdl_ret_to_name(ret));
}

void gps_each_dsp_reg_read_ack(
	enum gps_dl_link_id_enum link_id, const struct gps_dl_hal_mcub_info *p_d2a)
{
	ASSERT_LINK_ID(link_id, GDL_VOIDF());

	GDL_LOGXD(link_id, "i = %d, addr = 0x%04x, val = 0x%04x",
		g_gps_each_dsp_reg_read_context[link_id].poll_index,
		g_gps_each_dsp_reg_read_context[link_id].poll_addr,
		p_d2a->dat0);

	g_gps_each_dsp_reg_read_context[link_id].poll_ongoing = false;
	gps_each_dsp_reg_gourp_read_next(link_id, false);
}

void gps_each_dsp_reg_gourp_read_start(enum gps_dl_link_id_enum link_id)
{
	unsigned int a2d_flag;
	struct gps_dl_hal_mcub_info d2a;

	ASSERT_LINK_ID(link_id, GDL_VOIDF());

	if (g_gps_each_dsp_reg_read_context[link_id].poll_ongoing) {
		GDL_LOGXW(link_id, "i = %d, addr = 0x%04x, seem busy, check it",
			g_gps_each_dsp_reg_read_context[link_id].poll_index,
			g_gps_each_dsp_reg_read_context[link_id].poll_addr);

		/* TODO: show hw status */
		a2d_flag = gps_dl_hw_get_mcub_a2d_flag(link_id);
		gps_dl_hw_get_mcub_info(link_id, &d2a);
		GDL_LOGXW(link_id, "a2d_flag = %d, d2a_flag = %d, d0 = 0x%04x, d1 = 0x%04x",
			a2d_flag, d2a.flag, d2a.dat0, d2a.dat1);

		if (a2d_flag & GPS_MCUB_A2DF_MASK_DSP_REG_READ_REQ ||
			d2a.flag & GPS_MCUB_D2AF_MASK_DSP_REG_READ_READY) {
			/* real busy, bypass */
			return;
		}
	}

	gps_each_dsp_reg_gourp_read_next(link_id, true);
}

void gps_each_dsp_reg_gourp_read_init(enum gps_dl_link_id_enum link_id)
{
	ASSERT_LINK_ID(link_id, GDL_VOIDF());

	memset(&g_gps_each_dsp_reg_read_context[link_id], 0,
		sizeof(g_gps_each_dsp_reg_read_context[link_id]));
}

