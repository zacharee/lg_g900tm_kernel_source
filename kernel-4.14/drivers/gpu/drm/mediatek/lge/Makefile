ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)	\
             -I$(srctree)/drivers/misc/mediatek/smi/			\
             -I$(srctree)/drivers/misc/mediatek/mmp/			\
             -I$(srctree)/drivers/misc/mediatek/mmdvfs/			\
             -I$(srctree)/drivers/iommu/				\
             -I$(srctree)/drivers/devfreq/				\
             -I$(srctree)/drivers/gpu/drm/				\
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/				\
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat \
             -I$(srctree)/drivers/misc/mediatek/leds/ \
             -I$(srctree)/drivers/misc/mediatek/leds/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/ \
             -I$(srctree)/drivers/gpu/drm/mediatek/lge/ \
             -I$(srctree)/drivers/gpu/drm/mediatek/lge/ambient/ \
             -I$(srctree)/drivers/gpu/drm/mediatek/lge/brightness/ \
             -I$(srctree)/drivers/gpu/drm/mediatek/lge/cm/ \
             -I$(srctree)/drivers/gpu/drm/mediatek/lge/lge_tuning/ \
             -I$(srctree)/drivers/gpu/drm/mediatek/ \
             -I$(srctree)/kernel/irq/

lge_display_common-objs := lge_dsi_panel.o
lge_display_common-objs += lge_panel_reg.o
lge_display_common-objs += lge_ddic_ops.o
lge_display_common-objs += lge_ddic_ops_sw43103.o
lge_display_common-objs += ambient/lge_backlight_ex.o
lge_display_common-objs += ambient/lge_ambient.o
lge_display_common-objs += cm/lge_color_manager.o
#lge_display_common-objs += drs/lge_drs_mngr.o
lge_display_common-objs += brightness/lge_brightness.o
lge_display_common-objs += factory/lge_factory.o
lge_display_common-objs += err_detect/lge_err_detect.o

obj-$(CONFIG_LGE_DISPLAY_COMMON) += lge_display_common.o

ifeq ($(CONFIG_LGE_DUAL_SCREEN),y)
	#lge_display_common-objs += ds3/lge_backlight_ds3.o
	lge_display_common-objs += cover/lge_cover_ctrl.o
	lge_display_common-objs += cover/lge_dsi_panel_cover.o
endif

ifeq ($(CONFIG_LGE_DISPLAY_TUNING_SUPPORT),y)
obj-y += lge_tuning/lge_tuning.o
endif
