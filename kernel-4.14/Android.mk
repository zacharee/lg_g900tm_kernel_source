# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2019 MediaTek Inc.

LOCAL_PATH := $(call my-dir)

ifeq ($(notdir $(LOCAL_PATH)),$(strip $(LINUX_KERNEL_VERSION)))
ifneq ($(strip $(TARGET_NO_KERNEL)),true)

include $(LOCAL_PATH)/kenv.mk

ifeq ($(PRODUCT_SUPPORT_EXFAT), y)
sinclude ./device/lge/common/fs/tuxera.mk
endif

# Add LGE target feature
ifeq ($(findstring muse,$(TARGET_DEVICE)),muse)
LGE_MUSE_NAME = $(subst _, ,$(subst muse,mt,$(TARGET_DEVICE)))
LGE_REAL_PLATFORM = $(word 1, $(LGE_MUSE_NAME))
ifeq ($(findstring 64,$(TARGET_ARCH)),64)
LGE_REAL_DEVICE = $(word 3, $(LGE_MUSE_NAME))
else
LGE_REAL_DEVICE = $(word 2, $(LGE_MUSE_NAME))
endif
LGE_LAMP_DEVICE=no
else
LGE_REAL_PLATFORM = $(TARGET_BOARD_PLATFORM)
LGE_REAL_DEVICE = $(TARGET_DEVICE)
LGE_LAMP_DEVICE=yes
endif
KERNEL_MAKE_OPTION := $(KERNEL_MAKE_OPTION) LGE_TARGET_PLATFORM=$(LGE_REAL_PLATFORM) LGE_TARGET_DEVICE=$(LGE_REAL_DEVICE) LGE_LAMP_DEVICE=$(LGE_LAMP_DEVICE)

ifeq ($(findstring lge,$(PROJECT_DTB_NAMES)),lge)
	LGE_DTS_FILES := $(notdir $(basename $(wildcard $(KERNEL_DIR)/arch/$(KERNEL_TARGET_ARCH)/boot/dts/$(PROJECT_DTB_NAMES)/*.dts)))
	LGE_DT_NAMES := $(addprefix $(PROJECT_DTB_NAMES)/, $(LGE_DTS_FILES))
endif

ifeq ($(wildcard $(TARGET_PREBUILT_KERNEL)),)
KERNEL_MAKE_DEPENDENCIES := $(shell find $(KERNEL_DIR) -name .git -prune -o -type f | sort)

$(TARGET_KERNEL_CONFIG): PRIVATE_DIR := $(KERNEL_DIR)
$(TARGET_KERNEL_CONFIG): $(KERNEL_CONFIG_FILE) $(LOCAL_PATH)/Android.mk
$(TARGET_KERNEL_CONFIG): $(KERNEL_MAKE_DEPENDENCIES) | $(KERNEL_OUT)
	$(hide) mkdir -p $(dir $@)
	$(PREBUILT_MAKE_PREFIX)$(MAKE) -C $(PRIVATE_DIR) $(KERNEL_MAKE_OPTION) $(KERNEL_DEFCONFIG)

.KATI_RESTAT: $(KERNEL_ZIMAGE_OUT)
$(KERNEL_ZIMAGE_OUT): PRIVATE_DIR := $(KERNEL_DIR)
$(KERNEL_ZIMAGE_OUT): $(TARGET_KERNEL_CONFIG) $(KERNEL_MAKE_DEPENDENCIES) | $(KERNEL_OUT)
	$(hide) mkdir -p $(dir $@)
	$(PREBUILT_MAKE_PREFIX)$(MAKE) -C $(PRIVATE_DIR) $(KERNEL_MAKE_OPTION)
	$(hide) $(call fixup-kernel-cmd-file,$(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/compressed/.piggy.xzkern.cmd)
ifeq ($(PRODUCT_SUPPORT_EXFAT), y)
	$(build-tuxera-exfat)
endif
	# check the kernel image size
ifeq ($(findstring lge,$(PROJECT_DTB_NAMES)),lge)
	python device/mediatek/build/build/tools/check_kernel_size.py $(KERNEL_OUT) $(KERNEL_DIR) $(LGE_DT_NAMES)
else
	python device/mediatek/build/build/tools/check_kernel_size.py $(KERNEL_OUT) $(KERNEL_DIR) $(PROJECT_DTB_NAMES)
endif

$(BUILT_KERNEL_TARGET): $(KERNEL_ZIMAGE_OUT) $(LOCAL_PATH)/Android.mk $(KERNEL_HEADERS_INSTALL) | $(ACP)
	$(copy-file-to-target)

$(TARGET_PREBUILT_KERNEL): $(BUILT_KERNEL_TARGET) $(LOCAL_PATH)/Android.mk | $(ACP)
	$(copy-file-to-new-target)

endif #TARGET_PREBUILT_KERNEL is empty

$(INSTALLED_KERNEL_TARGET): $(BUILT_KERNEL_TARGET) $(LOCAL_PATH)/Android.mk | $(ACP)
	$(copy-file-to-target)

.PHONY: kernel save-kernel kernel-savedefconfig kernel-menuconfig menuconfig-kernel savedefconfig-kernel clean-kernel

KERNEL_HEADER_DEFCONFIG := $(strip $(KERNEL_HEADER_DEFCONFIG))
ifeq ($(KERNEL_HEADER_DEFCONFIG),)
	KERNEL_HEADER_DEFCONFIG := $(KERNEL_DEFCONFIG)
endif
KERNEL_CONFIG := $(TARGET_KERNEL_CONFIG)

kernel: $(INSTALLED_KERNEL_TARGET) $(INSTALLED_MTK_DTB_TARGET)
save-kernel: $(TARGET_PREBUILT_KERNEL)

kernel-savedefconfig: $(TARGET_KERNEL_CONFIG)
	cp $(TARGET_KERNEL_CONFIG) $(KERNEL_CONFIG_FILE)

kernel-menuconfig:
	$(hide) mkdir -p $(KERNEL_OUT)
	$(PREBUILT_MAKE_PREFIX)$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) menuconfig

menuconfig-kernel savedefconfig-kernel:
	$(hide) mkdir -p $(KERNEL_OUT)
	$(PREBUILT_MAKE_PREFIX)$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(patsubst %config-kernel,%config,$@)

clean-kernel:
	$(hide) rm -rf $(KERNEL_OUT) $(INSTALLED_KERNEL_TARGET)
$(KERNEL_OUT):
	$(hide) mkdir -p $@

$(KERNEL_HEADERS_TIMESTAMP) : $(KERNEL_HEADERS_INSTALL)
$(KERNEL_HEADERS_INSTALL) : $(TARGET_KERNEL_CONFIG) | $(KERNEL_OUT)
	$(hide) if [ ! -z "$(KERNEL_HEADER_DEFCONFIG)" ]; then \
				rm -f ../$(KERNEL_CONFIG); \
				$(PREBUILT_MAKE_PREFIX)$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(KERNEL_HEADER_DEFCONFIG) headers_install; fi
	$(hide) if [ "$(KERNEL_HEADER_DEFCONFIG)" != "$(KERNEL_DEFCONFIG)" ]; then \
				echo "Used a different defconfig for header generation"; \
				rm -f ../$(KERNEL_CONFIG); \
				$(PREBUILT_MAKE_PREFIX)$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(KERNEL_DEFCONFIG); fi
	$(hide) if [ ! -z "$(KERNEL_CONFIG_OVERRIDE)" ]; then \
				echo "Overriding kernel config with '$(KERNEL_CONFIG_OVERRIDE)'"; \
				echo $(KERNEL_CONFIG_OVERRIDE) >> $(TARGET_KERNEL_CONFIG); \
				$(PREBUILT_MAKE_PREFIX)$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) oldconfig; fi
	$(hide) touch $@/build-timestamp

### DTB build template
MTK_DTBIMAGE_DTS := $(addsuffix .dts,$(addprefix $(KERNEL_DIR)/arch/$(KERNEL_TARGET_ARCH)/boot/dts/,$(PLATFORM_DTB_NAME)))
include device/mediatek/build/core/build_dtbimage.mk

ifeq ($(findstring lge,$(PROJECT_DTB_NAMES)),lge)
MTK_DTBOIMAGE_DTS := $(addsuffix .dts,$(addprefix $(KERNEL_DIR)/arch/$(KERNEL_TARGET_ARCH)/boot/dts/,$(LGE_DT_NAMES)))
include device/mediatek/build/core/build_dtboimage_lge.mk
else
MTK_DTBOIMAGE_DTS := $(addsuffix .dts,$(addprefix $(KERNEL_DIR)/arch/$(KERNEL_TARGET_ARCH)/boot/dts/,$(PROJECT_DTB_NAMES)))
include device/mediatek/build/core/build_dtboimage.mk
endif

endif #TARGET_NO_KERNEL
endif #LINUX_KERNEL_VERSION
