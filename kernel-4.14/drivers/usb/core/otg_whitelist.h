/*
 * drivers/usb/core/otg_whitelist.h
 *
 * Copyright (C) 2004 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * This OTG and Embedded Host Whitelist is "Targeted Peripheral List".
 * It should mostly use of USB_DEVICE() or USB_DEVICE_VER() entries..
 *
 * YOU _SHOULD_ CHANGE THIS LIST TO MATCH YOUR PRODUCT AND ITS TESTING!
 */

static struct usb_device_id whitelist_table[] = {

/* hubs are optional in OTG, but very handy ... */
{ USB_DEVICE_INFO(USB_CLASS_HUB, 0, 0), },
{ USB_DEVICE_INFO(USB_CLASS_HUB, 0, 1), },
#ifdef CONFIG_USBIF_COMPLIANCE
{ USB_DEVICE_INFO(USB_CLASS_MASS_STORAGE, 0, 0), },
{ USB_DEVICE_INFO(USB_CLASS_HID, 0, 0), },
{ USB_DEVICE_INFO(0, 0, 0), },
#endif

#ifdef	CONFIG_USB_PRINTER		/* ignoring nonstatic linkage! */
/* FIXME actually, printers are NOT supposed to use device classes;
 * they're supposed to use interface classes...
 */
{ USB_DEVICE_INFO(7, 1, 1) },
{ USB_DEVICE_INFO(7, 1, 2) },
{ USB_DEVICE_INFO(7, 1, 3) },
#endif

#ifdef	CONFIG_USB_NET_CDCETHER
/* Linux-USB CDC Ethernet gadget */
{ USB_DEVICE(0x0525, 0xa4a1), },
/* Linux-USB CDC Ethernet + RNDIS gadget */
{ USB_DEVICE(0x0525, 0xa4a2), },
#endif

#if	IS_ENABLED(CONFIG_USB_TEST)
/* gadget zero, for testing */
{ USB_DEVICE(0x0525, 0xa4a0), },
#endif

{ }	/* Terminating entry */
};

#ifdef CONFIG_USBIF_COMPLIANCE
#define USB_CLASS_ID_HID			3
#define USB_CLASS_ID_MASS_STORAGE	8
#define USB_CLASS_ID_HUB			9
#endif

static int is_targeted(struct usb_device *dev)
{
	struct usb_device_id	*id = whitelist_table;

#ifdef CONFIG_USBIF_COMPLIANCE
	dev_info(&dev->dev, "idVendor %x\n", dev->descriptor.idVendor);
	dev_info(&dev->dev, "idProduct %x\n", dev->descriptor.idProduct);
	dev_info(&dev->dev, "bcdDevice %x\n", dev->descriptor.bcdDevice);
	dev_info(&dev->dev, "bDeviceClass %x\n", dev->descriptor.bDeviceClass);
	dev_info(&dev->dev, "bDeviceSubClass %x\n", dev->descriptor.bDeviceSubClass);
	dev_info(&dev->dev, "bDeviceProtocol %x\n",dev->descriptor.bDeviceProtocol);
#endif

	/* HNP test device is _never_ targeted (see OTG spec 6.6.6) */
	if ((le16_to_cpu(dev->descriptor.idVendor) == 0x1a0a &&
	     le16_to_cpu(dev->descriptor.idProduct) == 0xbadd))
		return 0;

	/* OTG PET device is always targeted (see OTG 2.0 ECN 6.4.2) */
	if ((le16_to_cpu(dev->descriptor.idVendor) == 0x1a0a &&
	     le16_to_cpu(dev->descriptor.idProduct) == 0x0200))
		return 1;

#if defined(CONFIG_USBIF_COMPLIANCE)
	/* HUB */
	if (le16_to_cpu(dev->descriptor.bDeviceClass) == USB_CLASS_ID_HUB){
		dev_err(&dev->dev, "device v%04x p%04x HUB\n",
		le16_to_cpu(dev->descriptor.idVendor),
		le16_to_cpu(dev->descriptor.idProduct));
		return 1 ;
	}

	/* HID */
	if (le16_to_cpu(dev->descriptor.bDeviceClass) == USB_CLASS_ID_HID &&
		le16_to_cpu(dev->descriptor.bDeviceSubClass) == 0 && le16_to_cpu(dev->descriptor.bDeviceProtocol) == 0) {
		dev_err(&dev->dev, "device v%04x p%04x HID\n",
		le16_to_cpu(dev->descriptor.idVendor),
		le16_to_cpu(dev->descriptor.idProduct));
		return 1;
	}
	/* STORAGE */
	if (le16_to_cpu(dev->descriptor.bDeviceClass) == USB_CLASS_ID_MASS_STORAGE &&
		le16_to_cpu(dev->descriptor.bDeviceSubClass) == 0 && le16_to_cpu(dev->descriptor.bDeviceProtocol) == 0) {
		dev_err(&dev->dev, "device v%04x p%04x STORAGE\n",
		le16_to_cpu(dev->descriptor.idVendor),
		le16_to_cpu(dev->descriptor.idProduct));
		return 1;
	}
	/* OTG PET device is always targeted (see OTG 2.0 ECN 6.4.2) */
	if ((le16_to_cpu(dev->descriptor.idVendor) == 0x1a0a &&
		 le16_to_cpu(dev->descriptor.idProduct) == 0x0201)) {
		dev_err(&dev->dev, "device v%04x p%04x PET case 1\n",
		le16_to_cpu(dev->descriptor.idVendor),
		le16_to_cpu(dev->descriptor.idProduct));
		return 0;
	}

	/* OTG PET device is always targeted (see OTG 2.0 ECN 6.4.2) */
	if (le16_to_cpu(dev->descriptor.idVendor) == 0x1a0a) {
		dev_err(&dev->dev, "device v%04x p%04x PET case 2\n",
		le16_to_cpu(dev->descriptor.idVendor),
		le16_to_cpu(dev->descriptor.idProduct));
		return 0;
	}
#endif
	/* NOTE: can't use usb_match_id() since interface caches
	 * aren't set up yet. this is cut/paste from that code.
	 */
	for (id = whitelist_table; id->match_flags; id++) {
		if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
		    id->idVendor != le16_to_cpu(dev->descriptor.idVendor))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
		    id->idProduct != le16_to_cpu(dev->descriptor.idProduct))
			continue;

		/* No need to test id->bcdDevice_lo != 0, since 0 is never
		   greater than any unsigned number. */
		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO) &&
		    (id->bcdDevice_lo > le16_to_cpu(dev->descriptor.bcdDevice)))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI) &&
		    (id->bcdDevice_hi < le16_to_cpu(dev->descriptor.bcdDevice)))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS) &&
		    (id->bDeviceClass != dev->descriptor.bDeviceClass))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS) &&
		    (id->bDeviceSubClass != dev->descriptor.bDeviceSubClass))
			continue;

		if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL) &&
		    (id->bDeviceProtocol != dev->descriptor.bDeviceProtocol))
			continue;

		return 1;
	}

	/* add other match criteria here ... */


	/* OTG MESSAGE: report errors here, customize to match your product */
	dev_err(&dev->dev, "device v%04x p%04x is not supported\n",
		le16_to_cpu(dev->descriptor.idVendor),
		le16_to_cpu(dev->descriptor.idProduct));

	return 0;
}

