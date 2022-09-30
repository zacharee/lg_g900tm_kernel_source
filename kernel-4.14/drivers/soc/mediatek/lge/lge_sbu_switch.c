/*
 * LGE_SBU_SWITCH Port Protection Switch driver
 *
 * Copyright (C) 2018 LG Electronics, Inc.
 * Author: Hansun Lee <hansun.lee@lge.com>
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
#define pr_fmt(fmt) "[SBU_SW] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/platform_device.h>

#include <tcpm.h>
#include <soc/mediatek/lge/lge_sbu_switch.h>
#ifdef CONFIG_LGE_USB
#include <linux/lge_ds3.h>
#endif

struct lge_sbu_switch {
	struct device *dev;

	struct mutex lock;
	enum lge_sbu_mode mode;
	bool enabled[LGE_SBU_MODE_DEFAULT];

	struct delayed_work tcpc_dwork;
	struct tcpc_device *tcpc;
	struct notifier_block tcpc_nb;
#ifdef CONFIG_LGE_USB_DEBUGGER
	struct delayed_work debug_uart_dwork;
#endif
};

static struct lge_sbu_switch *g_sbu_sw = NULL;

static const char *mode_name[] = {
	[LGE_SBU_MODE_DISABLE] = "disable",
	[LGE_SBU_MODE_USBID] = "usbid",
	[LGE_SBU_MODE_AUX] = "aux",
	[LGE_SBU_MODE_UART] = "uart",
	[LGE_SBU_MODE_DEFAULT] = "default",
};

static int sbu_sw_update(struct lge_sbu_switch *sbu_sw)
{
	struct pinctrl *p;
	int mode = LGE_SBU_MODE_DISABLE;

	for (; mode < LGE_SBU_MODE_DEFAULT; mode++) {
		if (!sbu_sw->enabled[mode])
			continue;
		if (!mode_name[mode])
			continue;

		p = devm_pinctrl_get_select(sbu_sw->dev, mode_name[mode]);
		if (!IS_ERR(p))
			break;
	}

	if (mode >= LGE_SBU_MODE_DEFAULT) {
		mode = LGE_SBU_MODE_DEFAULT;
		p = devm_pinctrl_get_select_default(sbu_sw->dev);
		if (IS_ERR(p))
			return -ENODEV;
	}

	pr_info("sbu mode set to %s\n", mode_name[mode]);
	sbu_sw->mode = mode;

	return 0;
}

static int sbu_sw_set(struct lge_sbu_switch *sbu_sw, enum lge_sbu_mode mode)
{
	int ret = 0;

	if (mode >= LGE_SBU_MODE_DEFAULT)
		return -EINVAL;
	if (mode < LGE_SBU_MODE_DISABLE)
		return -EINVAL;

	#if	0
	pr_info("mode enabled: %d %d %d\n", sbu_sw->enabled[LGE_SBU_MODE_USBID], \
									sbu_sw->enabled[LGE_SBU_MODE_AUX], \
									sbu_sw->enabled[LGE_SBU_MODE_UART]);
	pr_info("enable mode: %d\n", mode);
	#endif

	mutex_lock(&sbu_sw->lock);

	if (sbu_sw->enabled[mode]) {
		pr_info("%s is already set.\n", mode_name[mode]);
		goto out;
	}
	sbu_sw->enabled[mode] = true;

	if (sbu_sw->mode < mode)
		goto out;

	ret = sbu_sw_update(sbu_sw);

out:
	mutex_unlock(&sbu_sw->lock);

	return ret;
}

static int sbu_sw_clr(struct lge_sbu_switch *sbu_sw, enum lge_sbu_mode mode)
{
	int ret = 0;

	if (mode >= LGE_SBU_MODE_DEFAULT)
		return -EINVAL;
	if (mode < LGE_SBU_MODE_DISABLE)
		return -EINVAL;

	#if	0
	pr_info("mode enabled %d %d %d\n", sbu_sw->enabled[LGE_SBU_MODE_USBID], \
									sbu_sw->enabled[LGE_SBU_MODE_AUX], \
									sbu_sw->enabled[LGE_SBU_MODE_UART]);
	pr_info("disable mode: %d\n", mode);
	#endif

	mutex_lock(&sbu_sw->lock);

	if (!sbu_sw->enabled[mode])
		goto out;
	sbu_sw->enabled[mode] = false;

	if (sbu_sw->mode < mode)
		goto out;

	ret = sbu_sw_update(sbu_sw);

out:
	mutex_unlock(&sbu_sw->lock);

	return ret;
}

int lge_sbu_switch_enable(enum lge_sbu_mode mode, bool en)
{
	struct lge_sbu_switch *sbu_sw = g_sbu_sw;

	if (!sbu_sw) {
		pr_err("not ready\n");
		return -ENODEV;
	}

	return (en ? sbu_sw_set : sbu_sw_clr)(sbu_sw, mode);
}
EXPORT_SYMBOL(lge_sbu_switch_enable);
#ifdef CONFIG_LGE_USB_DEBUGGER
extern bool lge_get_uart_key_press_status(void);
extern void lge_uart_enable(int enable);
static bool lge_uart_changed = false;
static bool lge_uart_scheduled = false;
extern bool mt_get_uartlog_status(void);
#endif
static int sbu_sw_tcp_notifier_call(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct lge_sbu_switch *sbu_sw = container_of(nb,
			struct lge_sbu_switch, tcpc_nb);
	struct tcp_notify *noti = data;

	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.new_state == TYPEC_UNATTACHED) {
			if (is_ds_connected())	{
				pr_info("%s DS3 is connected - hold sbu_switch\n", __func__);
			} else {
				/* clear all mode on detach */
				sbu_sw_clr(sbu_sw, LGE_SBU_MODE_USBID);
				sbu_sw_clr(sbu_sw, LGE_SBU_MODE_AUX);
				sbu_sw_clr(sbu_sw, LGE_SBU_MODE_UART);
			}
#ifdef CONFIG_LGE_USB_DEBUGGER
			if(lge_uart_scheduled){
				lge_uart_scheduled = false;
				cancel_delayed_work_sync(&sbu_sw->debug_uart_dwork);
			}
			if(lge_uart_changed)	{
				lge_uart_changed = false;
				lge_uart_enable(0);
			}
#endif
		}
		#ifdef CONFIG_LGE_USB_DEBUGGER
		else if ( (noti->typec_state.new_state == TYPEC_ATTACHED_DEBUG)
#ifdef CONFIG_LGE_DUAL_SCREEN
			&& !is_ds_connected()
#endif
			&& lge_get_uart_key_press_status()) {
				lge_uart_scheduled = true;
				schedule_delayed_work(&sbu_sw->debug_uart_dwork, msecs_to_jiffies(2000));
				pr_info("%s debug_uart_dwork schedule_delayed_work\n",__func__);
		}
		#endif
		break;
	case TCP_NOTIFY_AMA_DP_STATE:
		if (!noti->ama_dp_state.active) {
			pr_err("sbu_sw_tcp_notifier_call - Not active!!!\n");
			return NOTIFY_OK;
		}

		pr_info("%s TCP_NOTIFY_AMA_DP_STATE signal: %x pin: %x polarity: %x \n", __func__,
			noti->ama_dp_state.active,
			noti->ama_dp_state.pin_assignment, noti->ama_dp_state.polarity);

		sbu_sw_set(sbu_sw, LGE_SBU_MODE_AUX);
		break;
	}

	return NOTIFY_OK;
}

static void sbu_sw_tcpc_work(struct work_struct *work)
{
	struct lge_sbu_switch *sbu_sw = container_of(to_delayed_work(work),
			struct lge_sbu_switch, tcpc_dwork);

	sbu_sw->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!sbu_sw->tcpc) {
		schedule_delayed_work(to_delayed_work(work),
				msecs_to_jiffies(1000));
		return;
	}

	sbu_sw->tcpc_nb.notifier_call = sbu_sw_tcp_notifier_call;
	register_tcp_dev_notifier(sbu_sw->tcpc, &sbu_sw->tcpc_nb,
			TCP_NOTIFY_TYPE_MODE|TCP_NOTIFY_TYPE_USB);
}

#ifdef CONFIG_LGE_USB_DEBUGGER
static void sbu_sw_debug_uart_work(struct work_struct *work)
{
	struct lge_sbu_switch *sbu_sw = container_of(to_delayed_work(work),
			struct lge_sbu_switch, debug_uart_dwork);

	pr_info("Run...\n");
	sbu_sw_set(sbu_sw, LGE_SBU_MODE_UART);
	if(!mt_get_uartlog_status()){
		lge_uart_enable(1);
		lge_uart_changed = true;
	}
}
#endif

static int sbu_sw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lge_sbu_switch *sbu_sw;
	struct pinctrl *p;

	sbu_sw = devm_kzalloc(dev, sizeof(*sbu_sw), GFP_KERNEL);
	if (!sbu_sw) {
		pr_err("out of memory\n");
		return -ENOMEM;
	}

	sbu_sw->dev = &pdev->dev;
	sbu_sw->mode = LGE_SBU_MODE_DEFAULT;
	mutex_init(&sbu_sw->lock);
	platform_set_drvdata(pdev, sbu_sw);

	INIT_DELAYED_WORK(&sbu_sw->tcpc_dwork, sbu_sw_tcpc_work);
#ifdef CONFIG_LGE_USB_DEBUGGER
	INIT_DELAYED_WORK(&sbu_sw->debug_uart_dwork, sbu_sw_debug_uart_work);
#endif

	p = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(p)) {
		pr_err("failed to set default\n");
		return -ENODEV;
	}

	schedule_delayed_work(&sbu_sw->tcpc_dwork, 0);

	g_sbu_sw = sbu_sw;

	return 0;
}

static const struct of_device_id sbu_sw_match_table[] = {
	{ .compatible = "lge,lge_sbu_switch" },
	{ }
};
MODULE_DEVICE_TABLE(of, sbu_sw_match_table);

static struct platform_driver lge_sbu_switch_driver = {
	.driver = {
		.name = "lge_sbu_switch",
		.of_match_table = sbu_sw_match_table,
	},
	.probe = sbu_sw_probe,
};
module_platform_driver(lge_sbu_switch_driver);

MODULE_DESCRIPTION("LGE CC/SBU Protection Switch driver");
MODULE_LICENSE("GPL v2");
