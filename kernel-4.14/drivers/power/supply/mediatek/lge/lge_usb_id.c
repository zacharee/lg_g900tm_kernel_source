#define pr_fmt(fmt) "[USB_ID] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/reboot.h>
#include <mt-plat/charger_class.h>

/* PD */
#include <tcpm.h>
#ifdef CONFIG_MTK_BOOT
#include <mt-plat/mtk_boot_common.h>
#endif

#ifdef CONFIG_MACH_LGE
#include <soc/mediatek/lge/board_lge.h>
#endif
#ifdef CONFIG_LGE_HANDLE_PANIC
#include <soc/mediatek/lge/lge_handle_panic.h>
#endif
#ifdef CONFIG_LGE_USB_SBU_SWITCH
#include <soc/mediatek/lge/lge_sbu_switch.h>
#endif
#include <linux/power/lge_usb_id.h>
#ifdef CONFIG_LGE_USB_MOISTURE_FUSB251
#include <linux/fusb251.h>
#endif

struct usb_id_table {
	int adc_min;
	int adc_max;
	usb_cable_type type;
};

struct lge_usb_id {
	struct device *dev;
	struct charger_device *chgdev;

	struct work_struct update_work;
	struct work_struct reboot_work;
	struct mutex lock;

	int type;
	int voltage;

	struct notifier_block psy_nb;
	int online;

	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	bool typec_debug;

	struct timespec time_disconnect;
	bool usb_configured;

	/* device configuration */
	unsigned int transition_delay;
	unsigned int delay;

	/* options */
	bool embedded_battery;
	int debounce;

	struct usb_id_table *table;
	int table_size;
};

static struct lge_usb_id *g_chip = NULL;

/* CAUTION: These strings are come from LK. */
static char *cable_str[] = {
	" "," "," "," "," "," ",
	"LT_56K",
	"LT_130K",
	"400MA",
	"DTC_500MA",
	"Abnormal_400MA",
	"LT_910K",
	"NO_INIT",
};

/* boot cable id inforamtion */
static usb_cable_type lge_boot_cable = NO_INIT_CABLE;

usb_cable_type lge_get_board_cable(void)
{
	return	lge_boot_cable;
}
EXPORT_SYMBOL(lge_get_board_cable);

bool lge_is_factory_cable_boot(void)
{
	switch (lge_boot_cable) {
	case LT_CABLE_56K:
	case LT_CABLE_130K:
	case LT_CABLE_910K:
		return true;
	default:
		return false;
	}
}
EXPORT_SYMBOL(lge_is_factory_cable_boot);

/* runtime cable id inforamtion */
int lge_get_cable_voltage(void)
{
	if (!g_chip) {
		pr_err("not ready\n");
		return 0;
	}

	return g_chip->voltage;
}
EXPORT_SYMBOL(lge_get_cable_voltage);

int lge_get_cable_value(void)
{
	if (!g_chip) {
		pr_err("not ready\n");
		return 0;
	}

	switch (g_chip->type) {
	case LT_CABLE_56K:
		return 56;
	case LT_CABLE_130K:
		return 130;
	case LT_CABLE_910K:
		return 910;
	default:
		return 0;
	}
}
EXPORT_SYMBOL(lge_get_cable_value);

bool lge_is_factory_cable(void)
{
	if (!g_chip) {
		pr_err("not ready\n");
		return 0;
	}

	switch (g_chip->type) {
	case LT_CABLE_56K:
	case LT_CABLE_130K:
	case LT_CABLE_910K:
		return true;
	default:
		return false;
	}
}
EXPORT_SYMBOL(lge_is_factory_cable);

usb_cable_type lge_get_cable_type(void)
{
	if (!g_chip) {
		pr_err("not ready\n");
		return 0;
	}

	/* Remain type log temp.*/
	pr_err("%d\n", g_chip->type);
	return g_chip->type;
}
EXPORT_SYMBOL(lge_get_cable_type);

void lge_usb_id_set_usb_configured(bool configured)
{
	struct lge_usb_id *chip = g_chip;

	if (!chip) {
		pr_err("not ready\n");
		return;
	}

	if (!configured)
		return;

	if (chip->usb_configured)
		return;
	chip->usb_configured = true;

	if (!chip->embedded_battery)
		return;

	switch (chip->type) {
	case LT_CABLE_56K:
	case LT_CABLE_910K:
		schedule_work(&chip->reboot_work);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(lge_usb_id_set_usb_configured);

static int usb_id_get_voltage(struct lge_usb_id *chip, int *mV)
{
	int uV = 1800000;
	int ret;

	ret = charger_dev_get_adc(chip->chgdev, ADC_CHANNEL_USBID, &uV, &uV);
	if (ret < 0) {
		pr_err("failed to get usb-id adc (%d)\n", ret);
		return ret;
	}

	*mV = uV / 1000;

	return ret;
}

#define MAX_USB_VOLTAGE_COUNT 4
#define CABLE_VOLTAGE_DIFFERENCE 100
#define NORMAL_CABLE_VOLTAGE 1800
static int lge_read_factory_cable_voltage(struct lge_usb_id *chip, int *voltage)
{
	bool normal_case = false;
	int i = 0, cable_voltage = 0;
	int cable_voltage_data[2] = {0};

	do {
		if (i != 0) msleep(10);

		if (usb_id_get_voltage(chip, &cable_voltage) < 0)
			return -1;
		cable_voltage_data[0] = cable_voltage;

		msleep(20);

		if (usb_id_get_voltage(chip, &cable_voltage) < 0)
			return -1;

		cable_voltage_data[1] = cable_voltage;

		if (abs(cable_voltage_data[1] - cable_voltage_data[0])
				< CABLE_VOLTAGE_DIFFERENCE) {
			normal_case = true;
			break;
		}
	} while (!normal_case && (++i < MAX_USB_VOLTAGE_COUNT));

	*voltage = cable_voltage;

	return 0;
}

static int lge_read_check_cable_voltage(struct lge_usb_id *chip, int *voltage)
{
	bool abnormal_cable = false;
	int i = 0, j = 0, cable_voltage = 0;
	int cable_voltage_data[MAX_USB_VOLTAGE_COUNT] = {0};

	do {
		if (i != 0) msleep(10);

		if (usb_id_get_voltage(chip, &cable_voltage) < 0)
			return -1;

		cable_voltage_data[i] = cable_voltage;

		for (j = 1; j < i + 1; j++) {
			/* Assume that the cable is normal when the differences are over 100 mV */
			if (abs(cable_voltage_data[i] - cable_voltage_data[i-j])
					> CABLE_VOLTAGE_DIFFERENCE) {
				abnormal_cable = true;
				cable_voltage = NORMAL_CABLE_VOLTAGE;
				break;
			}
		}
	} while (!abnormal_cable && (++i < MAX_USB_VOLTAGE_COUNT));

	*voltage = cable_voltage;

	return 0;
}

static int usb_id_read_voltage(struct lge_usb_id *chip, int *voltage)
{
	if (!chip->embedded_battery)
		return usb_id_get_voltage(chip, voltage);

	/* embedded battery */
	if (lge_is_factory_cable_boot())
		return lge_read_factory_cable_voltage(chip, voltage);

	return lge_read_check_cable_voltage(chip, voltage);
}

static void usb_id_read_pre(struct lge_usb_id *chip)
{
	struct pinctrl *pinctrl;
	int ret;

#ifdef CONFIG_LGE_USB_SBU_SWITCH
	lge_sbu_switch_enable(LGE_SBU_MODE_USBID, true);
#endif
#ifdef CONFIG_LGE_USB_MOISTURE_FUSB251
	fusb251_set_factory_mode(true);
#endif

	pinctrl = devm_pinctrl_get_select(chip->dev, "transition");
	if (!IS_ERR(pinctrl) && chip->transition_delay)
		msleep(chip->transition_delay);

	devm_pinctrl_get_select(chip->dev, "adc");

	/* Pull high usb idpin */
	ret = charger_dev_set_usbid_src_ton(chip->chgdev, 0);
	if (ret < 0) {
		pr_err("usbid always src on fail\n");
		return;
	}

	ret = charger_dev_set_usbid_rup(chip->chgdev, 500000);
	if (ret < 0) {
		pr_err("usbid rup500k fail\n");
		return;
	}

	ret = charger_dev_enable_usbid(chip->chgdev, true);
	if (ret < 0) {
		pr_err("usbid pulled high fail\n");
		return;
	}

	/* wait for adc voltage stabilized */
	if (chip->delay)
		msleep(chip->delay);
}

static void usb_id_read_post(struct lge_usb_id *chip)
{
	struct pinctrl *pinctrl;

	charger_dev_enable_usbid_floating(chip->chgdev, true);
	charger_dev_enable_usbid(chip->chgdev, false);

	pinctrl = devm_pinctrl_get_select(chip->dev, "transition");
	if (!IS_ERR(pinctrl) && chip->transition_delay)
		msleep(chip->transition_delay);

	devm_pinctrl_get_select(chip->dev, "default");

#ifdef CONFIG_LGE_USB_SBU_SWITCH
	lge_sbu_switch_enable(LGE_SBU_MODE_USBID, false);
	if (chip->type == USB_CABLE_400MA)
		lge_sbu_switch_enable(LGE_SBU_MODE_UART, true);
#endif
#ifdef CONFIG_LGE_USB_MOISTURE_FUSB251
	fusb251_set_factory_mode(false);
#endif

}

static int usb_id_find_type(struct lge_usb_id *chip, int adc)
{
	int i;

	/* if valid table not exist, just return as normal */
	if (!chip->table || !chip->table_size)
		return USB_CABLE_400MA;

	for (i = 0; i < chip->table_size; i++) {
		/* found matched cable id */
		if (adc >= chip->table[i].adc_min
				&& adc <= chip->table[i].adc_max) {
			return chip->table[i].type;
		}
	}

	return -EINVAL;
}

static bool usb_id_is_cable_valid(struct lge_usb_id *chip)
{
	if (!chip->online)
		return false;

	/* do not read adc if cable is not for debug */
	if (chip->tcpc && !chip->typec_debug)
		return false;

	return true;
}

static void usb_id_update(struct work_struct *work)
{
	struct lge_usb_id *chip = container_of(work, struct lge_usb_id,
			update_work);
	int cnt, retry_cnt = 3;
	int voltage, type;
	int ret;

	mutex_lock(&chip->lock);

	if (!usb_id_is_cable_valid(chip)) {
		chip->type = NO_INIT_CABLE;
		chip->voltage = 0;
		goto out_update;
	}

	usb_id_read_pre(chip);

	for (cnt = 0; cnt < retry_cnt; cnt++) {
		if (!usb_id_is_cable_valid(chip)) {
			chip->type = NO_INIT_CABLE;
			chip->voltage = 0;
			break;
		}

		ret = usb_id_read_voltage(chip, &voltage);
		if (ret) {
			msleep(50);
			continue;
		}

		pr_info("usb id voltage = %dmV\n", voltage);

		type = usb_id_find_type(chip, voltage);
		if (type < 0) {
			msleep(50);
			continue;
		}

		/* found type. exit loop */
		pr_info("usb id = %s\n", cable_str[type]);
		chip->type = type;
		chip->voltage = voltage;

		break;
	}

	usb_id_read_post(chip);

out_update:
	mutex_unlock(&chip->lock);

	if (chip->embedded_battery)
		schedule_work(&chip->reboot_work);

	return;
}

static void usb_id_reboot(struct work_struct *work)
{
	struct lge_usb_id *chip = container_of(work, struct lge_usb_id,
			reboot_work);
	struct timespec time_now;
	struct timespec time_diff;

	get_monotonic_boottime(&time_now);

	/* mark disconnect time */
	if (!chip->online) {
		chip->time_disconnect = time_now;
		return;
	}

	switch (chip->type) {
	case LT_CABLE_56K:
#ifdef CONFIG_MTK_BOOT
		/* do not reboot except normal boot */
		if (get_boot_mode() != NORMAL_BOOT)
			break;
#endif
		if (lge_is_factory_cable_boot())
			break;

		pr_info("[FACTORY] PIF_56K detected in NORMAL BOOT, reboot!!\n");
		/* wait for usb configuration */
		msleep(500);
		kernel_restart("LGE Reboot by PIF 56k");
		break;
	case LT_CABLE_910K:
#ifdef CONFIG_MTK_BOOT
		/* do not reboot in recovery boot */
		if (get_boot_mode() == RECOVERY_BOOT)
			break;
#endif
#ifdef CONFIG_MACH_LGE
		/* do not reboot in laf mode */
		if (lge_get_laf_mode())
			break;
#endif

		if (lge_boot_cable == LT_CABLE_910K) {
			/* do not reboot before plug-out */
			if (!chip->time_disconnect.tv_sec)
				break;
			/* do not reboot if usb not configured yet */
			if (!chip->usb_configured)
				break;
		}

		/* do not reboot if cable re-plugged too fast */
		time_diff = timespec_sub(time_now, chip->time_disconnect);
		if (time_diff.tv_sec <= chip->debounce)
			break;

		pr_info("[FACTORY] PIF_910K detected, reboot!!\n");
		/* wait for usb configuration */
		msleep(500);
#ifdef CONFIG_LGE_HANDLE_PANIC
		lge_set_reboot_reason(LGE_REBOOT_REASON_DLOAD);
#endif
		kernel_restart("LGE Reboot by PIF 910k");
		break;
	default:
		break;
	}
}

static int usb_id_psy_notifier_call(struct notifier_block *nb,
				      unsigned long event, void *v)
{
	struct lge_usb_id *chip = container_of(nb, struct lge_usb_id, psy_nb);
	struct power_supply *psy = (struct power_supply*)v;
	union power_supply_propval val;
	int ret;

	/* handle only usb */
	switch (psy->desc->type) {
	case POWER_SUPPLY_TYPE_USB:
	case POWER_SUPPLY_TYPE_USB_CDP:
		break;
	default:
		return NOTIFY_DONE;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
	if (ret)
		return NOTIFY_DONE;

	if (val.intval == chip->online)
		return NOTIFY_DONE;

	chip->online = val.intval;
	schedule_work(&chip->update_work);

	return NOTIFY_DONE;
}

#ifdef CONFIG_TCPC_CLASS
static int usb_id_pd_tcp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct lge_usb_id *chip = container_of(nb, struct lge_usb_id, pd_nb);
	struct tcp_notify *noti = data;

	if (event != TCP_NOTIFY_TYPEC_STATE)
		return NOTIFY_OK;

	switch (noti->typec_state.new_state) {
	case TYPEC_ATTACHED_DEBUG:
	case TYPEC_ATTACHED_DBGACC_SNK:
	case TYPEC_ATTACHED_CUSTOM_SRC:
		if (!chip->typec_debug) {
			chip->typec_debug = true;
			pr_info("debug accessory attached\n");
		}

		break;
	default:
		if (chip->typec_debug) {
			pr_info("debug accessory dettached\n");
			chip->typec_debug = false;
		}
		break;
	}

#ifdef CONFIG_LGE_USB_SBU_SWITCH
	if (!chip->typec_debug) {
		lge_sbu_switch_enable(LGE_SBU_MODE_USBID, false);
		lge_sbu_switch_enable(LGE_SBU_MODE_UART, false);
	}
#endif

	return NOTIFY_OK;
}
#endif

/*
 * AT%USBIDADC
 * - output : adc,id
 * - /proc/lge_power/testmode/usb_id
 */
#define USB_ID_ATCMD_RO_PERM (S_IRUSR | S_IRGRP | S_IROTH)
static int param_get_atcmd_usb_id(char *buffer, const struct kernel_param *kp)
{
	int adc, id;

	if (!g_chip)
		return -ENODEV;

	adc = lge_get_cable_voltage();
	id = lge_get_cable_value();

	return scnprintf(buffer, PAGE_SIZE, "%d,%d", adc, id);
}
static struct kernel_param_ops atcmd_usb_id = {
	.get = param_get_atcmd_usb_id,
};
module_param_cb(atcmd_usb_id, &atcmd_usb_id, NULL, USB_ID_ATCMD_RO_PERM);

static int param_get_usb_id(char *buffer, const struct kernel_param *kp)
{
	int voltage, ret;

	if (!g_chip)
		return -ENODEV;

	mutex_lock(&g_chip->lock);

	usb_id_read_pre(g_chip);

	ret = usb_id_read_voltage(g_chip, &voltage);
	if (ret)
		voltage = 0;

	usb_id_read_post(g_chip);

	mutex_unlock(&g_chip->lock);

	return scnprintf(buffer, PAGE_SIZE, "%d", voltage * 1000);
}
static struct kernel_param_ops usb_id = {
	.get = param_get_usb_id,
};
module_param_cb(usb_id, &usb_id, NULL, USB_ID_ATCMD_RO_PERM);

static void usb_id_dump_info(struct lge_usb_id *chip)
{
	int i;

	pr_info("delay = %d ms\n", chip->delay);
	if (chip->embedded_battery) {
		pr_info("embedded battery mode with %d sec debounce\n",
				chip->debounce);
	}

	pr_info("%s mode\n", chip->tcpc ? "type-c" : "type-b");
	for (i = 0; i < chip->table_size; i++) {
		pr_info("%s = %dmV to %dmV\n", cable_str[chip->table[i].type],
			chip->table[i].adc_min, chip->table[i].adc_max);
	}
}

static int usb_id_parse_dt(struct lge_usb_id *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct property *prop = NULL;
	const __be32 *data = NULL;
	int size;
	int ret;
	int i;

	ret = of_property_read_u32(node, "delay", &chip->delay);
	if (ret)
		chip->delay = 0;

	ret = of_property_read_u32(node, "transition-delay",
			&chip->transition_delay);
	if (ret)
		chip->transition_delay = 0;

	chip->embedded_battery = of_property_read_bool(node, "embedded-battery");
	ret = of_property_read_u32(node, "debounce", &chip->debounce);
	if (ret)
		chip->debounce = 5;	/* default : 5sec */

	prop = of_find_property(node, "range", &size);
	if (!prop)
		return -ENODATA;

	/* invalid data size */
	if (!size || size % sizeof(struct usb_id_table))
		return -EINVAL;

	chip->table_size = size / sizeof(struct usb_id_table);
	chip->table = (struct usb_id_table *)devm_kzalloc(chip->dev, size,
			GFP_KERNEL);
	if (!chip->table)
		return -ENOMEM;

	for (i = 0; i < chip->table_size; i++) {
		data = of_prop_next_u32(prop, data, &chip->table[i].adc_min);
		data = of_prop_next_u32(prop, data, &chip->table[i].adc_max);
		data = of_prop_next_u32(prop, data, &chip->table[i].type);
		if (chip->table[i].type < LT_CABLE_56K
				|| chip->table[i].type > NO_INIT_CABLE)
			return -EINVAL;
	}

	return 0;
}

static int usb_id_probe(struct platform_device *pdev)
{
	struct lge_usb_id *chip = NULL;
	int ret = 0;

	chip = devm_kzalloc(&pdev->dev, sizeof(struct lge_usb_id),
			GFP_KERNEL);
	if (!chip) {
		pr_err("failed to alloc memory\n");
		return -ENOMEM;
	}
	chip->dev = &pdev->dev;
	platform_set_drvdata(pdev, chip);

	mutex_init(&chip->lock);
	INIT_WORK(&chip->update_work, usb_id_update);
	INIT_WORK(&chip->reboot_work, usb_id_reboot);

	ret = usb_id_parse_dt(chip);
	if (ret) {
		pr_err("failed to parse device-tree\n");
		return ret;
	}

	chip->chgdev = get_charger_by_name("primary_chg");
	if (!chip->chgdev)
		return -EPROBE_DEFER;

#ifdef CONFIG_TCPC_CLASS
	chip->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!chip->tcpc)
		return -EPROBE_DEFER;

	chip->pd_nb.notifier_call = usb_id_pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(chip->tcpc, &chip->pd_nb,
					TCP_NOTIFY_TYPE_USB);
#endif

	/* power supply notifier */
	chip->psy_nb.notifier_call = usb_id_psy_notifier_call;
	ret = power_supply_reg_notifier(&chip->psy_nb);
	if (ret) {
		pr_err("failed to register notifier for power_supply\n");
		return ret;
	}

	usb_id_dump_info(chip);

	g_chip = chip;

	return 0;
}

static int usb_id_remove(struct platform_device *pdev)
{
	struct lge_usb_id *chip = platform_get_drvdata(pdev);

	power_supply_unreg_notifier(&chip->psy_nb);

	return 0;
}

static struct of_device_id usb_id_match_table[] = {
	{
		.compatible = "lge,usb-id",
	},
	{},
};

static struct platform_driver usb_id_driver = {
	.probe = usb_id_probe,
	.remove = usb_id_remove,
	.driver = {
		.name = "usb-id",
		.owner = THIS_MODULE,
		.of_match_table = usb_id_match_table,
	},
};

static int __init boot_cable_setup(char *cable_info)
{
	int len;
	int i;

	lge_boot_cable = NO_INIT_CABLE;

	for (i = LT_CABLE_56K; i <= NO_INIT_CABLE; i++)	{
		len = max(strlen(cable_info), strlen(cable_str[i]));
		if(strncmp(cable_info, cable_str[i], len))
			continue;

		/* cable type matched */
		lge_boot_cable = (usb_cable_type) i;
		break;
	}

	pr_info("boot cable = %s\n", cable_str[lge_boot_cable]);

	return 1;
}
__setup("lge.cable=", boot_cable_setup);

static int __init fdt_find_boot_cable(unsigned long node, const char *uname,
	int depth, void *data)
{
	char *id;

	if (depth != 1)
		return 0;

	if (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0)
		return 0;

	id = (char*)of_get_flat_dt_prop(node, "lge,boot-cable", NULL);
	if (!id)
		return 0;

	boot_cable_setup(id);

	return 1;
}

static int __init lge_usb_id_pure_init(void)
{
	int rc;

	rc = of_scan_flat_dt(fdt_find_boot_cable, NULL);
	if (!rc)
		pr_err("boot cable not found\n");

	return 0;
}

static int __init lge_usb_id_init(void)
{
	return platform_driver_register(&usb_id_driver);
}

static void __exit lge_usb_id_exit(void)
{
	platform_driver_unregister(&usb_id_driver);
}

pure_initcall(lge_usb_id_pure_init);
module_init(lge_usb_id_init);
module_exit(lge_usb_id_exit);

MODULE_DESCRIPTION("LG Electronics USB ID Module");
MODULE_VERSION("1.2");
MODULE_LICENSE("GPL");
