/*
 * LGE USB DS3 driver
 *
 * Copyright (C) 2019 LG Electronics, Inc.
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
#define DEBUG
//#define VERBOSE_DEBUG

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/extcon.h>
#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/usb.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>

#include <linux/lge_ds3.h>
#include <linux/hall_ic.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <soc/mediatek/lge/board_lge.h>
#include <soc/mediatek/lge/lge_boot_mode.h>

#include "./../../misc/mediatek/typec/tcpc/inc/pd_dpm_core.h"
#include "./../../misc/mediatek/typec/tcpc/inc/tcpci_core.h"
#include "./../../misc/mediatek/typec/tcpc/inc/tcpci.h"
#include "./../../misc/mediatek/typec/tcpc/inc/tcpm.h"
#include "./../../gpu/drm/mediatek/lge/dp/lge_dp_def.h"

#ifdef CONFIG_LGE_USB_SBU_SWITCH
#include <soc/mediatek/lge/lge_sbu_switch.h>
#endif

#ifdef CONFIG_LGE_USB_TUSB546
#include "./tusb546/tusb546.h"
#endif

#ifdef CONFIG_LGE_PM
#include <mt-plat/mtk_boot_common.h>
#include <soc/mediatek/lge/board_lge.h>
#endif

extern bool lge_get_mfts_mode(void);

static bool use_primary_usb;
module_param(use_primary_usb, bool, 0644);

static bool usb_sudden_disconnect_check;
module_param(usb_sudden_disconnect_check, bool, 0644);

static bool ds_recovery_store_check;
module_param(ds_recovery_store_check, bool, 0644);

static bool usb_2nd_host_test;
module_param(usb_2nd_host_test, bool, 0644);

static bool force_set_hallic;
module_param(force_set_hallic, bool, 0644);

static unsigned int ds_accid_reg_en_delay_ms = 50;
module_param(ds_accid_reg_en_delay_ms, uint, 0644);

static unsigned int ds_recheck_accid_ms = 2000;
module_param(ds_recheck_accid_ms, uint, 0644);

static unsigned int ds_usb_check_time_ms = 3000;
module_param(ds_usb_check_time_ms, uint, 0644);

static unsigned int ds_vconn_recovery_time_ms = 100;
module_param(ds_vconn_recovery_time_ms, uint, 0644);

static unsigned int ds_power_recovery_count = 5;
module_param(ds_power_recovery_count, uint, 0644);

static unsigned int usb_recovery_time_ms = 2000;
module_param(usb_recovery_time_ms, uint, 0644);

static unsigned int acc_high_threshold_uv;
module_param(acc_high_threshold_uv, uint, 0644);

static unsigned int acc_low_threshold_uv;
module_param(acc_low_threshold_uv, uint, 0644);

#define DP_USBPD_VDM_STATUS		0x10
#define DP_USBPD_VDM_CONFIGURE		0x11

enum ds_state {
	STATE_UNKNOWN,
	STATE_DS_USB_WAIT,
	STATE_DS_STARTUP,
	STATE_DS_READY,
	STATE_DS_RECOVERY,
	STATE_DS_RECOVERY_POWER_OFF,
	STATE_DS_RECOVERY_POWER_ON,
	STATE_DS_RECOVERY_USB_WAIT,
	STATE_DS_DLOAD,
};

static const char * const ds_state_strings[] = {
	"Unknown",
	"DS_USB_Wait",
	"DS_Startup",
	"DS_Ready",
	"DS_Recovery",
	"DS_Recovery_Power_Off",
	"DS_Recovery_Power_On",
	"DS_Recovery_USB_Wait",
	"DS_Dload",
};

enum ds3_usb {
	DS_USB_DISCONNECTED = 0,
	DS_USB_CONNECTED,
	DS_USB_DLOAD_CONNECTED,
};

struct ds3 {
	struct device			*dev;

	struct workqueue_struct		*wq;
	struct work_struct		sm_work;
	struct delayed_work		ds_acc_detect_work;
	struct hrtimer			acc_timer;
	struct hrtimer			timer;
	bool				sm_queued;
	enum ds_state			current_state;

	struct power_supply		*usb_psy;
	struct notifier_block		psy_nb;
	enum typec_attach_type	typec_type;

	struct extcon_dev		*extcon[2];
	struct gpio_desc		*load_sw_on;
	struct gpio_desc		*ds_en;
	struct gpio_desc		*acc_id_detect_en;
	struct notifier_block		nb;
	struct usb_device		*udev;

	bool				is_ds_connected;
	enum ds3_usb			is_ds_usb_connected;
	bool				is_ds_hal_ready;
	int				is_ds_recovery;
	bool				is_dp_configured;
	bool				is_dp_hpd_high;
	bool				is_accid_connected;

	bool				is_usb_connected;
	bool				vbus_present;
	bool				acc_det_vcomp;
	bool				acc_det_vadc;
	int				acc_det_count;
	int				pd_active;

	int				acc_detect;
	struct	iio_channel	*channel;
	int			acc_high_thr;
	int			acc_low_thr;

	bool		previous_hallic;
};

enum pd_control_msg_type {
	MSG_RESERVED = 0,
	MSG_GOODCRC,
	MSG_GOTOMIN,
	MSG_ACCEPT,
	MSG_REJECT,
	MSG_PING,
	MSG_PS_RDY,
	MSG_GET_SOURCE_CAP,
	MSG_GET_SINK_CAP,
	MSG_DR_SWAP,
	MSG_PR_SWAP,
	MSG_VCONN_SWAP,
	MSG_WAIT,
	MSG_SOFT_RESET,
	MSG_NOT_SUPPORTED = 0x10,
};

enum usbpd_data_msg_type {
	MSG_SOURCE_CAPABILITIES = 1,
	MSG_REQUEST,
	MSG_BIST,
	MSG_SINK_CAPABILITIES,
	MSG_VDM = 0xF,
};

static struct ds3 *__ds3 = NULL;

static bool hallic_status = false;
static bool *ds3_connected = NULL;
static bool tcpc_ready_status = false;

static const unsigned int ds_extcon_cable[] = {
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

static void kick_sm(struct ds3 *ds3, int ms);
static void ds_set_state(struct ds3 *ds3, enum ds_state next_state);

extern struct hallic_dev luke_sdev;
extern void request_dualscreen_recovery(void);
extern struct lge_dp_display *get_lge_dp(void);
extern void call_disconnect_uevent(void);
static bool check_ds3_accid(struct ds3 *ds3);
#ifdef CONFIG_LGE_USB_MOISTURE_FUSB251
extern void fusb251_enable_moisture(bool);
#endif

void set_hallic_status(bool enable)
{
	struct ds3 *ds3 = __ds3;

	#ifdef CONFIG_LGE_DUAL_SCREEN_DISABLE
	/* pr_info("%s: ds does not support\n", __func__); */
	return;
	#endif

	hallic_status = enable;

	if (!ds3) {
		pr_err("%s: %d (not ready yet)\n", __func__, enable);
		return;
	}

	pm_stay_awake(ds3->dev);
	dev_dbg(ds3->dev, "%s: %d\n", __func__, enable);

	/* If hallic status is changed, check acc_id */
	if (ds3->previous_hallic != hallic_status) {
		ds3->previous_hallic = hallic_status;

	if (ds3->acc_det_vcomp || ds3->acc_det_vadc) {
		if (enable) {
			ds3->is_accid_connected = check_ds3_accid(ds3);
			if (ds3->is_accid_connected)
				ds3->acc_det_count = 0;
		} else {
			hrtimer_cancel(&ds3->acc_timer);
			ds3->is_accid_connected = 0;
			ds3->acc_det_count = 0;
		}
	} else {
		dev_info(ds3->dev, "%s: there is no acc_det_vcomp, use hallic only\n", __func__);
		ds3->is_accid_connected = enable;
	}
	}

	dev_dbg(ds3->dev, "typec:%d vbus:%d pd:%d ds:%d hallic:%d accid:%d"
		" usb:%d ds_usb:%d ds_recovery:%d\n",
		ds3->typec_type,
		ds3->vbus_present,
		ds3->pd_active,
		ds3->is_ds_connected,
		hallic_status,
		ds3->is_accid_connected,
		ds3->is_usb_connected,
		ds3->is_ds_usb_connected,
		ds3->is_ds_recovery);

	if (enable && ds3->is_accid_connected) {
		if (!ds3->is_ds_connected)
			ds_set_state(ds3, STATE_DS_STARTUP);
	} else {
		if (ds3->is_ds_connected)
			kick_sm(ds3, 0);
	}
	pm_relax(ds3->dev);
}
EXPORT_SYMBOL(set_hallic_status);

static int set_ds_extcon_state(unsigned int id, int state)
{
	int ret = 0;
	struct lge_dp_display *lge_dp = get_lge_dp();

	ret = extcon_set_state_sync(lge_dp->dd_extcon_sdev[0], id, state);

	return ret;
}

static void hallic_state_notify(struct ds3 *ds3, struct hallic_dev *hdev,
				int state)
{
	char name_buf[40];
	char state_buf[40];
	char *uevent[3] = { name_buf, state_buf, NULL };

	if (!hdev || !hdev->dev) {
		dev_err(ds3->dev, "hallic_dev is NULL\n");
		return;
	}

	if (!lge_get_mfts_mode())
		hdev->state = state;

	snprintf(name_buf, sizeof(name_buf), "SWITCH_NAME=%s", hdev->name);
	snprintf(state_buf, sizeof(state_buf), "SWITCH_STATE=%d", state);

	kobject_uevent_env(&hdev->dev->kobj, KOBJ_CHANGE, uevent);
	dev_dbg(ds3->dev, "%s: %s\n", __func__, name_buf);
	dev_dbg(ds3->dev, "%s: %s\n", __func__, state_buf);
}

bool is_ds_connected(void)
{
	struct ds3 *ds3 = __ds3;
	bool ret = ds3_connected ? *ds3_connected : false;

	if (!ds3)
		pr_debug("%s: %d (not ready yet)\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(is_ds_connected);

void ds3_set_tcpc_ready(void)
{
	tcpc_ready_status = true;
}
EXPORT_SYMBOL(ds3_set_tcpc_ready);

void set_ds_vbus2_on(bool on)
{
	struct ds3 *ds3 = __ds3;

	/* TD0040021145 Null Return Added */
	if (!ds3)	{
		pr_debug("%s: (not ready yet)\n", __func__);
		return;
	}

	if (on) {
		gpiod_direction_output(ds3->load_sw_on, 1);
		/* pr_info("%s: vbus2 on\n", __func__); */

	} else {
		gpiod_direction_output(ds3->load_sw_on, 0);
		/* pr_info("%s: vbus2 off\n", __func__); */
	}
}
EXPORT_SYMBOL(set_ds_vbus2_on);

bool is_tcpc_ready(void)
{
	return tcpc_ready_status;
}

int check_ds_connect_state(void)
{
	struct ds3 *ds3 = __ds3;

	if (!ds3)
		return 0;

	if (ds3->is_dp_hpd_high)
		return DS_STATE_HPD_ENABLED;
	else if (ds3->is_dp_configured)
		return DS_STATE_DP_CONNECTED;
	else if (ds3->is_accid_connected)
		return DS_STATE_ACC_ID_CONNECTED;
	else if (hallic_status)
		return DS_STATE_HALLIC_CONNECTED;

	return DS_STATE_DISCONNECTED;
}
EXPORT_SYMBOL(check_ds_connect_state);


#if	0	/* Fake PD Temp */
	DP_DFP_U_NONE = 0,
	DP_DFP_U_DISCOVER_ID,
	DP_DFP_U_DISCOVER_SVIDS,
	DP_DFP_U_DISCOVER_MODES,
	DP_DFP_U_ENTER_MODE,
	DP_DFP_U_STATUS_UPDATE,
	DP_DFP_U_WAIT_ATTENTION,
	DP_DFP_U_CONFIGURE,
	DP_DFP_U_OPERATION,
	DP_DFP_U_STATE_NR,
#endif

static void dp_configure(void)
{
	struct tcpc_device *tcpc = g_tcpc;

	/* tcpci_dp_configure */
	ds3_tcpc_check_notify_DP_STATE(tcpc);
	msleep(1200);
}

static int ds_dp_config(struct ds3 *ds3, bool config)
{
	struct device *dev = ds3->dev;

	dev_info(dev, "%s: config:%d\n", __func__, config);

	if (config) {
		ds3->is_dp_configured = true;
		/* handler->connect(handler, false); */

		dp_configure();

		if (ds3->is_ds_usb_connected == DS_USB_CONNECTED) {
			set_ds_extcon_state(EXTCON_DISP_DS2, 1);
			/* Remove luke state at START UP	*
			   Not sure that ttyACM is ready.	*/
			#if	0
			dev_err(dev, "%s: current luke state = %d\n", __func__,
				luke_sdev.state);
			hallic_state_notify(ds3, &luke_sdev, 1);
			#endif
		}
	} else {
		ds3->is_dp_configured = false;
		/* handler->disconnect(handler); */
	}

#ifdef CONFIG_LGE_USB_SBU_SWITCH
	if (config)
		lge_sbu_switch_enable(LGE_SBU_MODE_AUX, true);
	else
		lge_sbu_switch_enable( LGE_SBU_MODE_AUX, false);
#endif

	return 0;
}

static int ds3_dp_hpd(struct ds3 *ds3, bool hpd)
{
	struct device *dev = ds3->dev;
	struct tcpc_device *tcpc = g_tcpc;

	if(!is_tcpc_ready()) {
		ds3_tcpc_check_notify_DP_STATE(tcpc);
		dev_info(dev, "%s: set DP_STATE again\n", __func__);
		msleep(10);
	}

	dev_info(dev, "%s: is_dp_hpd_high:%d hpd: %d\n", __func__,
			ds3->is_dp_hpd_high, hpd);

	/* Remove force off for MFTS */
	#if	0
	if (ds3->is_dp_hpd_high && hpd)
		ds3_dp_hpd(ds3, false);
	#endif

	if (ds3->is_dp_hpd_high == hpd) {
		dev_dbg(dev, "%s: duplicated value is set\n", __func__);
		return 0;
	}
	ds3->is_dp_hpd_high = hpd;

	if (ds3->is_dp_hpd_high) {
#ifdef CONFIG_LGE_USB_TUSB546
		if (ds3->is_ds_connected) {
			resetting_ds3_register();
		}
#endif
		ds3_tcpc_check_notify_DP_ATTENTION(tcpc);
		ds3_tcpc_check_notify_DP_HPD_STATE(tcpc);
	} else {
		ds3_tcpc_check_notify_DP_HPD_STATE_OFF(tcpc);
	}

	return 0;
}

static void stop_usb_host(struct ds3 *ds3)
{
	struct device *dev = ds3->dev;
	struct tcpc_device *tcpc = g_tcpc;
	struct extcon_dev *extcon = ds3->extcon[use_primary_usb ? 1 : 0];

	if (!extcon)
		return;

	if (extcon_get_state(extcon, EXTCON_USB_HOST) == 0)
		return;

	dev_dbg(dev, "%s\n", __func__);

	extcon_set_state_sync(extcon, EXTCON_USB_HOST, 0);

	if (ds3->is_dp_hpd_high)
		ds3_tcpc_check_notify_DP_HPD_STATE_OFF(tcpc);

	if (tcpm_inquire_typec_role(tcpc) != TYPEC_ROLE_TRY_SNK) {
		tcpm_typec_change_role(tcpc, TYPEC_ROLE_TRY_SNK);
	}
}

static void start_usb_host(struct ds3 *ds3)
{
	struct device *dev = ds3->dev;
	struct tcpc_device *tcpc = g_tcpc;
	struct extcon_dev *extcon = ds3->extcon[use_primary_usb ? 1 : 0];

	if (!extcon)
		return;

	if (extcon_get_state(extcon, EXTCON_USB_HOST) == 1)
		return;

	dev_dbg(dev, "%s\n", __func__);

	if (use_primary_usb) {
		union extcon_property_value val;
		val.intval = 1;
		extcon_set_property(extcon, EXTCON_USB_HOST,
				    EXTCON_PROP_USB_TYPEC_POLARITY, val);
		val.intval = 0;
		extcon_set_property(extcon, EXTCON_USB_HOST,
				    EXTCON_PROP_USB_SS, val);
	}
	extcon_set_state_sync(extcon, EXTCON_USB_HOST, 1);
	if (tcpm_inquire_typec_role(tcpc) != TYPEC_ROLE_SNK) {
		tcpm_typec_change_role(tcpc, TYPEC_ROLE_SNK);
	}
}

static bool is_start_usb_host(struct ds3 *ds3)
{
	struct extcon_dev *extcon = ds3->extcon[use_primary_usb ? 1 : 0];

	if (!extcon)
		return false;

	return extcon_get_state(extcon, EXTCON_USB_HOST) == 1;
}

static int ds3_usb_notify(struct notifier_block *nb, unsigned long action,
			  void *data)
{
	struct ds3 *ds3 = container_of(nb, struct ds3, nb);
	struct device *dev = ds3->dev;
	struct usb_device *udev = data;
	struct tcpc_device *tcpc = g_tcpc;

	dev_vdbg(dev, "%s: dev num:%d path:%s\n", __func__,
		udev->devnum, udev->devpath);
	dev_vdbg(dev, "%s: bus num:%d name:%s\n", __func__,
		udev->bus->busnum, udev->bus->bus_name);

	if (usb_2nd_host_test)
		return NOTIFY_DONE;

	switch (action) {
	case USB_DEVICE_ADD:
		if (!udev->parent)
			return NOTIFY_DONE;

		dev_info(dev, "%s: USB_DEVICE_ADD: idVendor:%04x idProduct:%04x bcdDevice:%04x\n",
			__func__,
			udev->descriptor.idVendor,
			udev->descriptor.idProduct,
			udev->descriptor.bcdDevice);

		ds3->is_usb_connected = true;

		if (!ds3->is_ds_connected)
			return NOTIFY_DONE;

		if (!IS_DS3_ANY_USB(udev) )
			return NOTIFY_DONE;

		set_ds_extcon_state(EXTCON_DISP_DS2, 1);

		// DS3 or DS3 USB Connected
		if (IS_DS3_USB(udev)) {
			dev_dbg(dev, "%s: FW_VER: %s-V%02u%c_XX\n", __func__,
				udev->product ? udev->product : DS3_CAYMAN_PRODUCT_STR,
				(udev->descriptor.bcdDevice >> 8) & 0xff,
				'a' + ((udev->descriptor.bcdDevice & 0xff) % 26/*a-z*/));

			ds3->is_ds_usb_connected = DS_USB_CONNECTED;
			dev_dbg(dev, "%s USB CONNECTED\n",__func__);

			/* Boot Up or Recheck tcpc role */
			if (tcpm_inquire_typec_role(tcpc) != TYPEC_ROLE_SNK) {
				tcpm_typec_change_role(tcpc, TYPEC_ROLE_SNK);
				dev_dbg(dev, "%s: Role Changed DS connected\n", __func__);
			}

		// DS3 Dload USB Connected
		} else if (IS_DS3_DLOAD_USB(udev)) {
			ds3->is_ds_usb_connected = DS_USB_DLOAD_CONNECTED;
			ds_set_state(ds3, STATE_DS_DLOAD);

			dev_err(dev, "%s: currunt luke state = %d\n",
				__func__, luke_sdev.state);
			call_disconnect_uevent();
			hallic_state_notify(ds3, &luke_sdev, 0);
		}

		return NOTIFY_OK;

	case USB_DEVICE_REMOVE:
		if (!udev->parent)
			return NOTIFY_DONE;

		dev_info(dev, "%s: USB_DEVICE_REMOVE: idVendor:%04x idProduct:%04x\n",
			__func__,
			udev->descriptor.idVendor,
			udev->descriptor.idProduct);

		ds3->is_usb_connected = false;

		if (!IS_DS3_ANY_USB(udev))
			return NOTIFY_DONE;

		ds3->is_ds_usb_connected = DS_USB_DISCONNECTED;
		ds3->is_ds_hal_ready = false;

		// DS3 USB Disconnected
		if (IS_DS3_USB(udev)) {
			BUG_ON(usb_sudden_disconnect_check);

			if (ds3->is_ds_connected && ds3->is_ds_recovery <= 0) {
				ds3->is_ds_recovery = ds_power_recovery_count;
				ds_set_state(ds3, STATE_DS_RECOVERY);
			} else if (hallic_status && ds3->is_accid_connected) {
				set_hallic_status(true);
			}

		// DS3 Dload USB Disconnected
		} else if (IS_DS3_DLOAD_USB(udev)) {

		}

		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static void ds3_sm(struct work_struct *w)
{
	struct ds3 *ds3 = container_of(w, struct ds3, sm_work);
	struct device *dev = ds3->dev;
	int ret = 0;

	hrtimer_cancel(&ds3->timer);
	ds3->sm_queued = false;

	if (usb_2nd_host_test) {
		if (ds3->typec_type == TYPEC_UNATTACHED) {
			stop_usb_host(ds3);
		} else {
			start_usb_host(ds3);
		}
		goto sm_done;
	}

	dev_info(dev, "%s: %s\n", __func__,
			ds_state_strings[ds3->current_state]);

#ifdef CONFIG_LGE_PM
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT
		|| get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
		if(!strcmp(unified_bootmode_region(), "CAN")
			|| !strcmp(unified_bootmode_region(), "USA")) {
			dev_info(dev, "%s: Disable DS3 forcely in chargerlogo\n",
				__func__);
			goto sm_done;
		}
	}
#endif

	// disconnect
	if (!hallic_status || !ds3->is_accid_connected) {
		if (!ds3->is_ds_connected) {
			dev_dbg(dev, "%s: DS3 is already disconnected\n",
				__func__);
			goto sm_done;
		}

		dev_info(dev, "%s: DS disconnect\n", __func__);
		ds3->is_ds_connected = false;

		// Secondary USB
		if (is_start_usb_host(ds3)) {
			stop_usb_host(ds3);
		}

		ds_dp_config(ds3, false);

		// Disable DisplayPort
		dev_err(dev, "%s: currunt luke state = %d\n", __func__,
				luke_sdev.state);
		set_ds_extcon_state(EXTCON_DISP_DS2, 0);
		call_disconnect_uevent();
		hallic_state_notify(ds3, &luke_sdev, 0);

		// DS Power Off
		if (ds3->ds_en) {
			gpiod_direction_output(ds3->ds_en, 0);
		}
		gpiod_direction_output(ds3->load_sw_on, 0);

		ds3->is_dp_configured = false;
		ds3->is_dp_hpd_high = false;

		ds3->is_ds_recovery = 0;

		ds3->current_state = STATE_UNKNOWN;

		set_ds3_start(false);

#ifdef CONFIG_LGE_USB_MOISTURE_FUSB251
		fusb251_enable_moisture(true);
#endif
		goto sm_done;
	}

	switch (ds3->current_state) {
	case STATE_UNKNOWN:
	case STATE_DS_STARTUP:
		if (ds3->is_ds_connected) {
			dev_dbg(dev, "%s: DS3 is already connected\n",
				__func__);
			goto sm_done;
		}

#ifdef CONFIG_LGE_USB_MOISTURE_FUSB251
		fusb251_enable_moisture(false);
#endif
		ds3->is_ds_connected = true;

		// DS Power On
		if (ds3->ds_en) {
			gpiod_direction_output(ds3->ds_en, 1);
		}

		gpiod_direction_output(ds3->load_sw_on, 1);

		// Secondary USB
		if (!is_start_usb_host(ds3)) {
			start_usb_host(ds3);
		}

		ret = ds_dp_config(ds3, true);
		if (ret) {
			ds3->is_ds_connected = false;
			stop_usb_host(ds3);
			ds_set_state(ds3, STATE_UNKNOWN);
			goto sm_done;
		}

		ds3->is_dp_hpd_high = false;
		set_ds3_start(true);

		if (!ds3->is_ds_usb_connected) {
			ds_set_state(ds3, STATE_DS_USB_WAIT);
			goto sm_done;
		}

		ds3->current_state = STATE_DS_STARTUP;
		goto sm_done;
		break;

	case STATE_DS_USB_WAIT:
		if (!ds3->is_ds_usb_connected)
			ds_set_state(ds3, STATE_DS_RECOVERY_POWER_OFF);
		break;

	case STATE_DS_READY:
		if (ds3->is_dp_configured) {
			dev_err(dev, "%s: current luke state = %d\n", __func__,
					luke_sdev.state);
			hallic_state_notify(ds3, &luke_sdev, 1);
		}
		break;

	case STATE_DS_RECOVERY:
		if (ds3->is_ds_usb_connected)
			break;

		dev_info(dev, "%s: %s %d\n", __func__,
				ds_state_strings[ds3->current_state],
				ds3->is_ds_recovery);

		if (ds3->is_ds_recovery > ds_power_recovery_count)
			ds3_dp_hpd(ds3, false);

		ds3->is_ds_recovery--;
		ds_set_state(ds3, STATE_DS_RECOVERY_POWER_OFF);
		break;

	case STATE_DS_RECOVERY_POWER_OFF:
		// 2nd USB off
		stop_usb_host(ds3);

#if 0
		/* blocks until USB host is completely stopped */
		ret = extcon_blocking_sync(ds3->extcon, EXTCON_USB_HOST, 0);
		if (ret) {
			dev_err(ds3->dev, "%s: err(%d) stopping host", ret);
			break;
		}
#endif
		if (ds3->is_ds_recovery > ds_power_recovery_count) {
			ds3->current_state = STATE_DS_RECOVERY_POWER_ON;
			kick_sm(ds3, 0);
			break;
		}

		// Power Off
		if (ds3->ds_en) {
			gpiod_direction_output(ds3->ds_en, 0);
		}
		gpiod_direction_output(ds3->load_sw_on, 0);

		ds_set_state(ds3, STATE_DS_RECOVERY_POWER_ON);
		break;

	case STATE_DS_RECOVERY_POWER_ON:
		// 2nd USB on
		 start_usb_host(ds3);

#if 0
		 /* blocks until USB host is completely started */
		 ret = extcon_blocking_sync(ds3->extcon, EXTCON_USB_HOST, 0);
		 if (ret) {
			 dev_err(ds3->dev, "%s: err(%d) starting host", ret);
			 break;
		 }
#endif
		if (ds3->is_ds_recovery > ds_power_recovery_count) {
			ds_set_state(ds3, STATE_DS_RECOVERY_USB_WAIT);
			break;
		}

		if (ds3->ds_en) {
			gpiod_direction_output(ds3->ds_en, 1);
		}
		gpiod_direction_output(ds3->load_sw_on, 1);

		ds_set_state(ds3, STATE_DS_RECOVERY_USB_WAIT);
		break;

	case STATE_DS_RECOVERY_USB_WAIT:
		ds_set_state(ds3, STATE_DS_RECOVERY);
		break;

	case STATE_DS_DLOAD:
		break;

	default:
		dev_err(dev, "%s: Unhandled state %s\n", __func__,
			ds_state_strings[ds3->current_state]);
		break;
	}

sm_done:
	if (!ds3->sm_queued)
		pm_relax(ds3->dev);
}

static void ds_set_state(struct ds3 *ds3, enum ds_state next_state)
{
	struct device *dev = ds3->dev;
	dev_dbg(dev, "%s: %s -> %s\n", __func__,
			ds_state_strings[ds3->current_state],
			ds_state_strings[next_state]);

	ds3->current_state = next_state;

	switch (next_state) {
	case STATE_DS_USB_WAIT:
		kick_sm(ds3, ds_usb_check_time_ms);
		break;

	case STATE_DS_STARTUP:
		kick_sm(ds3, 0);
		break;

	case STATE_DS_READY:
		kick_sm(ds3, 0);
		break;

	case STATE_DS_RECOVERY:
		if (ds3->is_ds_recovery <= 0)
			break;

		kick_sm(ds3, ds_usb_check_time_ms);
		break;

	case STATE_DS_RECOVERY_POWER_OFF:
		if (ds_power_recovery_count <= 0)
			break;

		kick_sm(ds3, 0);
		break;

	case STATE_DS_RECOVERY_POWER_ON:
		kick_sm(ds3, ds_vconn_recovery_time_ms);
		break;

	case STATE_DS_RECOVERY_USB_WAIT:
		kick_sm(ds3, ds_usb_check_time_ms);
		break;

	case STATE_DS_DLOAD:
		ds3->is_ds_recovery = 0;
		ds3_dp_hpd(ds3, false);
		break;

	default:
		dev_err(dev, "%s: No action for state %s\n", __func__,
				ds_state_strings[ds3->current_state]);
		break;
	}
}

static void kick_sm(struct ds3 *ds3, int ms)
{
	pm_stay_awake(ds3->dev);
	ds3->sm_queued = true;

	if (ms) {
		dev_dbg(ds3->dev, "delay %d ms", ms);
		hrtimer_start(&ds3->timer, ms_to_ktime(ms), HRTIMER_MODE_REL);
	} else {
		queue_work(ds3->wq, &ds3->sm_work);
	}
}

static enum hrtimer_restart ds_timeout(struct hrtimer *timer)
{
	struct ds3 *ds3 = container_of(timer, struct ds3, timer);

	queue_work(ds3->wq, &ds3->sm_work);

	return HRTIMER_NORESTART;
}

static void ds_acc_detect(struct work_struct *w)
{
	struct ds3 *ds3 = container_of(w, struct ds3, ds_acc_detect_work.work);

	if (hallic_status && !ds3->is_accid_connected)
		set_hallic_status(true);
}

static enum hrtimer_restart ds_acc_timeout(struct hrtimer *timer)
{
	struct ds3 *ds3 = container_of(timer, struct ds3, acc_timer);

	schedule_delayed_work(&ds3->ds_acc_detect_work, 0);

	return HRTIMER_NORESTART;
}

static int psy_changed(struct notifier_block *nb, unsigned long evt, void *ptr)
{
	struct ds3 *ds3 = container_of(nb, struct ds3, psy_nb);
	struct device *dev = ds3->dev;
	//union power_supply_propval val;
	enum typec_attach_type typec_type;
	//int ret;

	if (ptr != ds3->usb_psy || evt != PSY_EVENT_PROP_CHANGED)
		return 0;

	#if 0	// DS3_TEMP - CHANGE METHOD
	ret = power_supply_get_property(ds3->usb_psy,
			POWER_SUPPLY_PROP_PD_ACTIVE, &val);
	if (ret) {
		dev_err(dev, "Unable to read PD_ACTIVE: %d\n", ret);
		return ret;
	}

	ds3->pd_active = val.intval;
	#else
	ds3->pd_active = 1;
	#endif

	#if 0	// DS3_TEMP - CHANGE METHOD
	ret = power_supply_get_property(ds3->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &val);
	if (ret) {
		dev_err(dev, "Unable to read USB PRESENT: %d\n", ret);
		return ret;
	}
	ds3->vbus_present = val.intval;
	#else
	ds3->vbus_present = 1;
	#endif

	#if	0	// DS3_TEMP - CHANGE METHOD
	ret = power_supply_get_property(ds3->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_MODE, &val);
	if (ret < 0) {
		dev_err(dev, "Unable to read USB TYPEC_MODE: %d\n", __func__);
		return ret;
	}
	typec_type = val.intval;
	#else
	typec_type = 1;
	#endif

	if (force_set_hallic)
		hallic_status = true;

	dev_dbg(dev, "typec:%d vbus:%d pd:%d ds:%d hallic:%d accid:%d"
		" usb:%d ds_usb:%d ds_recovery:%d\n",
		typec_type,
		ds3->vbus_present,
		ds3->pd_active,
		ds3->is_ds_connected,
		hallic_status,
		ds3->is_accid_connected,
		ds3->is_usb_connected,
		ds3->is_ds_usb_connected,
		ds3->is_ds_recovery);

	ds3->typec_type = typec_type;

	return 0;
}

static ssize_t ds2_hal_ready_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct ds3 *ds3 = dev_get_drvdata(dev);
	bool ready;
	int ret;

	if (!ds3->is_ds_connected)
		return -ENODEV;

	ret = strtobool(buf, &ready);
	if (ret < 0)
		return ret;

	dev_info(ds3->dev, "%s: ready:%d, recovery:%d\n", __func__,
			ready, ds3->is_ds_recovery);

	if (!ready)
		return size;

	ds3->is_ds_hal_ready = true;

	if (ds3->is_ds_recovery || ds3->current_state == STATE_DS_RECOVERY)
		request_dualscreen_recovery();

	ds3->is_ds_recovery = 0;
	ds_set_state(ds3, STATE_DS_READY);

	return size;
}
static DEVICE_ATTR_WO(ds2_hal_ready);

static ssize_t ds2_pd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ds3 *ds3 = dev_get_drvdata(dev);
	dev_dbg(ds3->dev, "%s: hpd_high:%d\n", __func__, ds3->is_dp_hpd_high);
	return scnprintf(buf, PAGE_SIZE, "%d", ds3->is_dp_hpd_high);
}

static ssize_t ds2_pd_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct ds3 *ds3 = dev_get_drvdata(dev);
	int hpd_high, refresh_layer;

	if (sscanf(buf, "%d%d", &hpd_high, &refresh_layer) <= 0) {
		dev_err(ds3->dev, "%s: invalid agument: %s", __func__, buf);
		return -EINVAL;
	}

	dev_info(ds3->dev, "%s: hpd_high:%d refresh_layer:%d\n", __func__,
			hpd_high, refresh_layer);

	if (!ds3->is_dp_configured) {
		dev_info(ds3->dev, "%s: dp is not configured\n", __func__);
		return size;
	}

#ifdef CONFIG_LGE_USB_SBU_SWITCH
	if(hpd_high)
		lge_sbu_switch_enable(LGE_SBU_MODE_AUX, true);
#endif

#ifdef CONFIG_LGE_USB_TUSB546
	if (!hpd_high) {
		set_tusb546_off();
	} else {
		if(!is_tusb546_pwr_on())
			set_tusb546_rollback();
	}
#endif

	ds3_dp_hpd(ds3, hpd_high);

	return size;
}

static DEVICE_ATTR_RW(ds2_pd);

static ssize_t ds2_recovery_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct ds3 *ds3 = dev_get_drvdata(dev);
	bool recovery;
	int ret;

	if (!ds3->is_ds_connected)
		return -ENODEV;

	ret = strtobool(buf, &recovery);
	if (ret < 0)
		return ret;

	dev_info(ds3->dev, "%s: recovery:%d\n", __func__, recovery);

	if (!recovery)
		return size;

	BUG_ON(ds_recovery_store_check);

	ds3->is_ds_recovery = ds_power_recovery_count;
	if (ds3->is_ds_recovery > 0)
		ds3->is_ds_recovery++;
	ds_set_state(ds3, STATE_DS_RECOVERY_POWER_OFF);

	return size;
}
// #define RECOVERY_PATH "/sys/class/dualscreen/ds2/ds2_recovery"
static DEVICE_ATTR_WO(ds2_recovery);

static int ds3_enable_acc_regulator(struct ds3 *ds3, bool enable)
{

	if (ds3->acc_id_detect_en)
		gpiod_direction_output(ds3->acc_id_detect_en, enable);

  	/* regulator enable - QCT */

	return 0;
}

static bool check_ds3_accid(struct ds3 *ds3)
{
	bool connected = false;
	int val;
	int i;

	ds3_enable_acc_regulator(ds3, 1);
	msleep(ds_accid_reg_en_delay_ms);

	if (ds3->acc_det_vadc) {
		for (i = 0; i < 3; i++) {
			if (!ds3->channel) {
				dev_err(ds3->dev, "ADC Channel doesn't exist\n");
				ds3_enable_acc_regulator(ds3, 0);
				return 0;
			}

			if (i == 0) {
				msleep(300);
			}

			if(iio_read_channel_processed(ds3->channel, &val) < 0)//TD#968_CHECKED_RETURN
				dev_info(ds3->dev,"%s: error\n", __func__);

			val	=(val * 1500) >> 12;
			dev_err(ds3->dev, "ACC_DETECT VADC=%d mV\n", val);
			if (val <= acc_high_threshold_uv &&
			    val >= acc_low_threshold_uv) {
				connected = true;
				break;
			}

			msleep(20);
		}
	}

	ds3_enable_acc_regulator(ds3, 0);

	if (!connected) {
		if (ds3->acc_det_count < 5) {
			ds3->acc_det_count++;

			dev_dbg(ds3->dev, "hallic detected but acc_id isn't. start timer\n");
			hrtimer_start(&ds3->acc_timer,
				      ms_to_ktime(ds_recheck_accid_ms),
				      HRTIMER_MODE_REL);
		} else {
			dev_info(ds3->dev, "acc_detect retry count exceeded\n");
		}
	}

	return connected;
}

#if	0	// DS3_TEMP - NOT NEED
static int ds3_probe_vcomp(struct ds3 *ds3)
{
	struct device *dev = ds3->dev;
	int ret = 0;

	ds3->acc_detect = of_get_named_gpio(dev->of_node, "lge,acc-detect", 0);

	if (ds3->acc_detect < 0) {
		dev_err(dev, "Failed to get acc_detect: %d\n", ds3->acc_detect);
		return -ENODEV;
	}

	ret = gpio_request_one(ds3->acc_detect, GPIOF_DIR_IN, "acc_detect");
	if (ret < 0) {
		dev_err(dev, "Failed to request acc_detect, ret=%d\n", ret);
		return -ENXIO;
	}

	return 0;
}
#endif

static int ds3_probe_vadc(struct ds3 *ds3)
{
	struct iio_channel *channel 	= NULL;
	struct device *dev = ds3->dev;
	int ret = 0;

	channel	= iio_channel_get(dev, "thermistor-ch3");
	if (PTR_ERR(channel) == -EPROBE_DEFER) {
		dev_err(dev, "channel probe defer\n");
		return -EPROBE_DEFER;
	}
	if (IS_ERR(channel)) {
		ret = PTR_ERR(channel);
		dev_err(dev, "Unable to get VADC dev\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "lge,acc-high-thr",
						&ds3->acc_high_thr);
	ret = of_property_read_u32(dev->of_node, "lge,acc-low-thr",
						&ds3->acc_low_thr);
	if (ret) {
		dev_err(dev, "error reading acc threshold level property\n");
		return ret;
	}

	acc_high_threshold_uv = ds3->acc_high_thr;
	acc_low_threshold_uv = ds3->acc_low_thr;
	ds3->channel	= channel;
	ds3->acc_det_count = 0;

	return 0;
}

static int ds3_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ds3 *ds3;
	static struct class *ds3_class;
	static struct device *ds3_dev;
	int ret = 0;

	dev_info(dev, "%s\n", __func__);

	/* QCT PD INIT */

	ds3 = devm_kzalloc(dev, sizeof(*ds3), GFP_KERNEL);
	if (!ds3) {
		dev_err(dev, "out of memory\n");
		return -ENOMEM;
	}

	dev_set_drvdata(dev, ds3);
	ds3->dev = dev;

	ds3->usb_psy = power_supply_get_by_name("usb");
	if (!ds3->usb_psy) {
		dev_err(dev, "couldn't get USB power_supply, deferring probe\n");
		ret = -EPROBE_DEFER;
		goto err;
	}

	ds3->load_sw_on = devm_gpiod_get(dev, "lge,load-sw-on", GPIOD_OUT_LOW);
	if (IS_ERR(ds3->load_sw_on)) {
		ret = PTR_ERR(ds3->load_sw_on);
		dev_err(dev, "couldn't get load-sw-on: %d\n", ret);
		goto err;
	}

	ds3->ds_en = devm_gpiod_get(dev, "lge,ds-en", GPIOD_OUT_LOW);
	if (IS_ERR(ds3->ds_en)) {
		ret = PTR_ERR(ds3->ds_en);
		dev_err(dev, "Unable to get ds_en: %d\n", ret);
		ds3->ds_en = NULL;
	}

	ds3->acc_id_detect_en = devm_gpiod_get(dev, "lge,acc-id-detect-en",
					       GPIOD_OUT_LOW);
	if (IS_ERR(ds3->acc_id_detect_en)) {
		ret = PTR_ERR(ds3->acc_id_detect_en);
		dev_err(dev, "Unable to get acc_id_detect_en: %d\n", ret);
		ds3->acc_id_detect_en = NULL;
	}

	ds3->extcon[0] = devm_extcon_dev_allocate(dev, ds_extcon_cable);
	if (IS_ERR(ds3->extcon[0])) {
		ret = PTR_ERR(ds3->extcon[0]);
		dev_err(dev, "failed to allocate extcon device: %d\n", ret);
		goto err;
	}

	ret = devm_extcon_dev_register(dev, ds3->extcon[0]);
	if (ret < 0) {
		dev_err(dev, "failed to register extcon device: %d\n", ret);
		goto err;
	}

	ds3->extcon[1] = extcon_get_edev_by_phandle(dev, 0);
	if (IS_ERR(ds3->extcon[1])) {
		if (PTR_ERR(ds3->extcon[1]) != -ENODEV)
			return PTR_ERR(ds3->extcon[1]);
		ds3->extcon[0] = NULL;
	}

	use_primary_usb = device_property_read_bool(dev, "lge,use-primary-usb");

	ret = device_init_wakeup(ds3->dev, true);
	if (ret < 0)
		goto err;

	ds3->wq = alloc_ordered_workqueue("ds_wq", WQ_HIGHPRI);
	if (!ds3->wq)
		return -ENOMEM;

	/* DEL - acc_det_vcomp read */
	ds3->acc_det_vadc = device_property_read_bool(dev,
				"lge,acc_det_vadc");
	if (ds3->acc_det_vadc) {
		ret = ds3_probe_vadc(ds3);
		if (ret < 0) {
			dev_err(dev, "failed to register vadc: %d\n", ret);
			goto err;
		}
	}

	/* DEL - get acc regulator */

	hrtimer_init(&ds3->acc_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ds3->acc_timer.function = ds_acc_timeout;

	hrtimer_init(&ds3->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ds3->timer.function = ds_timeout;

	ds3->previous_hallic = false;

	/* DEL - sbu switch */

	INIT_WORK(&ds3->sm_work, ds3_sm);
	INIT_DELAYED_WORK(&ds3->ds_acc_detect_work, ds_acc_detect);
#ifdef CONFIG_LGE_DUAL_SCREEN_USB_WA
	INIT_DELAYED_WORK(&ds3->ds_usb_wa_work, ds_usb_wa);
#endif

	ds3->psy_nb.notifier_call = psy_changed;
	ret = power_supply_reg_notifier(&ds3->psy_nb);
	if (ret < 0)
		goto err;

	ds3_class = class_create(THIS_MODULE, "dualscreen");
	if (IS_ERR(ds3_class)) {
		ret = PTR_ERR(ds3_class);
		dev_err(dev, "failed to create dualscreen class: %d\n", ret);
		goto err_create_ds3_class;
	}

	ds3_dev = device_create(ds3_class, NULL, 0, ds3, "ds2");
	if (IS_ERR(ds3_dev)) {
		ret = PTR_ERR(ds3_dev);
		dev_err(dev, "failed to create device: %d\n", ret);
		goto err_create_ds3_dev;
	}

	ret = device_create_file(ds3_dev, &dev_attr_ds2_hal_ready);
	if (ret < 0) {
		dev_err(dev, "failed to create ds2_hal_ready node: %d\n", ret);
		goto err_create_ds3_hal_ready;
	}

	ret = device_create_file(ds3_dev, &dev_attr_ds2_pd);
	if (ret < 0) {
		dev_err(dev, "failed to create ds3_pd node: %d\n", ret);
		goto err_create_ds3_pd;
	}

	ret = device_create_file(ds3_dev, &dev_attr_ds2_recovery);
	if (ret < 0) {
		dev_err(dev, "failed to create ds2_recovery node: %d\n", ret);
		goto err_create_ds3_recovery;
	}
	ds3->nb.notifier_call = ds3_usb_notify;
	usb_register_notify(&ds3->nb);

	__ds3 = ds3;

	ds3_connected = &ds3->is_dp_configured;

	/* force read initial power_supply values */
	psy_changed(&ds3->psy_nb, PSY_EVENT_PROP_CHANGED, ds3->usb_psy);

	dev_info(dev, "%s END\n", __func__);		// debug

	return 0;

err_create_ds3_recovery:
	device_remove_file(ds3_dev, &dev_attr_ds2_pd);
err_create_ds3_pd:
	device_remove_file(ds3_dev, &dev_attr_ds2_hal_ready);
err_create_ds3_hal_ready:
	device_unregister(ds3_dev);
err_create_ds3_dev:
	class_destroy(ds3_class);
err_create_ds3_class:
	power_supply_unreg_notifier(&ds3->psy_nb);
err:
	return ret;
}

static void ds3_shutdown(struct platform_device *pdev)
{
	struct ds3 *ds3 = platform_get_drvdata(pdev);

	power_supply_unreg_notifier(&ds3->psy_nb);

	hrtimer_cancel(&ds3->acc_timer);
	ds3->sm_queued = false;

	stop_usb_host(ds3);

	if (ds3->acc_detect)
		gpio_free(ds3->acc_detect);

	/* DEL - get acc regulator */
}

static const struct of_device_id ds3_match_table[] = {
	{ .compatible = "lge,usb_ds3" },
	{ }
};
MODULE_DEVICE_TABLE(of, ds3_match_table);

static struct platform_driver ds3_driver = {
	.driver = {
		.name = "lge_usb_ds3",
		.of_match_table = ds3_match_table,
	},
	.probe = ds3_probe,
	.shutdown = ds3_shutdown,
};
module_platform_driver(ds3_driver);

MODULE_AUTHOR("Hansun Lee <hansun.lee@lge.com>");
MODULE_DESCRIPTION("LGE USB DS3 driver");
MODULE_LICENSE("GPL v2");
