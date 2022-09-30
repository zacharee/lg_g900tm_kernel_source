/*
 *  lge_battery_id.c
 *
 *  LGE Battery Charger Interface Driver
 *
 *  Copyright (C) 2011 LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/power/lge_battery_id.h>
#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
#include <linux/power/lge_pseudo_batt.h>
#endif
#ifdef CONFIG_LGE_PM_USB_ID
#include <linux/power/lge_usb_id.h>
#endif

#define BATT_NOT_PRESENT 200

enum {
	BATT_ID_MISSING         = -1,
	BATT_ID_UNKNOWN         = 0,
	/* Embedded ADC Gen 1 */
	BATT_ID_LGC             = 5,
	BATT_ID_TOCAD           = 75,
	BATT_ID_ATL             = 200,
	BATT_ID_BYD             = 225,
	BATT_ID_LISHEN          = 230,
	/* Embedded ADC Gen 2 */
	BATT_LGC_GEN2           = 7,
	BATT_TOCAD_GEN2         = 77,
	BATT_ATL_GEN2           = 202,
	BATT_BYD_GEN2           = 227,
	BATT_LISHEN_GEN2        = 232,
	/* Authentication IC */
	BATT_ID_DS2704_N        = 17,
	BATT_ID_DS2704_L        = 32,
	BATT_ID_DS2704_C        = 48,
	BATT_ID_ISL6296_N       = 73,
	BATT_ID_ISL6296_L       = 94,
	BATT_ID_ISL6296_C       = 105,
	BATT_ID_ISL6296A_N      = 110,
	BATT_ID_ISL6296A_L      = 115,
	BATT_ID_ISL6296A_C      = 120,
	BATT_ID_RA4301_VC0      = 130,
	BATT_ID_RA4301_VC1      = 147,
	BATT_ID_RA4301_VC2      = 162,
	BATT_ID_SW3800_VC0      = 187,
	BATT_ID_SW3800_VC1      = 204,
	BATT_ID_SW3800_VC2      = 219,
};

struct battery_id_t {
	int id;
	char *name;
};

struct lge_battery_id_info {
	struct device *dev;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;

	/* battery id information */
	const struct battery_id_t *id;
	int valid;

	/* battery information */
	int voltage_max;
	int charge_full_design;
	const char *model_name;
	const char *manufacturer;
	const char *type;

	/* MediaTek Battery Profile Index */
	int profile_idx;
};

static const struct battery_id_t id_unknown = {
	.id = BATT_ID_UNKNOWN,
	.name = "UNKNOWN",
};

static const struct battery_id_t id_missing = {
	.id = BATT_ID_MISSING,
	.name = "MISSING",
};

static const struct battery_id_t ids[] = {
	/* Embedded ADC Gen 1 */
	{ BATT_ID_LGC, "LGC" },
	{ BATT_ID_TOCAD, "TOCAD" },
	{ BATT_ID_ATL, "ATL" },
	{ BATT_ID_BYD, "BYD" },
	{ BATT_ID_LISHEN, "LISHEN" },
	/* Embedded ADC Gen 2 */
	{ BATT_LGC_GEN2, "LGC_GEN2" },
	{ BATT_TOCAD_GEN2, "TOCAD_GEN2" },
	{ BATT_ATL_GEN2, "ATL_GEN2" },
	{ BATT_BYD_GEN2, "BYD_GEN2" },
	{ BATT_LISHEN_GEN2, "LISHEN_GEN2" },
	/* Authentication IC */
	{ BATT_ID_DS2704_N, "DS2704_N" },
	{ BATT_ID_DS2704_L, "DS2704_L" },
	{ BATT_ID_DS2704_C, "DS2704_C" },
	{ BATT_ID_ISL6296_N, "ISL6296_N" },
	{ BATT_ID_ISL6296_L, "ISL6296_L" },
	{ BATT_ID_ISL6296_C, "ISL6296_C" },
	{ BATT_ID_ISL6296A_N, "ISL6296A_N" },
	{ BATT_ID_ISL6296A_L, "ISL6296A_L" },
	{ BATT_ID_ISL6296A_C, "ISL6296A_C" },
	{ BATT_ID_RA4301_VC0, "RA4301_VC0" },
	{ BATT_ID_RA4301_VC1, "RA4301_VC1" },
	{ BATT_ID_RA4301_VC2, "RA4301_VC2" },
	{ BATT_ID_SW3800_VC0, "SW3800_VC0" },
	{ BATT_ID_SW3800_VC1, "SW3800_VC1" },
	{ BATT_ID_SW3800_VC2, "SW3800_VC2" },
};
static char *battery_id_name = NULL;

static int battery_profile_id = 0;
int lge_battery_id_get_profile_id(void)
{
	return battery_profile_id;
}
EXPORT_SYMBOL(lge_battery_id_get_profile_id);

static enum power_supply_property lge_battery_id_properties[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_BATT_ID,
	POWER_SUPPLY_PROP_VALID_BATT_ID,
	POWER_SUPPLY_PROP_CHECK_BATT_ID_FOR_AAT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	/* Properties of type `const char *' */
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
};

static int lge_battery_id_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct lge_battery_id_info *info = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = info->voltage_max;
		break;
	case POWER_SUPPLY_PROP_BATT_ID:
		val->intval = info->id->id;
#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
		if (get_pseudo_batt_info(PSEUDO_BATT_MODE))
			val->intval = get_pseudo_batt_info(PSEUDO_BATT_ID);
#endif
		break;
	case POWER_SUPPLY_PROP_VALID_BATT_ID:
		val->intval = info->valid;
#ifdef CONFIG_LGE_PM_PSEUDO_BATTERY
		if (get_pseudo_batt_info(PSEUDO_BATT_MODE))
			val->intval = 1;
#endif
#ifdef CONFIG_LGE_PM_USB_ID
		if (lge_is_factory_cable_boot())
			val->intval = 1;
#endif
		break;
	case POWER_SUPPLY_PROP_CHECK_BATT_ID_FOR_AAT:
		val->intval = info->valid;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = info->charge_full_design;
		break;
	/* Properties of type `const char *' */
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = info->model_name;
		if (!val->strval)
			val->strval = "Unknown";
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = info->manufacturer;
		if (!val->strval)
			val->strval = "Unknown";
		break;
	case POWER_SUPPLY_PROP_BATTERY_TYPE:
		val->strval = info->type;
		if (!val->strval)
			val->strval = "Unknown";
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int lge_battery_id_read_info(struct lge_battery_id_info *info,
				    struct device_node *np)
{
	of_property_read_u32(np, "profile-idx", &info->profile_idx);
	of_property_read_u32(np, "voltage-max", &info->voltage_max);
	of_property_read_u32(np, "charge-full-design",
			&info->charge_full_design);
	of_property_read_string(np, "model-name", &info->model_name);
	of_property_read_string(np, "manufacturer", &info->manufacturer);
	of_property_read_string(np, "type", &info->type);

	return 0;
}

static bool lge_battery_id_is_match(struct lge_battery_id_info *info,
				    struct device_node *battery)
{
	struct property *prop;
	const char *cp;

	prop = of_find_property(battery, "id", NULL);
	if (!prop)
		return false;

	for (cp = of_prop_next_string(prop, NULL); cp;
			cp = of_prop_next_string(prop, cp)) {
		if (!strcmp(info->id->name, cp))
			break;
	}

	if (cp)
		return true;

	return false;
}
static int lge_battery_id_parse_dt(struct lge_battery_id_info *info)
{
	struct device_node *np = info->dev->of_node;
	struct device_node *battery = NULL;

	/* read base information */
	lge_battery_id_read_info(info, np);

	/* find battery information */
	for (battery = of_get_next_child(np, NULL); battery;
			battery = of_get_next_child(np, battery)) {
		if (lge_battery_id_is_match(info, battery))
			break;
	}

	/* no battery information found */
	if (!battery)
		return 0;

	/* read battery information */
	lge_battery_id_read_info(info, battery);

	of_node_put(battery);

	info->valid = 1;
	battery_profile_id = info->profile_idx;

	return 0;
}

static const struct battery_id_t *lge_battery_id_find_id(char *name)
{
	int i;

	if (!name)
		return &id_unknown;

	for (i = 0; i < ARRAY_SIZE(ids); i++) {
		if (!strcmp(name, ids[i].name))
			return &ids[i];
	}

	if (!strcmp(name, id_missing.name))
		return &id_missing;

	return &id_unknown;
}

static int lge_battery_id_probe(struct platform_device *pdev)
{
	struct lge_battery_id_info *info;
	int ret = 0;

	info = devm_kzalloc(&pdev->dev, sizeof(struct lge_battery_id_info),
			GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "failed: allocation memory\n");
		return -ENOMEM;
	}

	info->dev = &pdev->dev;
	info->id = lge_battery_id_find_id(battery_id_name);
	platform_set_drvdata(pdev, info);

	ret = lge_battery_id_parse_dt(info);
	if (ret)
		return ret;

	info->psy_desc.name = "battery_id";
	info->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_desc.properties = lge_battery_id_properties;
	info->psy_desc.num_properties =
			ARRAY_SIZE(lge_battery_id_properties);
	info->psy_desc.get_property = lge_battery_id_get_property;

	info->psy_cfg.drv_data = info;

	info->psy = power_supply_register(info->dev, &info->psy_desc,
			&info->psy_cfg);
	if (!info->psy)
		return -ENODEV;

	dev_info(info->dev, "ID: %s, Battery: %s, Vendor: %s\n",
			info->id->name,
			info->model_name ? info->model_name : "Unknown",
			info->manufacturer ? info->manufacturer : "Unknown");

	return ret;
}

static int lge_battery_id_remove(struct platform_device *pdev)
{
	struct lge_battery_id_info *info = platform_get_drvdata(pdev);

	power_supply_unregister(info->psy);

	return 0;
}

static struct of_device_id lge_battery_id_match_table[] = {
	{ .compatible = "lge,battery-id" },
	{}
};

static struct platform_driver lge_battery_id_driver = {
	.driver = {
		.name   = "lge_battery_id",
		.owner  = THIS_MODULE,
		.of_match_table = lge_battery_id_match_table,
	},
	.probe  = lge_battery_id_probe,
	.remove = lge_battery_id_remove,
};

static int __init fdt_find_battery_id(unsigned long node, const char *uname,
	int depth, void *data)
{
	char *name;

	if (depth != 1)
		return 0;

	if (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0)
		return 0;

	name = (char*)of_get_flat_dt_prop(node, "lge,battery-id", NULL);
	if (!name)
		return 0;

	battery_id_name = name;

	return 1;
}

static int __init lge_battery_id_init(void)
{
	int rc;

	rc = of_scan_flat_dt(fdt_find_battery_id, NULL);
	if (!rc) {
		pr_err("battery id not found. driver disabled\n");
		return 0;
	}

	return platform_driver_register(&lge_battery_id_driver);
}

static void __exit lge_battery_id_exit(void)
{
	platform_driver_unregister(&lge_battery_id_driver);
}

module_init(lge_battery_id_init);
module_exit(lge_battery_id_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Cowboy");
MODULE_DESCRIPTION("LGE Battery ID Checker");
