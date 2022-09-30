ccflags-$(CONFIG_FINGERPRINT_ID_HI_FPC) += -DUSE_FP_ID_DUAL -DFP_ID_VALUE=1
ccflags-$(CONFIG_FINGERPRINT_ID_LO_FPC) += -DUSE_FP_ID_DUAL -DFP_ID_VALUE=0

ccflags-y += -DUSE_ONE_BINARY

ccflags-y += -DWRAP_DEVICE=spi_device -DUSE_SPI_DEVICE
#ccflags-y += -DWRAP_DEVICE=platform_device

ccflags-y += -DUSE_VIRTUAL_INPUT
ccflags-y += -DUSE_FB_NOTIFIER
#ccflags-y += -DUSE_FB_NOTIFIER_LGE_CUSTOM

obj-$(CONFIG_FINGERPRINT_FPC) += fpc_irq.o mtk6779_evb.o
