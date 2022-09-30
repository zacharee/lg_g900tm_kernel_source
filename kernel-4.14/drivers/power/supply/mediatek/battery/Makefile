#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#


MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/pmic/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/pmic/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/power/supply/mediatek/misc

ifeq ($(CONFIG_LGE_PM_BATTERY_NTC_TUNED),y)
# add custom include path for project
	ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
	MT_PLAT_PATH := $(srctree)/drivers/misc/mediatek/include/mt-plat

	ifeq (,$(wildcard $(MT_PLAT_PATH)/$(MTK_PLATFORM)/include/mach/custom_battery_table/$(ARCH_MTK_PROJECT)))
		subdir-ccflags-y += -I$(MT_PLAT_PATH)/$(MTK_PLATFORM)/include/mach/custom_battery_table/default
	else
		subdir-ccflags-y += -I$(MT_PLAT_PATH)/$(MTK_PLATFORM)/include/mach/custom_battery_table/$(ARCH_MTK_PROJECT)
	endif
endif # CONFIG_LGE_PM_BATTERY_NTC_TUNED

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
	obj-y += mtk_battery.o mtk_power_misc.o mtk_gauge_coulomb_service.o mtk_gauge_class.o mtk_battery_recovery.o mtk_battery_core.o
endif
