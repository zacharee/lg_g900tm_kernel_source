#define pr_fmt(fmt) "[%s] %s: " fmt, KBUILD_MODNAME, __func__

#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/bug.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <mt-plat/mtk_boot_common.h>
#include <mt-plat/mtk_thermal_monitor.h>
#include <mt-plat/mtk_thermal_platform.h>
#include <linux/uidgid.h>
#include <linux/power_supply.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <mt-plat/lge_vts_monitor.h>

#define VTS_RW_PERM (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

/* thermal sensor matching */
struct tz_id_match_table {
	char *name;
	enum mtk_thermal_sensor_id id;
};

static const struct tz_id_match_table tz_id_match_table[] = {
	/* MTK_THERMAL_SENSOR_CPU */
	{ "cpu", MTK_THERMAL_SENSOR_CPU },
	{ "cpu_therm", MTK_THERMAL_SENSOR_CPU },
	{ "mtktscpu", MTK_THERMAL_SENSOR_CPU },
	/* MTK_THERMAL_SENSOR_ABB */
	{ "abb", MTK_THERMAL_SENSOR_ABB },
	{ "mtktsabb", MTK_THERMAL_SENSOR_ABB },
	/* MTK_THERMAL_SENSOR_PMIC */
	{ "pmic", MTK_THERMAL_SENSOR_PMIC },
	{ "pmic_therm", MTK_THERMAL_SENSOR_PMIC },
	{ "mtktspmic", MTK_THERMAL_SENSOR_PMIC },
	/* MTK_THERMAL_SENSOR_BATTERY */
	{ "battery", MTK_THERMAL_SENSOR_BATTERY },
	{ "battery_therm", MTK_THERMAL_SENSOR_BATTERY },
	{ "mtktsbattery", MTK_THERMAL_SENSOR_BATTERY },
	/* MTK_THERMAL_SENSOR_MD1 */
	{ "modem", MTK_THERMAL_SENSOR_MD1 },
	{ "modem1", MTK_THERMAL_SENSOR_MD1 },
	{ "mtktspa", MTK_THERMAL_SENSOR_MD1 },
	/* MTK_THERMAL_SENSOR_MD2 */
	{ "modem2", MTK_THERMAL_SENSOR_MD2 },
	{ "mtktstdpa", MTK_THERMAL_SENSOR_MD2 },
	/* MTK_THERMAL_SENSOR_WIFI */
	{ "wifi", MTK_THERMAL_SENSOR_WIFI },
	{ "mtktswmt", MTK_THERMAL_SENSOR_WIFI },
	/* MTK_THERMAL_SENSOR_BATTERY2 */
	{ "battery2", MTK_THERMAL_SENSOR_BATTERY2 },
	{ "mtktsbattery2", MTK_THERMAL_SENSOR_BATTERY2 },
	/* MTK_THERMAL_SENSOR_BUCK */
	{ "buck", MTK_THERMAL_SENSOR_BUCK },
	{ "mtktsbuck", MTK_THERMAL_SENSOR_BUCK },
	/* MTK_THERMAL_SENSOR_AP */
	{ "board", MTK_THERMAL_SENSOR_AP },
	{ "board_therm", MTK_THERMAL_SENSOR_AP },
	{ "mtktsAP", MTK_THERMAL_SENSOR_AP },
	/* MTK_THERMAL_SENSOR_PCB1 */
	{ "pcb1", MTK_THERMAL_SENSOR_PCB1 },
	{ "mtktspcb1", MTK_THERMAL_SENSOR_PCB1 },
	/* MTK_THERMAL_SENSOR_PCB2 */
	{ "pcb2", MTK_THERMAL_SENSOR_PCB2 },
	{ "mtktspcb2", MTK_THERMAL_SENSOR_PCB2 },
	/* MTK_THERMAL_SENSOR_SKIN */
	{ "skin", MTK_THERMAL_SENSOR_SKIN },
	{ "mtktsskin", MTK_THERMAL_SENSOR_SKIN },
	/* MTK_THERMAL_SENSOR_XTAL */
	{ "xo", MTK_THERMAL_SENSOR_XTAL },
	{ "xo_therm", MTK_THERMAL_SENSOR_XTAL },
	{ "mtktsxtal", MTK_THERMAL_SENSOR_XTAL },
	/* MTK_THERMAL_SENSOR_MD_PA */
	{ "pa", MTK_THERMAL_SENSOR_MD_PA },
	{ "pa_therm", MTK_THERMAL_SENSOR_MD_PA },
	{ "mtktsbtsmdpa", MTK_THERMAL_SENSOR_MD_PA },
	/* LGE_THERMAL_SENSOR_QUIET */
	{ "quiet", LGE_THERMAL_SENSOR_QUIET },
	{ "quiet_therm", LGE_THERMAL_SENSOR_QUIET },
	/* LGE_THERMAL_SENSOR_SKIN */
	{ "vts", LGE_THERMAL_SENSOR_SKIN },
	{ "skin_vts", LGE_THERMAL_SENSOR_SKIN },
	{ "vts_skin", LGE_THERMAL_SENSOR_SKIN },
	/* LGE_THERMAL_SENSOR_BATTERY */
	{ "battery_vts", LGE_THERMAL_SENSOR_BATTERY },
	{ "vts_battery", LGE_THERMAL_SENSOR_BATTERY },
};

enum mtk_thermal_sensor_id lge_vts_get_thermal_sensor_id(char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tz_id_match_table); i++) {
		if (!strcmp(name, tz_id_match_table[i].name))
			return tz_id_match_table[i].id;
	}

	return -1;
}
EXPORT_SYMBOL(lge_vts_get_thermal_sensor_id);

char *lge_vts_get_thermal_sensor_name(enum mtk_thermal_sensor_id id)
{
	int i;

	if (id < 0)
		return "no-tz";

	for (i = 0; i < ARRAY_SIZE(tz_id_match_table); i++) {
		if (id == tz_id_match_table[i].id)
			return tz_id_match_table[i].name;
	}

	return "no-tz";
}
EXPORT_SYMBOL(lge_vts_get_thermal_sensor_name);

/* cmode */
static bool vts_cmode_enabled = false;
static bool vts_display_on = false;
static bool vts_fastchg = false;
static struct delayed_work vts_cmode;
static void vts_cmode_update(struct work_struct *work)
{
	bool cmode = false;

#ifdef CONFIG_MTK_BOOT
	if (get_boot_mode() != NORMAL_BOOT)
		return;
#endif

	/* cmode condition : display off & fast charging */
	if (!vts_display_on && vts_fastchg)
		cmode = true;

	if (vts_cmode_enabled != cmode) {
		vts_cmode_enabled = cmode;
		pr_info("cmode %s\n", vts_cmode_enabled
				? "enabled" : "disabled");
	}
}

bool lge_vts_cmode_enabled(void)
{
	return vts_cmode_enabled;
}
EXPORT_SYMBOL(lge_vts_cmode_enabled);

/* notifiers */
static int vts_monitor_fb_notifier_call(struct notifier_block *nb,
					unsigned long event, void *v)
{
	struct fb_event *ev = (struct fb_event *)v;
	bool display_on = false;

	if (event != FB_EVENT_BLANK)
		return NOTIFY_DONE;

	if (!ev || !ev->data)
		return NOTIFY_DONE;

	if (*(int*)ev->data == FB_BLANK_UNBLANK)
		display_on = true;

	if (vts_display_on == display_on)
		return NOTIFY_DONE;

	vts_display_on = display_on;

	schedule_delayed_work(&vts_cmode, 0);

	return NOTIFY_DONE;
}

static struct notifier_block vts_monitor_fb_nb = {
	.notifier_call = vts_monitor_fb_notifier_call,
};

static int vts_monitor_psy_notifier_call(struct notifier_block *nb,
					 unsigned long event, void *v)
{
	static struct power_supply *charger = NULL;
	struct power_supply *psy = (struct power_supply *)v;
	union power_supply_propval val;
	bool fastchg;
	int ret;

	if (strcmp(psy->desc->name, "ac")
			&& strcmp(psy->desc->name, "usb"))
		return NOTIFY_DONE;

	if (!charger) {
		charger = power_supply_get_by_name("charger");
		if (!charger)
			return NOTIFY_DONE;
	}

	ret = power_supply_get_property(charger,
			POWER_SUPPLY_PROP_FASTCHG, &val);
	if (ret)
		return NOTIFY_DONE;

	fastchg = val.intval ? true : false;
	if (fastchg == vts_fastchg)
		return NOTIFY_DONE;

	vts_fastchg = fastchg;
	schedule_delayed_work(&vts_cmode, 0);

	return NOTIFY_DONE;
}

static struct notifier_block vts_monitor_psy_nb = {
	.notifier_call = vts_monitor_psy_notifier_call,
};

/* monitor */
static int vts_monitor_interval = (60 * 1000);	/* default 1 minute */
module_param_named(interval, vts_monitor_interval, int, VTS_RW_PERM);

static int vts_monitor_get_temp(enum mtk_thermal_sensor_id id)
{
	int temp = mtk_thermal_get_temp(id);

	if (temp == -127000)
		return temp;

	if (id == LGE_THERMAL_SENSOR_SKIN
			|| id == LGE_THERMAL_SENSOR_BATTERY)
		return temp;

	/* if sensor is not vts, convert unit */
	return temp / 100;
}

static void vts_monitor_info(struct work_struct *work)
{
	int cpu = vts_monitor_get_temp(MTK_THERMAL_SENSOR_CPU);
	int battery = vts_monitor_get_temp(MTK_THERMAL_SENSOR_BATTERY);
	int board = vts_monitor_get_temp(MTK_THERMAL_SENSOR_AP);
	int quiet = vts_monitor_get_temp(LGE_THERMAL_SENSOR_QUIET);
	int skin = vts_monitor_get_temp(LGE_THERMAL_SENSOR_SKIN);

	pr_info("CPU( %d ) BOARD( %d ) BATTRY( %d ) QUIET( %d ) SKIN( %d )\n",
		cpu, board, battery, quiet, skin);

	schedule_delayed_work(to_delayed_work(work),
			msecs_to_jiffies(vts_monitor_interval));
}

static struct delayed_work vts_monitor;
static int __init vts_monitor_init(void)
{
	INIT_DELAYED_WORK(&vts_cmode, vts_cmode_update);
	fb_register_client(&vts_monitor_fb_nb);
	power_supply_reg_notifier(&vts_monitor_psy_nb);

	INIT_DELAYED_WORK(&vts_monitor, vts_monitor_info);
	schedule_delayed_work(&vts_monitor,
			msecs_to_jiffies(vts_monitor_interval));

	return 0;
}

static void __exit vts_monitor_exit(void)
{
	return;
}
module_init(vts_monitor_init);
module_exit(vts_monitor_exit);
