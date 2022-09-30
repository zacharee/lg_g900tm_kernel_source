MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)

ccflags-$(CONFIG_FINGERPRINT_ID_HI_EGISTEC) += -DUSE_FP_ID_DUAL -DFP_ID_VALUE=1
ccflags-$(CONFIG_FINGERPRINT_ID_LO_EGISTEC) += -DUSE_FP_ID_DUAL -DFP_ID_VALUE=0

ccflags-y += -DUSE_ONE_BINARY
ccflags-y += -DUSE_FB_NOTIFIER

obj-$(CONFIG_FINGERPRINT_EGISTEC) += et520.o navi_input.o
