# Copyright (C) 2017 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.

LOCAL_PATH := $(call my-dir)

ifeq ($(notdir $(LOCAL_PATH)),$(strip $(LINUX_KERNEL_VERSION)))
ifneq ($(strip $(TARGET_NO_KERNEL)),true)
include $(LOCAL_PATH)/kenv.mk

ifeq ($(PRODUCT_SUPPORT_EXFAT), y)
TUXERA_MODULES_OUT := $(TARGET_OUT_VENDOR)/lib/modules
sinclude ./device/lge/common/tuxera.mk
endif

ifeq ($(wildcard $(TARGET_PREBUILT_KERNEL)),)
KERNEL_MAKE_DEPENDENCIES := $(shell find $(KERNEL_DIR) -name .git -prune -o -type f | sort)
KERNEL_MAKE_DEPENDENCIES := $(filter-out %/.git %/.gitignore %/.gitattributes,$(KERNEL_MAKE_DEPENDENCIES))

$(TARGET_KERNEL_CONFIG): PRIVATE_DIR := $(KERNEL_DIR)
$(TARGET_KERNEL_CONFIG): $(KERNEL_CONFIG_FILE) $(LOCAL_PATH)/Android.mk
$(TARGET_KERNEL_CONFIG): $(KERNEL_MAKE_DEPENDENCIES) | $(KERNEL_OUT)
	echo "Generating a .config file for header generation"
	$(hide) mkdir -p $(dir $@)
	$(PREBUILT_MAKE_PREFIX)$(MAKE) -C $(PRIVATE_DIR) $(KERNEL_MAKE_OPTION) $(KERNEL_DEFCONFIG)

$(BUILT_DTB_OVERLAY_TARGET): $(KERNEL_ZIMAGE_OUT)

.KATI_RESTAT: $(KERNEL_ZIMAGE_OUT)
$(KERNEL_ZIMAGE_OUT): PRIVATE_DIR := $(KERNEL_DIR)
$(KERNEL_ZIMAGE_OUT): $(TARGET_KERNEL_CONFIG) $(KERNEL_MAKE_DEPENDENCIES) | $(KERNEL_OUT)
	$(hide) mkdir -p $(dir $@)
	$(PREBUILT_MAKE_PREFIX)$(MAKE) -C $(PRIVATE_DIR) $(KERNEL_MAKE_OPTION)
	$(hide) $(call fixup-kernel-cmd-file,$(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/compressed/.piggy.xzkern.cmd)
	# check the kernel image size
	python device/mediatek/build/build/tools/check_kernel_size.py $(KERNEL_OUT) $(KERNEL_DIR)

ifeq ($(strip $(MTK_HEADER_SUPPORT)), yes)
$(BUILT_KERNEL_TARGET): $(KERNEL_ZIMAGE_OUT) $(LOCAL_PATH)/Android.mk $(KERNEL_HEADERS_INSTALL) | $(HOST_OUT_EXECUTABLES)/mkimage$(HOST_EXECUTABLE_SUFFIX)
	$(hide) $(HOST_OUT_EXECUTABLES)/mkimage$(HOST_EXECUTABLE_SUFFIX) $< KERNEL 0xffffffff > $@
else
$(BUILT_KERNEL_TARGET): $(KERNEL_ZIMAGE_OUT) $(LOCAL_PATH)/Android.mk $(KERNEL_HEADERS_INSTALL) | $(ACP)
	$(copy-file-to-target)
endif

ifeq ($(PRODUCT_SUPPORT_EXFAT), y)
ifneq ($(SUPPORT_EXFAT_TUXERA), )
	@cp -f ./kernel-4.14/tuxera_update.sh .
ifeq ($(findstring 64,$(TARGET_ARCH)),64)
	@sh tuxera_update.sh --target target/lg.d/mt6883-q-arm64 --use-cache --latest --max-cache-entries 2 --source-dir ./kernel-4.14 --output-dir $(KERNEL_OUT) $(SUPPORT_EXFAT_TUXERA)
else
	@sh tuxera_update.sh --target target/lg.d/mt6883-q-arm32 --use-cache --latest --max-cache-entries 2 --source-dir ./kernel-4.14 --output-dir $(KERNEL_OUT) $(SUPPORT_EXFAT_TUXERA)
endif
	@tar -xzf tuxera-exfat*.tgz
	@mkdir -p ./$(TUXERA_MODULES_OUT)
	@mkdir -p ./$(TARGET_OUT_EXECUTABLES)
	@cp ./tuxera-exfat*/exfat/kernel-module/texfat.ko ./$(TUXERA_MODULES_OUT)
	@cp ./tuxera-exfat*/exfat/tools/* ./$(TARGET_OUT_EXECUTABLES)
	@./$(KERNEL_OUT)/scripts/sign-file sha1 ./$(KERNEL_OUT)/certs/signing_key.pem ./$(KERNEL_OUT)/certs/signing_key.x509 $(TUXERA_MODULES_OUT)/texfat.ko
	@rm -f kheaders*.tar.bz2
	@rm -f tuxera-exfat*.tgz
	@rm -rf tuxera-exfat*
	@rm -f tuxera_update.sh
endif
endif

ifeq ($(USE_LGE_VPN), true)
	@mkdir -p .$(ANDROID_BUILD_TOP)/$(TARGET_OUT_VENDOR)/lib/modules
	@cp $(KERNEL_OUT)/net/netfilter/interceptor_v38/vpnclient.ko .$(ANDROID_BUILD_TOP)/$(TARGET_OUT_VENDOR)/lib/modules/
	@.$(ANDROID_BUILD_TOP)/$(KERNEL_OUT)/scripts/sign-file sha1 .$(ANDROID_BUILD_TOP)/$(KERNEL_OUT)/certs/signing_key.pem .$(ANDROID_BUILD_TOP)/$(KERNEL_OUT)/certs/signing_key.x509 $(TARGET_OUT_VENDOR)/lib/modules/vpnclient.ko
endif

$(TARGET_PREBUILT_KERNEL): $(BUILT_KERNEL_TARGET) $(LOCAL_PATH)/Android.mk | $(ACP)
	$(copy-file-to-new-target)

endif#TARGET_PREBUILT_KERNEL is empty

$(INSTALLED_KERNEL_TARGET): $(BUILT_KERNEL_TARGET) $(LOCAL_PATH)/Android.mk | $(ACP)
	$(copy-file-to-target)

.PHONY: kernel save-kernel kernel-savedefconfig kernel-menuconfig menuconfig-kernel savedefconfig-kernel clean-kernel

KERNEL_HEADER_DEFCONFIG := $(strip $(KERNEL_HEADER_DEFCONFIG))
ifeq ($(KERNEL_HEADER_DEFCONFIG),)
KERNEL_HEADER_DEFCONFIG := $(KERNEL_DEFCONFIG)
endif
KERNEL_CONFIG := $(TARGET_KERNEL_CONFIG)

kernel: $(INSTALLED_KERNEL_TARGET)
save-kernel: $(TARGET_PREBUILT_KERNEL)

kernel-savedefconfig: $(TARGET_KERNEL_CONFIG)
	cp $(TARGET_KERNEL_CONFIG) $(KERNEL_CONFIG_FILE)

kernel-menuconfig: | $(KERNEL_OUT)
	$(hide) mkdir -p $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) menuconfig

menuconfig-kernel savedefconfig-kernel: | $(KERNEL_OUT)
	$(hide) mkdir -p $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(patsubst %config-kernel,%config,$@)

clean-kernel:
	$(hide) rm -rf $(KERNEL_OUT) $(KERNEL_MODULES_OUT) $(INSTALLED_KERNEL_TARGET)
	$(hide) rm -f $(INSTALLED_DTB_OVERLAY_TARGET)

$(KERNEL_OUT):
	$(hide) mkdir -p $@

$(KERNEL_HEADERS_TIMESTAMP) : $(KERNEL_HEADERS_INSTALL)
$(KERNEL_HEADERS_INSTALL) : $(TARGET_KERNEL_CONFIG) | $(KERNEL_OUT)
	$(hide) if [ ! -z "$(KERNEL_HEADER_DEFCONFIG)" ]; then \
				rm -f ../$(KERNEL_CONFIG); \
				$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(KERNEL_HEADER_DEFCONFIG) headers_install; fi
	$(hide) if [ "$(KERNEL_HEADER_DEFCONFIG)" != "$(KERNEL_DEFCONFIG)" ]; then \
				echo "Used a different defconfig for header generation"; \
				rm -f ../$(KERNEL_CONFIG); \
				$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(KERNEL_DEFCONFIG); fi
	$(hide) if [ ! -z "$(KERNEL_CONFIG_OVERRIDE)" ]; then \
				echo "Overriding kernel config with '$(KERNEL_CONFIG_OVERRIDE)'"; \
				echo $(KERNEL_CONFIG_OVERRIDE) >> $(TARGET_KERNEL_CONFIG); \
				$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) oldconfig; fi
	$(hide) touch $@/build-timestamp


.PHONY: check-kernel-config check-kernel-dotconfig check-mtk-config
droid: check-kernel-config check-kernel-dotconfig
check-mtk-config: check-kernel-config check-kernel-dotconfig
check-kernel-config: PRIVATE_COMMAND := $(if $(wildcard device/mediatek/build/build/tools/check_kernel_config.py),$(if $(filter yes,$(DISABLE_MTK_CONFIG_CHECK)),-)python device/mediatek/build/build/tools/check_kernel_config.py -c $(MTK_TARGET_PROJECT_FOLDER)/ProjectConfig.mk -k $(KERNEL_CONFIG_FILE) -p $(MTK_PROJECT_NAME))
# no need to check defconfig. It is same with check-kernel-dotconfig(.config)
check-kernel-config:
	echo $(PRIVATE_COMMAND)

check-kernel-dotconfig: PRIVATE_COMMAND := $(if $(wildcard device/mediatek/build/build/tools/check_kernel_config.py),$(if $(filter yes,$(DISABLE_MTK_CONFIG_CHECK)),-)python device/mediatek/build/build/tools/check_kernel_config.py -c $(MTK_TARGET_PROJECT_FOLDER)/ProjectConfig.mk -k $(TARGET_KERNEL_CONFIG) -p $(MTK_PROJECT_NAME))
check-kernel-dotconfig: $(TARGET_KERNEL_CONFIG)
	$(PRIVATE_COMMAND)

### DTB
ifdef BOARD_PREBUILT_DTBIMAGE_DIR
INSTALLED_MTK_DTB_TARGET := $(BOARD_PREBUILT_DTBIMAGE_DIR)/mtk_dtb
$(shell if [ ! -f $(INSTALLED_MTK_DTB_TARGET) ]; then mkdir -p $(dir $(INSTALLED_MTK_DTB_TARGET)); touch $(INSTALLED_MTK_DTB_TARGET);fi)
$(INSTALLED_MTK_DTB_TARGET): $(INSTALLED_KERNEL_TARGET)
	@mkdir -p $(dir $@)
	@cp -f $(KERNEL_DTB_FILE) $@
endif

endif#TARGET_NO_KERNEL
endif#LINUX_KERNEL_VERSION
