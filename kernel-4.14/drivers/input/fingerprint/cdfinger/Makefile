#
# Makefile for cdfinger drivers.
#
ccflags-$(CONFIG_FINGERPRINT_ID_HI_CDFINGER) += -DUSE_FP_ID_DUAL -DFP_ID_VALUE=1
ccflags-$(CONFIG_FINGERPRINT_ID_LO_CDFINGER) += -DUSE_FP_ID_DUAL -DFP_ID_VALUE=0

ccflags-y += -DUSE_ONE_BINARY

obj-y += cdfinger-mtk-tee.o
