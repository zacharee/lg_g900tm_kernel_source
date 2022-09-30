#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>
#include <linux/fb.h>
#include <linux/pm_wakeup.h>
#include "et7xx_0410_02.h"

#ifdef USE_ONE_BINARY
extern int lge_get_fingerprint_hwinfo(void);
#endif

extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);

unsigned int bufsiz = 4096;

struct ioctl_cmd {
	int int_mode;
	int detect_period;
	int detect_threshold;
};

static void delete_device_node(void);
static struct egistec_data *g_data;

DECLARE_BITMAP(minors, N_SPI_MINORS);
LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static struct of_device_id egistec_match_table[] = {
	{ .compatible = "mediatek,fingerprint", },
	{},
};
static struct of_device_id et7xx_spi_of_match[] = {
	{ .compatible = "fingerprint,mediatek", },
	{},
};

MODULE_DEVICE_TABLE(of, et7xx_spi_of_match);
MODULE_DEVICE_TABLE(of, egistec_match_table);

static void spi_clk_enable(struct egistec_data *egistec, bool en)
{
	if (en == egistec->spi_clk_enabled) {
		DEBUG_PRINT("[EGISTEC] %s: skip duplicated clk ctrl %d %d\n", __func__, en, egistec->spi_clk_enabled);
		return ;
	}

	if (en) {
		egistec->spi_clk_enabled = true;
		mt_spi_enable_master_clk(egistec->spi);
	} else {
		egistec->spi_clk_enabled = false;
		mt_spi_disable_master_clk(egistec->spi);
	}
}


#ifdef USE_FB_NOTIFIER
static int egis_fb_notifier_callback(struct notifier_block* self,
		unsigned long event, void* data)
{
	struct egistec_data *egistec = container_of(self, struct egistec_data, fb_notifier);
	struct fb_event* evdata = data;
	unsigned int blank;
	char *envp[2];
	int ret;

	if (event != FB_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
		return 0;
	}

	blank = *(int*)evdata->data;
	switch (blank) {
	case FB_BLANK_UNBLANK:
		envp[0] = "PANEL=1";
		DEBUG_PRINT("EGISTEC %s FB_BLANK_UNBLANK\n", __func__);
		break;

	case FB_BLANK_POWERDOWN:
		envp[0] = "PANEL=0";
		DEBUG_PRINT("EGISTEC %s FB_BLANK_POWERDOWN\n", __func__);
		break;
	}

	envp[1] = NULL;

	ret = kobject_uevent_env(&egistec->pd->dev.kobj, KOBJ_CHANGE, envp);

	return ret;
}
#endif

/*---------Power Control-----------------------------------------------------*/
static void egistec_power_onoff(struct egistec_data *egistec, int power_onoff)
{
	DEBUG_PRINT("[egistec] %s , power_onoff = %d \n", __func__, power_onoff);

	if (power_onoff) {
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_v33_on);
	} else {
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_v33_off);
	}
}
/*-------------------------------------------------------------------------*/

static void egistec_reset(struct egistec_data *egistec)
{
	DEBUG_PRINT("[EGISTEC] %s\n", __func__);
	pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_reset_low);
	mdelay(13);
	pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_reset_high);
	mdelay(12);
}

static void egistec_reset_set(struct egistec_data *egistec, int reset_onoff)
{
	DEBUG_PRINT("[EGISTEC] %s\n", __func__);
	if (reset_onoff) {
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_reset_high);
		mdelay(10);
	} else {
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_reset_low);
		mdelay(10);
	}
}


int egistec_spi_pin(struct egistec_data *egistec, int spi_pulllow)
{
	DEBUG_PRINT("%s spi_pulllow = %d\n", __func__, spi_pulllow);
	if (spi_pulllow) {
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_miso_pulllow);
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_mosi_pulllow);
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_cs_pulllow);
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_clk_pulllow);
	} else {
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_miso_spi);
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_mosi_spi);
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_cs_spi);
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_clk_spi);
	}
	return 0;
}


static void egistec_before_after_using_sensor(struct egistec_data *egistec, int before_after)
{
	DEBUG_PRINT("[EGISTEC] %s\n", __func__);
	if (!before_after) {
		spi_clk_enable(egistec, 1);
		egistec_spi_pin(egistec, 0);
		egistec_power_onoff(egistec, 1);
		egistec_reset(egistec);
	} else {
		spi_clk_enable(egistec, 0);
		egistec_spi_pin(egistec, 1);
		pinctrl_select_state(egistec->pinctrl_gpios, egistec->pins_reset_low);
		egistec_power_onoff(egistec, 0);
		mdelay(50);
	}
}

static ssize_t egistec_read(struct file *filp,
	char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}

static ssize_t egistec_write(struct file *filp,
	const char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}

static long egistec_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	struct egistec_data *egistec;
	struct ioctl_cmd data;

	memset(&data, 0, sizeof(data));
	//printk(KERN_INFO "%s: cmd = 0x%X\n", __func__, cmd);
	egistec = filp->private_data;

	switch (cmd) {
	case FP_SENSOR_RESET:
		DEBUG_PRINT("[EGISTEC] fp_ioctl ioc->opcode == FP_SENSOR_RESET\n");
		egistec_reset(egistec);
		break;

	case FP_RESET_SET:
		if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
			retval = -EFAULT;
			break;
		}
		DEBUG_PRINT("[EGISTEC] fp_ioctl ioc->opcode == FP_RESET_SET\n");
		egistec_reset_set(egistec, data.int_mode);
		break;

	case FP_BEFORE_AFTER_USING_SENSOR:
		if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
			retval = -EFAULT;
			break;
		}
		DEBUG_PRINT("[EGISTEC] fp_ioctl ioc->opcode == FP_BEFORE_AFTER_USING_SENSOR\n");
		egistec_before_after_using_sensor(egistec, data.int_mode);
		break;

	case FP_SPIPIN_SETTING:
		DEBUG_PRINT("[EGISTEC] fp_ioctl <<< FP_SPIPIN_SETTING\n");
		egistec_spi_pin(egistec, 0);
		break;

	case FP_SPIPIN_PULLLOW:
		DEBUG_PRINT("[EGISTEC] fp_ioctl <<< FP_SPIPIN_PULLLOW\n");
		egistec_spi_pin(egistec, 1);
		break;

	case FP_POWER_ONOFF:
		if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
			retval = -EFAULT;
			break;
		}
		egistec_power_onoff(egistec, data.int_mode);  // Use data.int_mode as power setting. 1 = on, 0 = off.
		DEBUG_PRINT("[EGISTEC] %s: egistec_power_onoff = %d\n", __func__, data.int_mode);
		break;

	case FP_WAKELOCK_TIMEOUT_ENABLE: //0Xb1
		DEBUG_PRINT("[EGISTEC] fp_ioctl <<< FP_WAKELOCK_TIMEOUT_ENABLE\n");
		pm_wakeup_event(&egistec->pd->dev, 1000);
		break;

	case FP_WAKELOCK_TIMEOUT_DISABLE: //0Xb2
		DEBUG_PRINT("[EGISTEC] fp_ioctl <<< FP_WAKELOCK_TIMEOUT_DISABLE\n");
		pm_relax(&egistec->pd->dev);
		break;

	case FP_SPICLK_ENABLE:
		//DEBUG_PRINT("[EGISTEC] fp_ioctl <<< FP_SPICLK_ENABLE\n");
		spi_clk_enable(egistec, 1);
		break;

	case FP_SPICLK_DISABLE:
		//DEBUG_PRINT("[EGISTEC] fp_ioctl <<< FP_SPICLK_DISABLE\n");
		spi_clk_enable(egistec, 0);
		break;

	case DELETE_DEVICE_NODE:
		DEBUG_PRINT("[EGISTEC] fp_ioctl <<< DELETE_DEVICE_NODE\n");
		delete_device_node();
		break;

	default:
		DEBUG_PRINT("[EGISTEC] fp_ioctl <<< cmd 0x%X\n", cmd);
		retval = -ENOTTY;
		break;
	}

	if (retval) {
		DEBUG_PRINT("[EGISTEC] %s: retval = %d\n", __func__, retval);
	}

	return retval;
}

#ifdef CONFIG_COMPAT
static long egistec_compat_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	return egistec_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define egistec_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int egistec_open(struct inode *inode, struct file *filp)
{
	struct egistec_data *egistec;
	int status = -ENXIO;
	DEBUG_PRINT("[EGISTEC] %s\n", __func__);

	mutex_lock(&device_list_lock);
	list_for_each_entry(egistec, &device_list, device_entry)
	{
		if (egistec->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	if (status == 0) {
		if (egistec->buffer == NULL) {
			egistec->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (egistec->buffer == NULL) {
				printk(KERN_ERR "%s: open ENOMEM\n", __func__);
				status = -ENOMEM;
			}
		}
		if (status == 0) {
			egistec->users++;
			filp->private_data = egistec;
			nonseekable_open(inode, filp);
		}
	} else {
		printk(KERN_INFO "%s: nothing for minor %d\n" , __func__, iminor(inode));
	}
	mutex_unlock(&device_list_lock);
	return status;
}

static int egistec_release(struct inode *inode, struct file *filp)
{
	struct egistec_data *egistec;
	DEBUG_PRINT("[EGISTEC] %s\n", __func__);

	mutex_lock(&device_list_lock);
	egistec = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	egistec->users--;
	if (egistec->users == 0) {
		int	dofree;

		kfree(egistec->buffer);
		egistec->buffer = NULL;

		/* ... after we unbound from the underlying device? */
		spin_lock_irq(&egistec->spi_lock);
		dofree = (egistec->pd == NULL);
		spin_unlock_irq(&egistec->spi_lock);

		if (dofree)
			kfree(egistec);
	}
	mutex_unlock(&device_list_lock);
	return 0;
}

static int egistec_parse_dt(struct device *dev,
	struct egistec_data *data)
{
#ifdef CONFIG_OF
	int ret;
	struct device_node *node = NULL;
	struct platform_device *pdev = NULL;
	dev_info(dev, "%s: from dts pinctrl\n", __func__);

	node = of_find_compatible_node(NULL, NULL, "mediatek,fingerprint");

	if (node) {
		pdev = of_find_device_by_node(node);
		if (pdev) {
			data->pinctrl_gpios = devm_pinctrl_get(&pdev->dev);
			if (IS_ERR(data->pinctrl_gpios)) {
				ret = PTR_ERR(data->pinctrl_gpios);
				dev_err(dev, "%s: can't find fingerprint pinctrl\n", __func__);
				return ret;
			}
			data->pins_reset_high = pinctrl_lookup_state(data->pinctrl_gpios, "fingerprint_rst_high");
			if (IS_ERR(data->pins_reset_high)) {
				ret = PTR_ERR(data->pins_reset_high);
				dev_err(dev, "%s: can't find fingerprint pinctrl reset_high\n", __func__);
				return ret;
			}
			data->pins_reset_low = pinctrl_lookup_state(data->pinctrl_gpios, "fingerprint_rst_low");
			if (IS_ERR(data->pins_reset_low)) {
				ret = PTR_ERR(data->pins_reset_low);
				dev_err(dev, "%s: can't find fingerprint pinctrl reset_low\n", __func__);
				return ret;
			}
#if 1
			data->pins_v33_on = pinctrl_lookup_state(data->pinctrl_gpios, "fingerprint_pwr_high");
			if (IS_ERR(data->pins_v33_on)) {
				ret = PTR_ERR(data->pins_v33_on);
				dev_err(dev, "%s: can't find fingerprint pinctrl pins_v33_on\n", __func__);
				return ret;
			}
			data->pins_v33_off = pinctrl_lookup_state(data->pinctrl_gpios, "fingerprint_pwr_low");
			if (IS_ERR(data->pins_v33_off)) {
				ret = PTR_ERR(data->pins_v33_off);
				dev_err(dev, "%s: can't find fingerprint pinctrl pins_v33_off\n", __func__);
				return ret;
			}
#endif
			data->pins_mosi_spi = pinctrl_lookup_state(data->pinctrl_gpios, "mosi_spi");
			if (IS_ERR(data->pins_mosi_spi)) {
				ret = PTR_ERR(data->pins_mosi_spi);
				DEBUG_PRINT("%s can't find fingerprint pinctrl pins_mosi_spi\n", __func__);
			return ret;
			}
			data->pins_mosi_pullhigh = pinctrl_lookup_state(data->pinctrl_gpios, "mosi_pullhigh");
			if (IS_ERR(data->pins_mosi_pullhigh)) {
				ret = PTR_ERR(data->pins_mosi_pullhigh);
				DEBUG_PRINT("%s can't find fingerprint pinctrl mosi_pullhigh\n", __func__);
				return ret;
			}
			data->pins_mosi_pulllow = pinctrl_lookup_state(data->pinctrl_gpios, "mosi_pulllow");
			if (IS_ERR(data->pins_mosi_pulllow)) {
				ret = PTR_ERR(data->pins_mosi_pulllow);
				DEBUG_PRINT("%s can't find fingerprint pinctrl mosi_pulllow\n", __func__);
				return ret;
			}
			data->pins_miso_spi = pinctrl_lookup_state(data->pinctrl_gpios, "miso_spi");
			if (IS_ERR(data->pins_miso_spi)) {
				ret = PTR_ERR(data->pins_miso_spi);
				DEBUG_PRINT("%s can't find fingerprint pinctrl pins_miso_spi\n", __func__);
				return ret;
			}
			data->pins_miso_pullhigh = pinctrl_lookup_state(data->pinctrl_gpios, "miso_pullhigh");
			if (IS_ERR(data->pins_miso_pullhigh)) {
				ret = PTR_ERR(data->pins_miso_pullhigh);
				DEBUG_PRINT("%s can't find fingerprint pinctrl miso_pullhigh\n", __func__);
				return ret;
			}
			data->pins_miso_pulllow = pinctrl_lookup_state(data->pinctrl_gpios, "miso_pulllow");
			if (IS_ERR(data->pins_miso_pulllow)) {
				ret = PTR_ERR(data->pins_miso_pulllow);
				DEBUG_PRINT("%s can't find fingerprint pinctrl miso_pulllow\n", __func__);
				return ret;
			}
			data->pins_cs_spi = pinctrl_lookup_state(data->pinctrl_gpios, "cs_spi");
			if (IS_ERR(data->pins_cs_spi)) {
				ret = PTR_ERR(data->pins_cs_spi);
				DEBUG_PRINT("%s can't find fingerprint pinctrl pins_cs_spi\n", __func__);
				return ret;
			}
			data->pins_cs_pullhigh = pinctrl_lookup_state(data->pinctrl_gpios, "cs_pullhigh");
			if (IS_ERR(data->pins_cs_pullhigh)) {
				ret = PTR_ERR(data->pins_cs_pullhigh);
				DEBUG_PRINT("%s can't find fingerprint pinctrl cs_pullhigh\n", __func__);
				return ret;
			}
			data->pins_cs_pulllow = pinctrl_lookup_state(data->pinctrl_gpios, "cs_pulllow");
			if (IS_ERR(data->pins_cs_pulllow)) {
				ret = PTR_ERR(data->pins_cs_pulllow);
				DEBUG_PRINT("%s can't find fingerprint pinctrl cs_pulllow\n", __func__);
				return ret;
			}
			data->pins_clk_spi = pinctrl_lookup_state(data->pinctrl_gpios, "clk_spi");
			if (IS_ERR(data->pins_clk_spi)) {
				ret = PTR_ERR(data->pins_clk_spi);
				DEBUG_PRINT("%s can't find fingerprint pinctrl pins_clk_spi\n", __func__);
				return ret;
			}
			data->pins_clk_pullhigh = pinctrl_lookup_state(data->pinctrl_gpios, "clk_pullhigh");
			if (IS_ERR(data->pins_clk_pullhigh)) {
				ret = PTR_ERR(data->pins_clk_pullhigh);
				DEBUG_PRINT("%s can't find fingerprint pinctrl clk_pullhigh\n", __func__);
				return ret;
			}
			data->pins_clk_pulllow = pinctrl_lookup_state(data->pinctrl_gpios, "clk_pulllow");
			if (IS_ERR(data->pins_clk_pulllow)) {
				ret = PTR_ERR(data->pins_clk_pulllow);
				DEBUG_PRINT("%s can't find fingerprint pinctrl clk_pulllow\n", __func__);
				return ret;
			}

			dev_info(dev, "%s: get pinctrl success!\n", __func__);
		} else {
			dev_err(dev, "%s: platform device is null\n", __func__);
		}
	} else {
		dev_err(dev, "%s: device node is null\n", __func__);
	}
#endif

	return 0;
}

static const struct file_operations egistec_fops = {
	.owner = THIS_MODULE,
	.write = egistec_write,
	.read = egistec_read,
	.unlocked_ioctl = egistec_ioctl,
	.compat_ioctl = egistec_compat_ioctl,
	.open = egistec_open,
	.release = egistec_release,
	.llseek = no_llseek,
};

/*-------------------------------------------------------------------------*/

static struct class *egistec_class;

/*-------------------------------------------------------------------------*/

static int egistec_probe(struct platform_device *pdev);
static int egistec_remove(struct platform_device *pdev);

typedef struct {
	struct spi_device      *spi;
	struct class           *class;
	struct device          *device;
	dev_t                  devno;
	u8                     *huge_buffer;
	size_t                 huge_buffer_size;
	struct input_dev       *input_dev;
} et7xx_data_t;

/* -------------------------------------------------------------------- */

static int et7xx_spi_probe(struct spi_device *spi)
{
//	int retval = 0;
	int error = 0;
	et7xx_data_t *et7xx = NULL;
	/* size_t buffer_size; */
	DEBUG_PRINT("[EGISTEC] %s\n", __func__);

	et7xx = kzalloc(sizeof(*et7xx), GFP_KERNEL);
	if (!et7xx) {
		return -ENOMEM;
	}

	spi_set_drvdata(spi, et7xx);

	g_data->spi = spi;

	DEBUG_PRINT("[EGISTEC] %s successful\n", __func__);

	return error;
}

/* -------------------------------------------------------------------- */
static int et7xx_spi_remove(struct spi_device *spi)
{
	et7xx_data_t *et7xx = spi_get_drvdata(spi);

	kfree(et7xx);

	return 0;
}

static struct spi_driver spi_driver = {
	.driver = {
		.name	= "et7xx",
		.owner	= THIS_MODULE,
		.of_match_table = et7xx_spi_of_match,
		.bus	= &spi_bus_type,
	},
	.probe	= et7xx_spi_probe,
	.remove	= et7xx_spi_remove,
};

struct spi_board_info spi_board_devs[] __initdata = {
	[0] = {
		.modalias="et7xx",
		.bus_num = 1,
		.chip_select=0,
		.mode = SPI_MODE_0,
	},
};

static struct platform_driver egistec_driver = {
	.driver = {
		.name		= "et7xx",
		.owner		= THIS_MODULE,
		.of_match_table = egistec_match_table,
	},
	.probe	= egistec_probe,
	.remove	= egistec_remove,
};

static int egistec_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct egistec_data *egistec = dev_get_drvdata(dev);

	DEBUG_PRINT("[EGISTEC] %s\n", __func__);

#ifdef USE_FB_NOTIFIER
	fb_unregister_client(&egistec->fb_notifier);
#endif

	kfree(egistec);
	return 0;
}

static int egistec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct egistec_data *egistec;
	int status = 0;
	unsigned long minor;
	//bool use_ext_ldo = false;

	DEBUG_PRINT("[EGISTEC] %s: initial\n", __func__);

	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(ET7XX_MAJOR, "et7xx", &egistec_fops);
	if (status < 0) {
		dev_err(dev, "%s: register_chrdev error.\n", __func__);
		return status;
	}

	egistec_class = class_create(THIS_MODULE, "et7xx");
	if (IS_ERR(egistec_class)) {
		dev_err(dev, "%s: class_create error.\n", __func__);
		unregister_chrdev(ET7XX_MAJOR, egistec_driver.driver.name);
		return PTR_ERR(egistec_class);
	}

	/* Allocate driver data */
	egistec = kzalloc(sizeof(*egistec), GFP_KERNEL);
	if (egistec== NULL) {
		dev_err(dev, "%s: Failed to kzalloc\n", __func__);
		return -ENOMEM;
	}

	/* device tree call */
	if (pdev->dev.of_node) {
		status = egistec_parse_dt(&pdev->dev, egistec);
		if (status) {
			dev_err(dev, "%s: Failed to parse DT\n", __func__);
			goto egistec_probe_parse_dt_failed;
		}
	}
	/* Initialize the driver data */
	egistec->pd = pdev;
	g_data = egistec;

	spin_lock_init(&egistec->spi_lock);
	mutex_init(&egistec->buf_lock);
	mutex_init(&device_list_lock);

	INIT_LIST_HEAD(&egistec->device_entry);

	/*
	 * If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *fdev;
		egistec->devt = MKDEV(ET7XX_MAJOR, minor);
		fdev = device_create(egistec_class, &pdev->dev, egistec->devt, egistec, "esfp0");
		status = IS_ERR(fdev) ? PTR_ERR(fdev) : 0;
	} else {
		dev_err(dev, "%s: no minor number available!\n", __func__);
		status = -ENODEV;
	}

	if (status) {
		mutex_unlock(&device_list_lock);
		goto egistec_probe_failed;
	}

	set_bit(minor, minors);
	list_add(&egistec->device_entry, &device_list);

	mutex_unlock(&device_list_lock);

	dev_set_drvdata(dev, egistec);

#ifdef USE_FB_NOTIFIER
	/* Register screen on/off callback. */
	egistec->fb_notifier.notifier_call = egis_fb_notifier_callback;
	status = fb_register_client(&egistec->fb_notifier);
	if (status) {
		DEBUG_PRINT("%s: fb_notifier_register fail %d\n", __func__, status);
		goto egistec_probe_failed;
	}
#endif

	dev_info(dev, "%s: success\n", __func__);
	return status;

egistec_probe_failed:
	device_destroy(egistec_class, egistec->devt);
	class_destroy(egistec_class);

egistec_probe_parse_dt_failed:
	kfree(egistec);
	dev_err(dev, "%s: failed %d\n", __func__, status);
	return status;
}

static void delete_device_node(void)
{
	DEBUG_PRINT("[EGISTEC] %s\n", __func__);
	device_destroy(egistec_class, g_data->devt);
	list_del(&g_data->device_entry);
	class_destroy(egistec_class);
	unregister_chrdev(ET7XX_MAJOR, egistec_driver.driver.name);
	g_data = NULL;
}

static int __init egis7xx_init(void)
{
	int status = 0;

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

	status = platform_driver_register(&egistec_driver);
	if (status < 0) {
		printk(KERN_ERR "%s: egistec platform_driver_register failed\n", __func__);
		return -EINVAL;
	}

	DEBUG_PRINT("[EGISTEC] platform_driver_register OK!\n");

	status = spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs));
	if (status) {
		printk(KERN_ERR "%s: egistec spi_register_board_info failed\n", __func__);
		return -EINVAL;
	}

	if (spi_register_driver(&spi_driver)) {
		printk(KERN_ERR "%s: egistec spi_register_driver failed\n", __func__);
		return -EINVAL;
	}

	DEBUG_PRINT("[EGISTEC] spi_register_driver OK!\n");

	return status;
}

static void __exit egis7xx_exit(void)
{
	platform_driver_unregister(&egistec_driver);
	spi_unregister_driver(&spi_driver);
}

late_initcall(egis7xx_init);
module_exit(egis7xx_exit);

MODULE_AUTHOR("ZQ Chen, <zq.chen@egistec.com>");
MODULE_DESCRIPTION("NWd driver for ET7XX");
MODULE_LICENSE("GPL");
