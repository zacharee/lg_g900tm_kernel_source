#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/power_supply.h>
#include "lge_pseudo_power_supply.h"

void pseudo_power_supply_update(char *name)
{
	struct power_supply *psy;

	if (!name)
		return;

	psy = power_supply_get_by_name(name);
	if (!psy)
		return;

	/* notify userspace using exist power-supply */
	power_supply_changed(psy);

	power_supply_put(psy);
}

static void __exit pseudo_power_supply_exit(void)
{
	return;
}

static int __init pseudo_power_supply_init(void)
{
	return 0;
}

module_init(pseudo_power_supply_init);
module_exit(pseudo_power_supply_exit);

MODULE_DESCRIPTION("Pseudo Power-Supply Module");
MODULE_VERSION("1.2");
MODULE_LICENSE("GPL");
