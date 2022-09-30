/*
 * Copyright(c) 2017, LG Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef LGE_ERR_DETECT_H
#define LGE_ERR_DETECT_H

#include "mtk_dsi.h"
#include "mtk_panel_ext.h"

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

void lge_panel_err_detect_init(struct mtk_dsi *dsi);
void lge_panel_err_detect_parse_dt(struct mtk_panel_ext *panel, struct device_node *of_node);
int lge_panel_err_detect_create_sysfs(struct mtk_dsi *dsi, struct class *class_panel);
void lge_panel_err_detect_remove(struct mtk_panel_ext *panel);
void lge_panel_err_detect_irq_control(struct mtk_panel_ext *panel, bool enable);
#endif
