subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/misc

subdir-ccflags-$(CONFIG_LGE_PM_DEBUG) += -DDEBUG

charger_controller-y := charger_controller_core.o \
			charger_controller_common.o \
			charger_controller_dbg.o \
			charger_controller_impl.o
charger_controller-y += charger_controller_actm.o
charger_controller-y += charger_controller_ccd.o
charger_controller-y += charger_controller_otp.o
charger_controller-y += charger_controller_spec.o
charger_controller-y += charger_controller_game.o
charger_controller-y += charger_controller_thermal.o
charger_controller-y += charger_controller_ttf.o
charger_controller-y += charger_controller_misc.o
charger_controller-y += charger_controller_atcmd.o
ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
	charger_controller-y += charger_controller_impl_gm30.o
endif

pseudo_power_supply-y := lge_pseudo_power_supply.o
pseudo_power_supply-$(CONFIG_LGE_PM_PSEUDO_BATTERY) += lge_pseudo_batt.o
pseudo_power_supply-$(CONFIG_LGE_PM_PSEUDO_HVDCP) += lge_pseudo_hvdcp.o

obj-$(CONFIG_LGE_PM) += lge_chg_type.o
obj-$(CONFIG_LGE_PM) += lge_charging.o lge_charging_hvdcp.o
obj-$(CONFIG_LGE_PM_BATTERY_CYCLE) += battery_cycle.o
obj-$(CONFIG_LGE_PM_BATTERY_ID) += lge_battery_id.o
obj-$(CONFIG_LGE_PM_CHARGER_CONTROLLER) += charger_controller.o
obj-$(CONFIG_LGE_PM_POWER_SYSFS) += lge_power_sysfs.o
obj-$(CONFIG_LGE_PM_PSEUDO_POWER_SUPPLY) += pseudo_power_supply.o
obj-$(CONFIG_LGE_PM_PUMP_EXPRESS_PLUS_ALGO) += lge_pe_algo.o
obj-$(CONFIG_LGE_PM_QNOVO_QNS) += qns_system.o
obj-$(CONFIG_LGE_PM_USB_ID) += lge_usb_id.o
obj-$(CONFIG_LGE_PM_WIRELESS_CHARGER) += lge_charging_wless.o
obj-$(CONFIG_CHARGER_BQ25910) += bq25910.o
obj-$(CONFIG_CHARGER_BQ25970) += bq25970.o
obj-$(CONFIG_CHARGER_MAX77932) += max77932.o
obj-$(CONFIG_WIRELESS_CHARGER_RX_RT1653) += rt1653.o
obj-$(CONFIG_WIRELESS_CHARGER_RX_P9222) += p9222.o
