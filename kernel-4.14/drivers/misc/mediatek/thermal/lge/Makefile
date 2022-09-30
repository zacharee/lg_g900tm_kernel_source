#
# Copyright (C) 2017 MediaTek Inc.
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

THERMAL_CHIP_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/thermal/$(MTK_PLATFORM)
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/lge/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/include/gpufreq_v1/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/auxadc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/$(MTK_PLATFORM)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/common/inc/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-$(CONFIG_LGE_PM_VTS) += lge_cooler_sysrst.o
obj-$(CONFIG_LGE_PM_VTS) += lge_ts_quiet.o
obj-$(CONFIG_LGE_PM_VTS) += lge_vts_skin.o
obj-$(CONFIG_LGE_PM_VTS) += lge_vts_battery.o
obj-$(CONFIG_LGE_PM_VTS) += lge_vts_monitor.o
