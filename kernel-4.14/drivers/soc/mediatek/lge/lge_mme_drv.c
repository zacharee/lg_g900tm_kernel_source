/* Copyright (c) 2016 LG Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include <soc/mediatek/lge/lge_mme.h>

#define DEBUG 0

/* predefined cmd */
#define CMD_MME_POWER_ON	30
#define CMD_MME_POWER_OFF	31
#define CMD_MME_NFC_ON		40
#define CMD_MME_NFC_OFF		41

/* global variables
   should be updated from dts */
static int gpio_sleep_n = 0;  // DRV8838_SLEEP_N_Pin, MSM GPIO 3

static unsigned int command_type = 0;           // send_commnad (odd: once, even: repeat)

static struct class *mme_class;
static struct device *mme_dev;
static int mme_major;

static int nfc_state = 0;
static int wmc_state = 0;

typedef enum tagMME_BIAS_GPIO_STATE {
	MME_GPIO_STATE_SLEEP_N0,
	MME_GPIO_STATE_SLEEP_N1,
	MME_BIAS_GPIO_STATE_MAX,	/* for array size */
} MME_BIAS_GPIO_STATE;

static struct pinctrl *mme_bias_pctrl; /* static pinctrl instance */
static const char *mme_bias_state_name[MME_BIAS_GPIO_STATE_MAX] = {
    "mme_gpio_sleep_n0",
    "mme_gpio_sleep_n1",
}; /* DTS state mapping name */

static int mme_bias_set_state(const char *name)
{
    int ret = 0;
    struct pinctrl_state *pState = 0;

    if (!mme_bias_pctrl) {
	pr_info("this pctrl is null\n");
	return -1;
    }

    pState = pinctrl_lookup_state(mme_bias_pctrl, name);
    if (IS_ERR(pState)) {
        pr_err("set state '%s' failed\n", name);
        ret = PTR_ERR(pState);
        goto exit;
    }

    /* select state! */
    pinctrl_select_state(mme_bias_pctrl, pState);

exit:
    return ret; /* Good! */
}

static int mme_bias_gpio_select_state(MME_BIAS_GPIO_STATE s)
{
	int ret = 0;
    BUG_ON(!((unsigned int)(s) < (unsigned int)(MME_BIAS_GPIO_STATE_MAX)));
    ret = mme_bias_set_state(mme_bias_state_name[s]);
	return ret;
}

/*
 * [mme_command] node read/write function
 */
static ssize_t lge_show_mme_command (struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", command_type);
}

static ssize_t lge_store_mme_command (struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &command_type);
	pr_err("[MME] %s: Start to send. command_type=%d\n", __func__, command_type);

	switch(command_type) {

		case CMD_MME_POWER_ON:
			pr_err("[MME] CMD_MME_POWER_ON\n");
			if (nfc_state == 1) {
				pr_err("[MME] NFC is still active\n");
				break;
			} else if (wmc_state == 1) {
				pr_err("[MME] WMC is already on\n");
				break;
			}

			/* Set WMC n_sleep to high */
			/* gpio_set_value(gpio_sleep_n, 1); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N1);
			wmc_state = 1;
			break;

		case CMD_MME_POWER_OFF:
			pr_err("[MME] CMD_MME_POWER_OFF\n");
			if (nfc_state == 1) {
				/* MME Power has already been off when NFC is acive */
				pr_err("[MME] NFC is still active\n");
				break;
			} else if (wmc_state == 0) {
				pr_err("[MME] WMC is already off\n");
			}

			/* Set WMC n_sleep to low */
			/* gpio_set_value(gpio_sleep_n, 0); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N0);
			wmc_state = 0;
			break;

		case CMD_MME_NFC_ON:
			pr_err("[MME] CMD_MME_NFC_ON\n");

			/* Set WMC n_sleep to low */
			/* gpio_set_value(gpio_sleep_n, 0); */
			mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N0);
			wmc_state = 0;

			/* Set NFC state to active */
			nfc_state = 1;
			break;

		case CMD_MME_NFC_OFF:
			pr_err("[MME] CMD_MME_NFC_OFF\n");
			nfc_state = 0;
			break;

		default:
			pr_err("[MME] Not suppported cmd_id(%d)\n", command_type);
			return -EINVAL;
			break;
	}

	return count;
}

/*        sysfs		  name	  perm	   cat function			echo function   */
static DEVICE_ATTR(mme_command, 0664, lge_show_mme_command, lge_store_mme_command);

static struct attribute *lge_mme_attrs[] = {
	&dev_attr_mme_command.attr,
	NULL
};

static const struct attribute_group lge_mme_files = {
	.attrs  = lge_mme_attrs,
};

static int mme_drv_gpio_init(struct device *dev)
{
	struct pinctrl *pinctrl = NULL;
	struct device_node *node = NULL;
	int ret;

	/* set up GPIO */
	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		pr_err("%s::error, fail to get pinctrl\n", __func__);
		return -ENODEV;
	}

	mme_bias_pctrl = pinctrl;

	ret = mme_bias_gpio_select_state(MME_GPIO_STATE_SLEEP_N0);
	if (ret)
		return ret;

	node = of_find_compatible_node(NULL, NULL, "lge,mme");
	if (of_property_read_u32_index(node, "lge,mme_sleep_n", 0, &gpio_sleep_n)) {
		pr_err("%s::error, fail to get lge,mme_sleep_n\n", __func__);
		return -ENODEV;
	}
	pr_err("%s::gpio_sleep_n=%d\n",__func__, gpio_sleep_n);
	return 0;
}

extern int lge_get_wmc_support(void);

static int __init lge_mme_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (lge_get_wmc_support() == 0) {
		pr_err("[MME] not support wmc %d\n", lge_get_wmc_support());
		ret = -ENODEV;
		goto exit;
	}
	pr_err("[MME] %s: probe enter\n", __func__);

	/* TO BE : mme drv gpio init */
	ret = mme_drv_gpio_init(&pdev->dev);
	if (ret < 0 ) {
		pr_err("[MME] %s: gpio init failed, ret %d\n", __func__, ret);
		return ret;
	}

	mme_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(mme_class)) {
		pr_err("[MME] %s: class_create() failed ENOMEM\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	mme_dev = device_create(mme_class, NULL, MKDEV(mme_major, 0), NULL, "mme_ctrl");
	if (IS_ERR(mme_dev)) {
		pr_err("[MME] %s: device_create() failed\n", __func__);
		ret = PTR_ERR(mme_dev);
		goto exit;
	}

	/* create /sys/class/lge_mme/mme_ctrl/mme_command */
	ret = device_create_file(mme_dev, &dev_attr_mme_command);
	if (ret < 0) {
		pr_err("[MME] %s: device create file fail\n", __func__);
		goto exit;
	}

	pr_info("[MME] %s: probe done\n", __func__);
	return 0;

exit:
	pr_err("[MME] %s: probe fail - %d\n", __func__, ret);
	return ret;
}

static int lge_mme_remove(struct platform_device *pdev)
{
	device_remove_file(mme_dev, &dev_attr_mme_command);
	device_destroy(mme_class, MKDEV(mme_major, 0));
	class_destroy(mme_class);

	return 0;
}

/* device driver structures */
static struct of_device_id mme_match_table[] = {
	{ .compatible = "lge,mme",},
	{},
};

static struct platform_driver lge_mme_driver __refdata = {
	.probe = lge_mme_probe,
	.remove = lge_mme_remove,
	.driver = {
		.name = "lge_mme_drv",
		.owner = THIS_MODULE,
		.of_match_table = mme_match_table,
	},
};

/* driver init funcion */
static int __init lge_mme_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&lge_mme_driver);
	if (ret < 0)
		pr_err("[MME] %s : platform_driver_register() err=%d\n", __func__, ret);

	return ret;
}

/* driver exit function */
static void __exit lge_mme_exit(void)
{
	platform_driver_unregister(&lge_mme_driver);
}

module_init(lge_mme_init);
module_exit(lge_mme_exit);

MODULE_DESCRIPTION("LGE MME driver for LG pay test");
MODULE_AUTHOR("jinsol.jo <jinsol.jo@lge.com>");
MODULE_LICENSE("GPL");
