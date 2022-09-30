#define pr_fmt(fmt) "[ATD_MID] %s: " fmt, __func__

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/kthread.h>
#ifdef CONFIG_LGE_BOOT_MODE
#include <soc/mediatek/lge/lge_boot_mode.h>
#endif

static struct kobject *atd_kobj = NULL;
static struct wakeup_source pwroff_lock;

static struct notifier_block power_supply_nb;
static int cable_con = 0;

static int atd_status = 0;

static ssize_t atd_status_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atd_status);
}

static ssize_t atd_status_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%du", &atd_status);

	pr_info("ATD_STATUS set as %d\n", atd_status);

	return count;
}
static struct kobj_attribute atd_status_attr =
	__ATTR(atd_status, 0660, atd_status_show, atd_status_store);

static int atd_mid_poweroff_thread(void *data)
{
	int loopcnt = 50;

	if (!atd_status)
		return 0;

	while (loopcnt-- > 0) {
		pr_info("waiting ATD power off [%d]. ATD_STATUS: %d, CABLE_CON: %d\n",
				loopcnt, atd_status, cable_con);
		msleep(100);
	}
	pr_err("timeout waiting ATD power off. ATD_STATUS: %d\n", atd_status);

	kernel_power_off();

	return 0;
}

static int atd_mid_update_online(struct power_supply *psy)
{
	static struct power_supply *charger = NULL;
	union power_supply_propval val;
	int ret;

	/* handle only usb / ac */
	switch (psy->desc->type) {
	case POWER_SUPPLY_TYPE_MAINS:
	case POWER_SUPPLY_TYPE_USB_DCP:
	case POWER_SUPPLY_TYPE_USB:
	case POWER_SUPPLY_TYPE_USB_CDP:
		break;
	default:
		return NOTIFY_DONE;
	}

	if (!charger) {
		charger = power_supply_get_by_name("charger");
		if (!charger)
			return NOTIFY_DONE;
	}

	ret = power_supply_get_property(charger,
			POWER_SUPPLY_PROP_ONLINE, &val);
	if (ret)
		return NOTIFY_DONE;
	if (val.intval == cable_con)
		return NOTIFY_DONE;

	cable_con = val.intval;
	if (cable_con)
		return NOTIFY_DONE;

	return NOTIFY_OK;
}

static int atd_mid_psy_notifier_call(struct notifier_block *nb,
				     unsigned long event, void *v)
{
	struct power_supply *psy = (struct power_supply *)v;
	struct task_struct *tsk;
	int ret;

	ret = atd_mid_update_online(psy);
	if (ret != NOTIFY_OK)
		return NOTIFY_DONE;

	if (!atd_status)
		return NOTIFY_DONE;

	__pm_stay_awake(&pwroff_lock);

	pr_info("create ATD power off waiting thread, ATD_STATUS : %d\n", atd_status);

	tsk = kthread_run(atd_mid_poweroff_thread, NULL, "ATD_PWROFF");
	if (IS_ERR(tsk))
		pr_info("error creating ATD power off waiting thread\n");

	return NOTIFY_DONE;
}

static void atd_mid_init_async(void *data, async_cookie_t cookie)
{
	int ret = 0;

#ifdef CONFIG_LGE_BOOT_MODE
	if (!lge_get_factory_boot()) {
		return;
	}
	pr_info("factory boot. prepare atd_mid\n");
#endif

	atd_kobj = kobject_create_and_add("atd_mid", kernel_kobj);
	if (!atd_kobj) {
		pr_err("failed to create the atd_mid kobj\n");
		return;
	}

	ret = sysfs_create_file(atd_kobj, &atd_status_attr.attr);
	if (ret) {
		pr_err("failed to create atd_status sysfs\n");
		goto failed_sysfs;
	}

	/* power supply notifier */
	power_supply_nb.notifier_call = atd_mid_psy_notifier_call;
	ret = power_supply_reg_notifier(&power_supply_nb);
	if (ret) {
		pr_err("failed to register power_supply notifier\n");
		goto failed_power_supply;
	}

	return;

failed_power_supply:
	sysfs_remove_file(atd_kobj, &atd_status_attr.attr);
failed_sysfs:
	kobject_put(atd_kobj);
	atd_kobj = NULL;

	return;
}

static int __init atd_mid_init(void)
{
	wakeup_source_init(&pwroff_lock, "ATD_PWROFF");

	async_schedule(atd_mid_init_async, NULL);

	return 0;
}

static void __exit atd_mid_exit(void)
{
	wakeup_source_trash(&pwroff_lock);

	if (!atd_kobj)
		return;

	sysfs_remove_file(atd_kobj, &atd_status_attr.attr);
	kobject_put(atd_kobj);
}

module_init(atd_mid_init);
module_exit(atd_mid_exit);
MODULE_DESCRIPTION("LGE ATD STATE driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
