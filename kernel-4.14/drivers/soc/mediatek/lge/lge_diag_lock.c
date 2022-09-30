#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#define DIAG_DISABLE 0
#define DIAG_ENABLE 1

int user_diag_enable	= DIAG_ENABLE;

int get_diag_enable(void)
{
	return user_diag_enable;
}
EXPORT_SYMBOL(get_diag_enable);

int set_diag_enable(int enable)
{
	if (enable == DIAG_ENABLE)
		user_diag_enable = DIAG_ENABLE;
	else
		user_diag_enable = DIAG_DISABLE;

	return 0;
}
EXPORT_SYMBOL(set_diag_enable);

static int __init get_diag_enable_cmdline(char *diag_enable)
{
	if(!strcmp(diag_enable,"true"))
		user_diag_enable = DIAG_ENABLE;
	else
		user_diag_enable = DIAG_DISABLE;

	pr_info("[%s] read cmdline value = %d\n", __func__, user_diag_enable);

	return 1;
}
__setup("usb.diag_enable=", get_diag_enable_cmdline);

static ssize_t read_diag_enable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", user_diag_enable);

	return ret;
}

static ssize_t write_diag_enable(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned char string[2];

	if (sscanf(buf, "%s", string) != 1)
		return -EINVAL;

	if (!strncmp(string, "1", 1))
		user_diag_enable = DIAG_ENABLE;
	else
		user_diag_enable = DIAG_DISABLE;

	pr_err("[%s] diag_enable: %d\n", __func__, user_diag_enable);

	return size;
}

static DEVICE_ATTR(diag_enable, S_IRUGO|S_IWUSR, read_diag_enable, write_diag_enable);

static int lg_diag_cmd_probe(struct platform_device *pdev)
{
	int ret;
	ret = device_create_file(&pdev->dev, &dev_attr_diag_enable);
	if (ret) {
		device_remove_file(&pdev->dev, &dev_attr_diag_enable);
		return ret;
	}
	return ret;
}

static int lg_diag_cmd_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_diag_enable);

	return 0;
}

static struct platform_driver lg_diag_cmd_driver = {
	.probe		= lg_diag_cmd_probe,
	.remove		= lg_diag_cmd_remove,
	.driver		= {
		.name = "lg_diag_cmd",
		.owner	= THIS_MODULE,
	},
};

static struct platform_device lg_diag_cmd_device = {
	.name = "lg_diag_cmd",
	.id = -1,
	.dev    = {
		.platform_data = 0, /* &lg_diag_cmd_pdata */
	},
};

static int __init lg_diag_cmd_init(void)
{
	int rc;

	rc	= platform_device_register(&lg_diag_cmd_device);
	if (rc) {
		pr_err("%s: platform_device_register fail\n", __func__);
		return rc;
	}

	rc	= platform_driver_register(&lg_diag_cmd_driver);
	if (rc) {
		pr_err("%s: platform_driver_register fail\n", __func__);
		platform_device_unregister(&lg_diag_cmd_device);
		return rc;
	}

	pr_err("%s: registered\n", __func__);
	return 0;
}

static void __exit lg_diag_cmd_exit(void)
{
	platform_device_unregister(&lg_diag_cmd_device);
	platform_driver_unregister(&lg_diag_cmd_driver);

	pr_err("%s: unregistered\n", __func__);
}

module_init(lg_diag_cmd_init);
module_exit(lg_diag_cmd_exit);
MODULE_DESCRIPTION("LGE DIAG CMD Driver");
MODULE_LICENSE("GPL");
