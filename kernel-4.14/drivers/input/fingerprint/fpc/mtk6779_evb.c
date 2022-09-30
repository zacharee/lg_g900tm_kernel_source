/*
 * FPC Fingerprint sensor device driver
 *
 * This driver will control the platform resources that the FPC fingerprint
 * sensor needs to operate. The major things are probing the sensor to check
 * that it is actually connected and let the Kernel know this and with that also
 * enabling and disabling of regulators, enabling and disabling of platform
 * clocks, controlling GPIOs such as SPI chip select, sensor reset line, sensor
 * IRQ line, MISO and MOSI lines.
 *
 * The driver will expose most of its available functionality in sysfs which
 * enables dynamic control of these features from eg. a user space process.
 *
 * The sensor's IRQ events will be pushed to Kernel's event handling system and
 * are exposed in the drivers event node. This makes it possible for a user
 * space process to poll the input node and receive IRQ events easily. Usually
 * this node is available under /dev/input/eventX where 'X' is a number given by
 * the event system. A user space process will need to traverse all the event
 * nodes and ask for its parent's name (through EVIOCGNAME) which should match
 * the value in device tree named input-device-name.
 *
 * This driver will NOT send any SPI commands to the sensor it only controls the
 * electrical parts.
 *
 *
 * Copyright (c) 2015 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

//#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>

#include "fpc_irq.h"

#define FPC_RESET_LOW_US 5000
#define FPC_RESET_HIGH1_US 100
#define FPC_RESET_HIGH2_US 5000

//#define FPC_TTW_HOLD_TIME 1000
//#define SUPPLY_1V8	1800000UL
//#define SUPPLY_3V3	3300000UL
//#define SUPPLY_TX_MIN	SUPPLY_3V3
//#define SUPPLY_TX_MAX	SUPPLY_3V3

//#define DEBUG 1

#ifdef USE_SPI_DEVICE
#define WRAP_DRIVER             spi_driver
#define WRAP_DRIVER_REGISTER    spi_register_driver
#define WRAP_DRIVER_UNREGISTER  spi_unregister_driver
#else
#define WRAP_DRIVER             platform_driver
#define WRAP_DRIVER_REGISTER    platform_driver_register
#define WRAP_DRIVER_UNREGISTER  platform_driver_unregister
#endif

#ifdef USE_ONE_BINARY
extern int lge_get_fingerprint_hwinfo(void);
#endif // USE_ONE_BINARY

#ifdef USE_FP_ID_DUAL
extern int get_fp_id_from_gpio(void)
{
	struct device_node *node = of_find_compatible_node(NULL, NULL, "mediatek,fp_id");
	int fp_gpio = of_get_named_gpio(node, "fp-id-gpios", 0);
	int fp_val = __gpio_get_value(fp_gpio);

	return fp_val;
}
#endif // USE_FP_ID_DUAL

#ifdef USE_SPI_DEVICE
extern void mt_spi_disable_master_clk(struct spi_device *spidev);
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
#endif


static ssize_t clk_enable_set(struct fpc_data *fpc, const char *buf, size_t count)
{
#ifdef USE_SPI_DEVICE
	bool enable = (*buf == '1');

	mutex_lock(&fpc->mutex);

	if (enable == fpc->clocks_enabled) {
		goto out;
	}

	if (enable) {
		mt_spi_enable_master_clk(fpc->wdev);
	} else {
		mt_spi_disable_master_clk(fpc->wdev);
	}

	fpc->clocks_enabled = enable;

out:
	mutex_unlock(&fpc->mutex);
#endif

	return count;
}

static int mtk6779_evb_init(struct fpc_data *fpc)
{
	struct device *dev = &fpc->wdev->dev;
	dev_info(dev, "%s", __func__);
	return 0;
}

static int mtk6779_evb_configure(struct fpc_data *fpc, int *irq_num, int *irq_trig_flags)
{
	struct device *dev = &fpc->wdev->dev;
	int rc = 0;

	dev_info(dev, "%s", __func__);

	rc = gpio_direction_input(fpc->irq_gpio);
	if (rc != 0) {
		dev_err(dev, "gpio_direction_output failed for IRQ.\n");
		return rc;
	}

	*irq_num = gpio_to_irq(fpc->irq_gpio);
	*irq_trig_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

	rc = gpio_direction_output(fpc->rst_gpio, 1);
	if (rc != 0) {
		dev_err(dev, "gpio_direction_output failed for RST.\n");
		return rc;
	}

	/* ensure it is high for a while */
	usleep_range(FPC_RESET_HIGH1_US, FPC_RESET_HIGH1_US + 100);

	/* set reset low */
	gpio_set_value(fpc->rst_gpio, 0);
	/* ensure it is low long enough */
	usleep_range(FPC_RESET_LOW_US, FPC_RESET_LOW_US + 100);

	return rc;
}

static struct fpc_gpio_info mtk6779_evb_ops = {
	.init = mtk6779_evb_init,
	.configure = mtk6779_evb_configure,
	.get_val = gpio_get_value,
	.set_val = gpio_set_value,
	.clk_enable_set = clk_enable_set,
	.irq_handler = NULL,
};

static struct of_device_id mtk6779_evb_of_match[] = {
#ifdef USE_SPI_DEVICE
	{ .compatible = "fingerprint,mediatek", }, // spi_device
#else
	{ .compatible = "mediatek,fingerprint", }, // platform_device (odm)
#endif
	{ .compatible = "fpc,fpc_irq", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk6779_evb_of_match);

static int mtk6779_evb_probe(struct WRAP_DEVICE *wdev)
{
	return fpc_probe(wdev, &mtk6779_evb_ops);
}

static struct WRAP_DRIVER mtk6779_evb_driver = {
	.driver = {
		.name = "fpc_irq",
		.owner = THIS_MODULE,
		.of_match_table = mtk6779_evb_of_match,
	},
	.probe = mtk6779_evb_probe,
	.remove = fpc_remove
};

static int __init fpc_init(void)
{
	int rc = 0;

#ifdef USE_ONE_BINARY
	{
		int is_support = lge_get_fingerprint_hwinfo();

		if (!is_support) {
			printk(KERN_WARNING "[fingerprint] %s: doesn't support fingerprint, skip register\n", __func__);

			return -1;
		}

		printk(KERN_INFO "[fingerprint] %s: support fingerprint\n", __func__);
	}
#endif // USE_ONE_BINARY

#ifdef USE_FP_ID_DUAL
	{
		int fp_id = get_fp_id_from_gpio();

		if (fp_id != FP_ID_VALUE) {
			printk(KERN_WARNING "[fingerprint] %s: fp_id mismatch, value = %d\n", __func__, fp_id);

			return -1;
		}

		printk(KERN_INFO "[fingerprint] %s: fp_id value = %d\n", __func__, fp_id);
	}
#endif // USE_FP_ID_DUAL

	rc = WRAP_DRIVER_REGISTER(&mtk6779_evb_driver);

	if (rc) {
		printk(KERN_ERR "[fingerprint] %s: driver register failed %d.\n", __func__, rc);
	}

	return rc;
}

static void __exit fpc_exit(void)
{
	WRAP_DRIVER_UNREGISTER(&mtk6779_evb_driver);
}

//module_spi_driver(mtk6779_evb_driver);
late_initcall(fpc_init);
module_exit(fpc_exit);

MODULE_LICENSE("GPL");
