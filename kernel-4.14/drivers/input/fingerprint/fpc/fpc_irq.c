#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/err.h>
//#include <linux/wakelock.h>
#include <linux/pm_wakeup.h>
#include <linux/regulator/consumer.h>

#include "fpc_irq.h"

#define FPC_RESET_LOW_US 5000
#define FPC_RESET_HIGH1_US 100
#define FPC_RESET_HIGH2_US 5000

#define FPC_TTW_HOLD_TIME 1000
#define SUPPLY_1V8	1800000UL
#define SUPPLY_3V3	3300000UL
#define SUPPLY_TX_MIN	SUPPLY_3V3
#define SUPPLY_TX_MAX	SUPPLY_3V3

#ifdef USE_FB_NOTIFIER
#include <linux/fb.h>
#include <linux/notifier.h>
#ifdef USE_FB_NOTIFIER_LGE_CUSTOM
#include <linux/lge_panel_notify.h>
#endif
#endif // USE_FB_NOTIFIER

#ifdef USE_FP_ID_DUAL
static int g_fp_id;

extern int get_fp_id_from_gpio(void);
#endif // USE_FP_ID_DUAL

#ifdef USE_FB_NOTIFIER
#ifdef USE_FB_NOTIFIER_LGE_CUSTOM
static int (*fb_notifier_register)(struct notifier_block *nb) = lge_panel_notifier_register_client;
static int (*fb_notifier_unregister)(struct notifier_block *nb) = lge_panel_notifier_unregister_client;
#else
static int (*fb_notifier_register)(struct notifier_block *nb) = fb_register_client;
static int (*fb_notifier_unregister)(struct notifier_block *nb) = fb_unregister_client;
#endif

static int fb_notifier_callback(struct notifier_block *nb, unsigned long action, void *data)
{
	struct fpc_data *fpc = container_of(nb, struct fpc_data, fb_notifier);
#ifdef USE_FB_NOTIFIER_LGE_CUSTOM
	static const int BLANK = LGE_PANEL_EVENT_BLANK;
	struct lge_panel_notifier *event = data;
	int *event_data = &event->state;
#else
	static const int BLANK = FB_EVENT_BLANK;
	struct fb_event *event = data;
	int *event_data = event->data;
#endif
	char *envp[2];

	//printk(KERN_INFO "fingerprint " "%s: action %lu event->data %d\n", __func__, action, *event_data);

	/* If we aren't interested in this event, skip it immediately ... */
	if (action != BLANK) {
		return NOTIFY_DONE;
	}

	switch (*event_data) {
#ifdef USE_FB_NOTIFIER_LGE_CUSTOM
	case LGE_PANEL_STATE_UNBLANK: // U3, UNBLANK
#else
	case FB_BLANK_UNBLANK:
#endif
		envp[0] = "SCREEN=1";
		break;

#ifdef USE_FB_NOTIFIER_LGE_CUSTOM
	case LGE_PANEL_STATE_BLANK: // U0, BLANK
	case LGE_PANEL_STATE_LP1: // U2_UNBLANK @ AOD
	case LGE_PANEL_STATE_LP2: // U2_BLANK @ AOD
#else
	case FB_BLANK_POWERDOWN:
#endif
		envp[0] = "SCREEN=0";
		break;

	default:
		printk(KERN_WARNING "fingerprint " "%s: skip event data %d\n", __func__, *event_data);
		return NOTIFY_DONE;
	}

	envp[1] = NULL;
	//printk(KERN_INFO "fingerprint " "%s: envp[0] %s\n", __func__, envp[0]);

#ifdef USE_VIRTUAL_INPUT
	kobject_uevent_env(&fpc->idev->dev.kobj, KOBJ_CHANGE, envp);
#else
	kobject_uevent_env(&fpc->wdev->dev.kobj, KOBJ_CHANGE, envp);
#endif

	return NOTIFY_OK;
}
#endif // USE_FB_NOTIFIER

static int hw_reset(struct fpc_data *fpc)
{
	int irq_gpio;
	struct device *dev = &fpc->wdev->dev;

	fpc->hwabs->set_val(fpc->rst_gpio, 1);
	usleep_range(FPC_RESET_HIGH1_US, FPC_RESET_HIGH1_US + 100);

	fpc->hwabs->set_val(fpc->rst_gpio, 0);
	usleep_range(FPC_RESET_LOW_US, FPC_RESET_LOW_US + 100);

	fpc->hwabs->set_val(fpc->rst_gpio, 1);
	usleep_range(FPC_RESET_HIGH2_US, FPC_RESET_HIGH2_US + 100);

	irq_gpio = fpc->hwabs->get_val(fpc->irq_gpio);
	dev_info(dev, "IRQ after reset %d\n", irq_gpio);

	dev_info(dev, "Using GPIO#%d as IRQ.\n", fpc->irq_gpio);
	dev_info(dev, "Using GPIO#%d as RST.\n", fpc->rst_gpio);

	return 0;
}

static ssize_t hw_reset_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct fpc_data *fpc = dev_get_drvdata(dev);

	if (!strncmp(buf, "reset", strlen("reset"))) {
		rc = hw_reset(fpc);
		return rc ? rc : count;
	}
	else
		return -EINVAL;


}
static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, hw_reset_set);

/**
 * sysfs node for controlling whether the driver is allowed
 * to wake up the platform on interrupt.
 */
static ssize_t wakeup_enable_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc_data *fpc = dev_get_drvdata(dev);
	ssize_t ret = count;

	mutex_lock(&fpc->mutex);
	if (!strncmp(buf, "enable", strlen("enable")))
		fpc->wakeup_enabled = true;
	else if (!strncmp(buf, "disable", strlen("disable"))) {
		fpc->wakeup_enabled = false;
		fpc->nbr_irqs_received = 0;
	}
	else
		ret = -EINVAL;
	mutex_unlock(&fpc->mutex);

	return ret;
}
static DEVICE_ATTR(wakeup_enable, S_IWUSR, NULL, wakeup_enable_set);

/**
 * sysfs node for controlling the wakelock.
 */
static ssize_t handle_wakelock_cmd(struct device *device,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	struct device *dev = &fpc->wdev->dev;
	ssize_t ret = count;

	mutex_lock(&fpc->mutex);
	if (!strncmp(buf, RELEASE_WAKELOCK_W_V, min(count,
		strlen(RELEASE_WAKELOCK_W_V)))) {
		if (fpc->nbr_irqs_received_counter_start ==
				fpc->nbr_irqs_received) {
			pm_relax(dev);
		} else {
			dev_warn(dev, "Ignore releasing of wakelock %d != %d\n",
				fpc->nbr_irqs_received_counter_start,
				fpc->nbr_irqs_received);
		}
	} else if (!strncmp(buf,RELEASE_WAKELOCK, min(count,
					strlen(RELEASE_WAKELOCK)))) {
			pm_relax(dev);
	} else if (!strncmp(buf, START_IRQS_RECEIVED_CNT,
			min(count, strlen(START_IRQS_RECEIVED_CNT))))
		fpc->nbr_irqs_received_counter_start = fpc->nbr_irqs_received;
	else
		ret = -EINVAL;
	mutex_unlock(&fpc->mutex);

	return ret;
}
static DEVICE_ATTR(handle_wakelock, S_IWUSR, NULL, handle_wakelock_cmd);

/**
 * sysf node to check the interrupt status of the sensor, the interrupt
 * handler should perform sysf_notify to allow userland to poll the node.
 */
static ssize_t irq_get(struct device *device,
			struct device_attribute *attribute,
			char* buffer)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	int irq = gpio_get_value(fpc->irq_gpio);

	return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}

/**
 * writing to the irq node will just drop a printk message
 * and return success, used for latency measurement.
 */
static ssize_t irq_ack(struct device *device,
			struct device_attribute *attribute,
			const char *buffer, size_t count)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	struct device *dev = &fpc->wdev->dev;

	dev_info(dev, "%s\n", __func__);

	return count;
}

static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);

static ssize_t clk_enable_set(struct device *device,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc_data *fpc = dev_get_drvdata(device);

	if (!fpc->hwabs->clk_enable_set)
		return count;

	return fpc->hwabs->clk_enable_set(fpc, buf, count);
}

static DEVICE_ATTR(clk_enable, S_IWUSR, NULL, clk_enable_set);

#ifdef USE_FP_ID_DUAL
static ssize_t get_fp_id (
		struct device* device,
		struct device_attribute* attribute,
		char* buffer)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	struct device *dev = &fpc->wdev->dev;

	(void) attribute;

	dev_info(dev, "%s: fp_id = %d\n", __func__, g_fp_id);

	return scnprintf(buffer, PAGE_SIZE, "%i\n", g_fp_id);
}

static ssize_t set_fp_id (
		struct device* device,
		struct device_attribute* attribute,
		const char* buffer,
		size_t count)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	struct device *dev = &fpc->wdev->dev;

	(void) attribute;

	if (*buffer == '1') {
		g_fp_id = 1;
	} else {
		g_fp_id = 0;
	}

	dev_info(dev, "%s: fp_id = %d\n", __func__, g_fp_id);

	return count;
}

static ssize_t get_fp_id_real (
		struct device* device,
		struct device_attribute* attribute,
		char* buffer)
{
	struct fpc_data *fpc = dev_get_drvdata(device);
	struct device *dev = &fpc->wdev->dev;
	int fp_id = get_fp_id_from_gpio();

	(void) attribute;

	dev_info(dev, "%s: fp_id = %d\n", __func__, fp_id);

	return scnprintf(buffer, PAGE_SIZE, "%i\n", fp_id);
}


static DEVICE_ATTR(fp_id, S_IRUGO | S_IWUSR, get_fp_id, set_fp_id);
static DEVICE_ATTR(fp_id_real, S_IRUGO | S_IWUSR, get_fp_id_real, NULL);
#endif // USE_FP_ID_DUAL

static struct attribute *fpc_attributes[] = {
	&dev_attr_hw_reset.attr,
	&dev_attr_wakeup_enable.attr,
	&dev_attr_handle_wakelock.attr,
	&dev_attr_clk_enable.attr,
	&dev_attr_irq.attr,
#ifdef USE_FP_ID_DUAL
	&dev_attr_fp_id.attr,
	&dev_attr_fp_id_real.attr,
#endif // USE_FP_ID_DUAL
	NULL,
};

static const struct attribute_group const fpc_attribute_group = {
	.attrs = fpc_attributes,
};

static irqreturn_t fpc_irq_handler(int irq, void *handle)
{
	struct fpc_data *fpc = handle;
	struct device *dev = &fpc->wdev->dev;

	if (fpc->hwabs->irq_handler)
		fpc->hwabs->irq_handler(irq, fpc);

	/* Make sure 'wakeup_enabled' is updated before using it
	** since this is interrupt context (other thread...) */
	mutex_lock(&fpc->mutex);
	if (fpc->wakeup_enabled) {
		pm_wakeup_event(dev, FPC_TTW_HOLD_TIME);
		fpc->nbr_irqs_received++;
	}
	mutex_unlock(&fpc->mutex);

#ifdef USE_VIRTUAL_INPUT
	sysfs_notify(&fpc->idev->dev.kobj, NULL, dev_attr_irq.attr.name);
#else
	sysfs_notify(&fpc->wdev->dev.kobj, NULL, dev_attr_irq.attr.name);
#endif

	return IRQ_HANDLED;
}

static int fpc_request_named_gpio(struct fpc_data *fpc, const char *label, int *gpio)
{
	struct device *dev = &fpc->wdev->dev;
	struct device_node *node = dev->of_node;
	int rc = of_get_named_gpio(node, label, 0);
	if (rc < 0) {
		dev_err(dev, "failed to get '%s'\n", label);
		return rc;
	}
	*gpio = rc;
	rc = devm_gpio_request(dev, *gpio, label);
	if (rc) {
		dev_err(dev, "failed to request gpio %d\n", *gpio);
		return rc;
	}
	dev_info(dev, "%s %d\n", label, *gpio);
	return 0;
}

int fpc_probe(struct WRAP_DEVICE *wdev,
		struct fpc_gpio_info *fpc_gpio_ops)
{
	struct device *dev = &wdev->dev;
	struct device_node *node = dev->of_node;
	struct fpc_data *fpc;
	int irqf;
	int irq_num;
	int rc;

	dev_info(dev, "%s\n", __func__);

	fpc = devm_kzalloc(dev, sizeof(*fpc), GFP_KERNEL);
	if (!fpc) {
		dev_err(dev,
			"failed to allocate memory for struct fpc_data\n");
		rc = -ENOMEM;
		goto exit;
	}

	dev_set_drvdata(dev, fpc);
	fpc->wdev = wdev;
	fpc->hwabs = fpc_gpio_ops;

	if (!node) {
		dev_err(dev, "no of node found\n");
		rc = -EINVAL;
		goto exit;
	}

	rc = fpc->hwabs->init(fpc);

	if (rc) {
		printk(KERN_INFO "error\n");
		goto exit;
	}

	/* Get the gpio pin used for irq from device tree */
	rc = fpc_request_named_gpio(fpc, "fpc,gpio_irq",
			&fpc->irq_gpio);
	if (rc) {
		dev_err(dev, "Requesting GPIO for IRQ failed with %d.\n", rc);
		goto exit;
	}

	rc = fpc_request_named_gpio(fpc, "fpc,gpio_rst",
			&fpc->rst_gpio);
	if (rc) {
		dev_err(dev, "Requesting GPIO for RST failed with %d.\n", rc);
		goto exit;
	}

	rc = fpc->hwabs->configure(fpc, &irq_num, &irqf);

	if (rc < 0)
		goto exit;

	dev_info(dev, "Using GPIO#%d as IRQ.\n", fpc->irq_gpio);
	dev_info(dev, "Using GPIO#%d as RST.\n", fpc->rst_gpio);

	fpc->wakeup_enabled = false;
	mutex_init(&fpc->mutex);

	dev->init_name = "fpc_dev";
	irqf |= IRQF_ONESHOT;
	if (of_property_read_bool(dev->of_node, "fpc,enable-wakeup")) {
		irqf |= IRQF_NO_SUSPEND;
		device_init_wakeup(dev, 1);
	}
	rc = devm_request_threaded_irq(dev, irq_num,
			NULL, fpc_irq_handler, irqf,
			dev_name(dev), fpc);
	if (rc) {
		dev_err(dev, "could not request irq %d\n", irq_num);
		goto exit;
	}
	dev_info(dev, "requested irq %d\n", irq_num);

	/* Request that the interrupt should be wakeable */
	enable_irq_wake(irq_num);

#ifdef USE_VIRTUAL_INPUT
	fpc->idev = input_allocate_device();

	if (!fpc->idev) {
		dev_err(dev, "input_allocate_device failed.\n");
		goto exit;
	}

	fpc->idev->name = "fingerprint";
	fpc->idev->dev.init_name = "lge_fingerprint";

	input_set_drvdata(fpc->idev, fpc);

	rc = input_register_device(fpc->idev);
	if (rc) {
		dev_err(dev, "input_register_device failed.");
		input_free_device(fpc->idev);
		fpc->idev = NULL;
		goto exit;
	}

	rc = sysfs_create_group(&fpc->idev->dev.kobj, &fpc_attribute_group);
#else
	rc = sysfs_create_group(&dev->kobj, &fpc_attribute_group);
#endif
	if (rc) {
		dev_err(dev, "could not create sysfs\n");
		goto exit;
	}

#ifdef USE_FP_ID_DUAL
	g_fp_id = FP_ID_VALUE;
#endif

#ifdef USE_FB_NOTIFIER
	/* Register screen on/off callback. */
	fpc->fb_notifier.notifier_call = fb_notifier_callback;
	rc = fb_notifier_register(&fpc->fb_notifier);
	if (rc) {
		printk(KERN_ERR "fingerprint " "%s: could not register fb notifier", __func__);
		goto exit;
	}
#endif // USE_FB_NOTIFIER

	hw_reset(fpc);
	dev_info(dev, "%s: ok\n", __func__);

	return rc;

exit:
	if (fpc != NULL) {
#ifdef USE_VIRTUAL_INPUT
		if (fpc->idev != NULL) {
			dev_info(dev, "release input device.\n");
			input_unregister_device(fpc->idev);
			input_free_device(fpc->idev);
			fpc->idev = NULL;
		}
#endif
#ifdef USE_FB_NOTIFIER
		fb_notifier_unregister(&fpc->fb_notifier);
#endif
	}

	return rc;
}

int fpc_remove(struct WRAP_DEVICE *wdev)
{
	struct fpc_data *fpc = dev_get_drvdata(&wdev->dev);
	struct device *dev = &wdev->dev;

#ifdef USE_FB_NOTIFIER
	fb_notifier_unregister(&fpc->fb_notifier);
#endif

#ifdef USE_VIRTUAL_INPUT
	sysfs_remove_group(&fpc->idev->dev.kobj, &fpc_attribute_group);
#else
	sysfs_remove_group(&fpc->wdev->dev.kobj, &fpc_attribute_group);
#endif
	mutex_destroy(&fpc->mutex);

	dev_info(dev, "%s\n", __func__);

	return 0;
}

MODULE_LICENSE("GPL");
