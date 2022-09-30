#ifndef __LGE_CHG_TYPE_H__
#define __LGE_CHG_TYPE_H__

#include <linux/device.h>
#include <linux/workqueue.h>
#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_charger.h>
#include <tcpm.h>

enum {
	VZW_NO_CHARGER,
	VZW_NORMAL_CHARGING,
	VZW_INCOMPATIBLE_CHARGING,
	VZW_UNDER_CURRENT_CHARGING,
	VZW_USB_DRIVER_UNINSTALLED,
	VZW_CHARGER_STATUS_MAX,
};

struct lge_chg_type {
	struct device *dev;
	struct power_supply *psy;

	enum charger_type chg_type;
	struct charger_consumer *bc12;
	struct tcpc_device *tcpc;

	int voltage_max;
	int current_max;

	int fastchg_support;
	int fastchg;

	int vzw_chg;

	struct delayed_work floated_dwork;
	int floated_triggered;
	int floated;

	unsigned int floated_retry_ms;
};

int lge_chg_type_get_voltage_max(struct lge_chg_type *lct);
int lge_chg_type_get_current_max(struct lge_chg_type *lct);
int lge_chg_type_is_fastchg(struct lge_chg_type *lct);
int lge_chg_type_is_fastchg_support(struct lge_chg_type *lct);
int lge_chg_type_get_vzw_chg(struct lge_chg_type *lct);
int lge_chg_type_is_floated(struct lge_chg_type *lct);
int lge_chg_type_set_property(struct lge_chg_type *lct,
			      enum power_supply_property psp,
			      const union power_supply_propval *val);
int lge_chg_type_init(struct lge_chg_type *lct);
#endif
