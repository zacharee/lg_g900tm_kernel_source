/*
 * SLS USB1.1 Host Controller Core
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/fpga/fpga-mgr.h>
#include <linux/spi/spi.h>
#include <linux/firmware.h>
#include <linux/clk.h>
#include <linux/extcon.h>
#include <mtk_srclken_rc_hw.h>
#include <linux/delay.h>
#include <soc/mediatek/lge/board_lge.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

static unsigned int extcon_idx = 0;
module_param(extcon_idx, uint, 0644);

struct sls11hc;

struct extcon_nb {
	struct extcon_dev		*edev;
	struct sls11hc			*hc;
	int				idx;
	struct notifier_block		host_nb;
};

struct sls11hc {
	struct device			*dev;

	struct regulator		*vdda18;

	struct gpio_desc		*ldo1_en;
	struct gpio_desc		*ldo2_en;
	struct gpio_desc		*reset;		/* DS_USB_IC_RESET_N */
	struct gpio_desc		*ds_sw_sel;
	unsigned int			creset_b;	/* CRESET_B */
	unsigned int			chip_cs;	/* SPI CS */

	bool				power_enabled;

	struct fpga_manager		*mgr;
	struct fpga_image_info		*info;
	const char *firmware_name;

	bool				host_state;
	struct extcon_nb		*extcon;
	int				ext_idx;
	struct delayed_work		sm_work;

	struct platform_device		*hcd;
};

static int sls11hc_enable_power(struct sls11hc *hc, bool on)
{
	int ret = 0;

	if (hc->power_enabled == on) {
		dev_info(hc->dev, "already %s\n", on ? "ON" : "OFF");
		return 0;
	}

	if (!on)
		goto disable_ldo;

	if (hc->ldo1_en)
		gpiod_direction_output(hc->ldo1_en, 1);

	if (hc->vdda18) {
		/* From Rev 1.1: Set 1.8v or Set 1.7v */
		if (lge_get_board_revno() >= HW_REV_1_1) {
			dev_info(hc->dev, "Set 1.8v for VM18 ");
			ret = regulator_set_voltage(hc->vdda18, 1800000, 1800000);
		} else {
			dev_info(hc->dev, "Set 1.7v for VM18 ");
			ret = regulator_set_voltage(hc->vdda18, 1700000, 1700000);
		}

		if (ret) {
			dev_err(hc->dev, "unable to set vdda18 1.7V\n");
			goto err_vdd;
		}
		ret = regulator_enable(hc->vdda18);
		if (ret) {
			dev_err(hc->dev, "unable to enable vdda18\n");
			goto err_vdd;
		}
	}

	if (hc->ldo2_en)
		gpiod_direction_output(hc->ldo2_en, 1);

	hc->power_enabled = true;
	return ret;

disable_ldo:
	if (hc->ldo2_en)
		gpiod_direction_output(hc->ldo2_en, 0);

	if (hc->vdda18) {
		ret = regulator_disable(hc->vdda18);
		if (ret)
			dev_err(hc->dev, "unable to disable vdda18\n");
	}

	if (hc->ldo1_en)
		gpiod_direction_output(hc->ldo1_en, 0);


err_vdd:
	hc->power_enabled = false;
	return ret;
}

static int sls11hc_fpga_mgr_load(struct sls11hc *hc)
{
	struct fpga_manager *mgr = hc->mgr;
	struct spi_device *spi = to_spi_device(mgr->dev.parent);
	int ret;

	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = 8000000;

	if (spi_setup(spi) < 0) {
		dev_err(hc->dev, "unable to setup SPI bus\n");
		return -EFAULT;
	}

	ret = fpga_mgr_lock(mgr);
	if (ret) {
		dev_err(hc->dev, "FPGA manager is busy\n");
		return ret;
	}

	gpiod_direction_output(hc->reset, 0);		/* RESET_N active low */

	ret = fpga_mgr_firmware_load(mgr, hc->info, hc->firmware_name);
	if (ret)
		dev_err(hc->dev, "failed to load fpga manager %d\n", ret);

	gpiod_direction_output(hc->reset, 1);		/* RESET_N to High for normal status */

	fpga_mgr_unlock(mgr);

	dev_err(hc->dev, "sls11hc_fpga_mgr_load\n");

	return ret;
}

static int sls11hc_host_init(struct sls11hc *hc)
{
	struct spi_device *spi = to_spi_device(hc->mgr->dev.parent);
	struct platform_device *hcd;
	int ret;

	hcd = platform_device_alloc("sls-hcd", PLATFORM_DEVID_AUTO);
	if (!hcd) {
		dev_err(hc->dev, "couldn't allocate sls-hcd device\n");
		return -ENOMEM;
	}

	hcd->dev.parent = &spi->dev;
	hc->hcd = hcd;

	ret = platform_device_add(hcd);
	if (ret) {
		dev_err(hc->dev, "failed to register sls-hcd device\n");
		goto err;
	}

	return 0;

err:
	platform_device_put(hcd);
	hc->hcd = NULL;
	return ret;
}

static void sls11hc_host_exit(struct sls11hc *hc)
{
	if (hc->hcd) {
		platform_device_unregister(hc->hcd);
		hc->hcd = NULL;
	}
}

static int sls11hc_start_host(struct sls11hc *hc, int on)
{
	int ret;

	if (on) {
		dev_info(hc->dev, "%s: turn on host\n", __func__);

		ret = sls11hc_enable_power(hc, true);
		if (ret)
			return ret;

		/* Restore Pin Map */
		gpio_set_value(hc->creset_b, 1);
		gpio_set_value(hc->chip_cs, 1);
		mdelay(10);

		ret = sls11hc_fpga_mgr_load(hc);
		if (ret) {
			dev_info(hc->dev, "%s: Retry Firmware Load\n", __func__);
			mdelay(2);
			if(sls11hc_fpga_mgr_load(hc))
				return ret;
		}

		if (hc->ds_sw_sel)
			gpiod_set_value(hc->ds_sw_sel, 1);

		ret = sls11hc_host_init(hc);
		if (ret)
			return ret;
	} else {
		dev_info(hc->dev, "%s: turn off host\n", __func__);

		if (hc->ds_sw_sel)
			gpiod_set_value(hc->ds_sw_sel, 0);

		sls11hc_host_exit(hc);

		sls11hc_enable_power(hc, false);

		/* DS_USB_IC_RESET_N set to LOW */
		gpiod_set_value(hc->reset, 0);

		/* ICE_CRESETB set to LOW */
		gpio_set_value(hc->creset_b, 0);

		/* SPI_CS set to LOW */
		gpio_set_value(hc->chip_cs, 0);
	}

	return 0;
}

static int sls11hc_host_notifier(struct notifier_block *nb,
		unsigned long event, void *ptr)
{
	struct extcon_dev *edev = ptr;
	struct extcon_nb *enb = container_of(nb, struct extcon_nb, host_nb);
	struct sls11hc *hc = enb->hc;

	if (!edev || !hc)
		return NOTIFY_DONE;

	dev_info(hc->dev, "sls11hc_host_notifier:%ld event check\n", event);//TD_912(Dereference before null check)


	if (extcon_idx != enb->idx)
		return NOTIFY_DONE;

	if (hc->host_state == event)
		return NOTIFY_DONE;

	/* Flush processing any pending events before handling new ones */
	flush_delayed_work(&hc->sm_work);

	dev_info(hc->dev, "host:%ld event received\n", event);

	hc->ext_idx = enb->idx;
	hc->host_state = event;

	schedule_delayed_work(&hc->sm_work, 0);

	return NOTIFY_DONE;
}

static void sls11hc_sm_work(struct work_struct *w)
{
	struct sls11hc *hc = container_of(w, struct sls11hc, sm_work.work);
	sls11hc_start_host(hc, hc->host_state);
}

static int sls11hc_extcon_register(struct sls11hc *hc)
{
	struct device_node *node = hc->dev->of_node;
	int idx, extcon_cnt;
	struct extcon_dev *edev;
	bool phandle_found = false;
	int ret;

	extcon_cnt = of_count_phandle_with_args(node, "extcon", NULL);
	if (extcon_cnt < 0) {
		dev_err(hc->dev, "of_count_phandle_with_args failed\n");
		return -ENODEV;
	}

	hc->extcon = devm_kcalloc(hc->dev, extcon_cnt,
				  sizeof(*hc->extcon), GFP_KERNEL);
	if (!hc->extcon)
		return -ENOMEM;

	for (idx = 0; idx < extcon_cnt; idx++) {
		edev = extcon_get_edev_by_phandle(hc->dev, idx);
		if (IS_ERR(edev) && PTR_ERR(edev) != -ENODEV)
			return PTR_ERR(edev);

		if (IS_ERR_OR_NULL(edev))
			continue;

		phandle_found = true;

		hc->extcon[idx].hc = hc;
		hc->extcon[idx].edev = edev;
		hc->extcon[idx].idx = idx;

		hc->extcon[idx].host_nb.notifier_call = sls11hc_host_notifier;
		ret = extcon_register_notifier(edev, EXTCON_USB_HOST,
					       &hc->extcon[idx].host_nb);
		if (ret < 0)
			continue;

		if (extcon_get_state(edev, EXTCON_USB_HOST))
			sls11hc_host_notifier(&hc->extcon[idx].host_nb,
					      true, edev);
	}

	if (!phandle_found) {
		dev_err(hc->dev, "no extcon device found\n");
		return -ENODEV;
	}

	dev_info(hc->dev, "sls11hc_extcon_register end\n");

	return 0;
}

static struct fpga_manager *of_sls11hc_get_mgr(struct device_node *np)
{
	struct device_node *mgr_node;
	struct fpga_manager *mgr;

	of_node_get(np);
	mgr_node = of_parse_phandle(np, "fpga-mgr", 0);
	if (mgr_node) {
		mgr = of_fpga_mgr_get(mgr_node);
		of_node_put(mgr_node);
		of_node_put(np);
		return mgr;
	}
	of_node_put(np);

	return ERR_PTR(-EINVAL);
}

static struct fpga_image_info *sls11hc_fpga_image_info_alloc(struct device *dev)
{
	struct fpga_image_info *info;

	get_device(dev);

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		put_device(dev);
		return ERR_PTR(-ENOMEM);
	}

	return info;
}

static ssize_t fw_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sls11hc *hc = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", hc->firmware_name);
}
static DEVICE_ATTR_RO(fw_name);

static ssize_t fw_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sls11hc *hc = dev_get_drvdata(dev);
	const struct firmware *fw;
	const char *p;
	int ret;

	ret = request_firmware(&fw, hc->firmware_name, hc->dev);
	if (ret) {
		dev_err(dev, "failed to request firmware %s\n",
			hc->firmware_name);
		return ret;
	}

	buf[0] = '\0';
	p = fw->data + 2;
	while (*p != 0xff) {
		snprintf(buf, PAGE_SIZE, "%s%s ", buf, p);
		p += strlen(p) + 1;
	}
	buf[strlen(buf) - 1] = '\n';

	release_firmware(fw);
	return strlen(buf);
}
static DEVICE_ATTR_RO(fw_info);

static struct attribute *sls11hc_attrs[] = {
	&dev_attr_fw_name.attr,
	&dev_attr_fw_info.attr,
	NULL,
};

static const struct attribute_group sls11hc_attr_group = {
	.name = NULL,	/* we want them in the same directory */
	.attrs = sls11hc_attrs,
};
static int sls11hc_probe(struct platform_device *pdev)
{
	struct sls11hc *hc;
	struct device *dev = &pdev->dev;
	int ret;

	dev_info(dev, "%s\n", __func__);

	hc = devm_kzalloc(dev, sizeof(*hc), GFP_KERNEL);
	if (!hc) {
		dev_err(dev, "failed to alloc hc\n");
		return -ENOMEM;
	}

	hc->dev = dev;
	dev_set_drvdata(dev, hc);

	INIT_DELAYED_WORK(&hc->sm_work, sls11hc_sm_work);

	/* XO_NFC clk Prepare */

	hc->vdda18 = devm_regulator_get(dev, "vdda18");
	if (IS_ERR(hc->vdda18)) {
		ret = PTR_ERR(hc->vdda18);
		dev_err(dev, "unable to get vdda18 supply %d\n", ret);
		hc->vdda18 = NULL;
	}

	hc->ldo1_en = devm_gpiod_get(dev, "ldo1_en", GPIOD_OUT_LOW);
	if (IS_ERR(hc->ldo1_en)) {
		ret = PTR_ERR(hc->ldo1_en);
		dev_err(dev, "failed to get ldo1_en gpio %d\n", ret);
		hc->ldo1_en = NULL;
	}

	hc->ldo2_en = devm_gpiod_get(dev, "ldo2_en", GPIOD_OUT_LOW);
	if (IS_ERR(hc->ldo2_en)) {
		ret = PTR_ERR(hc->ldo2_en);
		dev_err(dev, "failed to get ldo2_en gpio %d\n", ret);
		hc->ldo2_en = NULL;
	}

	hc->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(hc->reset)) {
		ret = PTR_ERR(hc->reset);
		dev_err(dev, "failed to get reset gpio %d\n", ret);
		return ret;
	}

	hc->ds_sw_sel = devm_gpiod_get(dev, "ds_sw_sel", GPIOD_OUT_LOW);
	if (IS_ERR(hc->ds_sw_sel)) {
		ret = PTR_ERR(hc->ds_sw_sel);
		dev_err(dev, "failed to get ds_sw_sel gpio %d\n", ret);
		hc->ds_sw_sel = NULL;
	}

	hc->creset_b = of_get_named_gpio(dev->of_node, "creset_b-gpio", 0);
	if (hc->creset_b < 0) {
		dev_err(dev, "failed to get creset_b gpio\n");
	}

	hc->chip_cs = of_get_named_gpio(dev->of_node, "cs-gpio", 0);
	if (hc->chip_cs < 0) {
		dev_err(dev, "failed to get chip_cs gpio\n");
	}

	hc->mgr = of_sls11hc_get_mgr(dev->of_node);
	if (IS_ERR(hc->mgr)) {
		dev_err(dev, "unable to get fpga manager\n");
		return -EPROBE_DEFER;
	}

	hc->info = sls11hc_fpga_image_info_alloc(dev);
	if (IS_ERR(hc->info)) {
		dev_err(dev, "failed to alloc fpga_image_info\n");
		return PTR_ERR(hc->info);
	}

	ret = of_property_read_string(dev->of_node, "firmware-name",
				     &hc->firmware_name);
	if (ret <0) {
		dev_err(dev, "could not read firmware name: %d\n", ret);
		return -EPROBE_DEFER;
	}

	ret = sls11hc_extcon_register(hc);
	if (ret) {
		dev_err(dev, "failed to register extcon %d\n", ret);
		return ret;
	}

	ret = sysfs_create_group(&dev->kobj, &sls11hc_attr_group);
	if (ret) {
		dev_err(dev, "failed to register sysfs attr %d\n", ret);
		return ret;
	}

	/* test */
	/* sls11hc_start_host(hc, 1); */
	return 0;
}

static const struct of_device_id of_sls11hc_match[] = {
	{ .compatible = "sls,sls11hc", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_sls11hc_match);

static struct platform_driver sls11hc_driver = {
	.probe = sls11hc_probe,
	.driver = {
		.name = "sls11hc",
		.of_match_table = of_sls11hc_match,
	},
};
module_platform_driver(sls11hc_driver);

MODULE_AUTHOR("Hansun Lee <hansun.lee@lge.com>");
MODULE_DESCRIPTION("SLS USB1.1 Host Controller Core");
MODULE_LICENSE("GPL v2");
