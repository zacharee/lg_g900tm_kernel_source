/* SPDX-License-Identifier: GPL-2.0 */
/* USB OTG (On The Go) defines */
/*
 *
 * These APIs may be used between USB controllers.  USB device drivers
 * (for either host or peripheral roles) don't use these calls; they
 * continue to use just usb_device and usb_gadget.
 */

#ifndef __LINUX_USB_OTG_H
#define __LINUX_USB_OTG_H

#include <linux/phy/phy.h>
#include <linux/usb/phy.h>

#if defined(CONFIG_USBIF_COMPLIANCE)
enum usb_otg_event {
	/* Device is not connected within
	 * TA_WAIT_BCON or not responding.
	 */
	OTG_EVENT_DEV_CONN_TMOUT,
	/* B-device returned STALL for
	 * B_HNP_ENABLE feature request.
	 */
	OTG_EVENT_NO_RESP_FOR_HNP_ENABLE,
	/* HUB class devices are not
	 * supported.
	 */
	OTG_EVENT_HUB_NOT_SUPPORTED,
	/* Device is not supported i.e
	 * not listed in TPL.
	 */
	OTG_EVENT_DEV_NOT_SUPPORTED,
	/* HNP failed due to
	 * TA_AIDL_BDIS timeout or
	 * TB_ASE0_BRST timeout
	 */
	OTG_EVENT_HNP_FAILED,
	/* B-device did not detect VBUS
	 * within TB_SRP_FAIL time.
	 */
	OTG_EVENT_NO_RESP_FOR_SRP,

	OTG_EVENT_DEV_OVER_CURRENT,
	OTG_EVENT_MAX_HUB_TIER_EXCEED,
};
#endif

struct usb_otg {
	u8			default_a;

	struct phy		*phy;
	/* old usb_phy interface */
	struct usb_phy		*usb_phy;
	struct usb_bus		*host;
	struct usb_gadget	*gadget;

	enum usb_otg_state	state;

	/* bind/unbind the host controller */
	int	(*set_host)(struct usb_otg *otg, struct usb_bus *host);

	/* bind/unbind the peripheral controller */
	int	(*set_peripheral)(struct usb_otg *otg,
					struct usb_gadget *gadget);

	/* effective for A-peripheral, ignored for B devices */
	int	(*set_vbus)(struct usb_otg *otg, bool enabled);

	/* for B devices only:  start session with A-Host */
	int	(*start_srp)(struct usb_otg *otg);

	/* start or continue HNP role switch */
	int	(*start_hnp)(struct usb_otg *otg);

};

/**
 * struct usb_otg_caps - describes the otg capabilities of the device
 * @otg_rev: The OTG revision number the device is compliant with, it's
 *		in binary-coded decimal (i.e. 2.0 is 0200H).
 * @hnp_support: Indicates if the device supports HNP.
 * @srp_support: Indicates if the device supports SRP.
 * @adp_support: Indicates if the device supports ADP.
 */
struct usb_otg_caps {
	u16 otg_rev;
	bool hnp_support;
	bool srp_support;
	bool adp_support;
};

extern const char *usb_otg_state_string(enum usb_otg_state state);

/* Context: can sleep */
static inline int
otg_start_hnp(struct usb_otg *otg)
{
	if (otg && otg->start_hnp)
		return otg->start_hnp(otg);

	return -ENOTSUPP;
}

/* Context: can sleep */
static inline int
otg_set_vbus(struct usb_otg *otg, bool enabled)
{
	if (otg && otg->set_vbus)
		return otg->set_vbus(otg, enabled);

	return -ENOTSUPP;
}

/* for HCDs */
static inline int
otg_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	if (otg && otg->set_host)
		return otg->set_host(otg, host);

	return -ENOTSUPP;
}

/* for usb peripheral controller drivers */

/* Context: can sleep */
static inline int
otg_set_peripheral(struct usb_otg *otg, struct usb_gadget *periph)
{
	if (otg && otg->set_peripheral)
		return otg->set_peripheral(otg, periph);

	return -ENOTSUPP;
}

static inline int
otg_start_srp(struct usb_otg *otg)
{
	if (otg && otg->start_srp)
		return otg->start_srp(otg);

	return -ENOTSUPP;
}

/* for OTG controller drivers (and maybe other stuff) */
extern int usb_bus_start_enum(struct usb_bus *bus, unsigned port_num);

enum usb_dr_mode {
	USB_DR_MODE_UNKNOWN,
	USB_DR_MODE_HOST,
	USB_DR_MODE_PERIPHERAL,
	USB_DR_MODE_OTG,
};

/**
 * usb_get_dr_mode - Get dual role mode for given device
 * @dev: Pointer to the given device
 *
 * The function gets phy interface string from property 'dr_mode',
 * and returns the correspondig enum usb_dr_mode
 */
extern enum usb_dr_mode usb_get_dr_mode(struct device *dev);

#endif /* __LINUX_USB_OTG_H */
