# Copyright (C) 2026 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifneq ($(TARGET_NO_KERNEL),true)
ifneq ($(strip $(TARGET_KERNEL_SOURCE)),)

AMAZON_KERNEL_SOURCE := $(TARGET_KERNEL_SOURCE)
AMAZON_KERNEL_ARCH := $(strip $(TARGET_KERNEL_ARCH))
ifeq ($(AMAZON_KERNEL_ARCH),)
AMAZON_KERNEL_ARCH := $(TARGET_ARCH)
endif

AMAZON_KERNEL_DEFCONFIG := $(word 1,$(TARGET_KERNEL_CONFIG))
ifeq ($(AMAZON_KERNEL_DEFCONFIG),)
$(error TARGET_KERNEL_CONFIG must be set to build the Amazon kernel)
endif
ifeq ($(BOARD_KERNEL_IMAGE_NAME),)
$(error BOARD_KERNEL_IMAGE_NAME must be set to build the Amazon kernel)
endif
ifeq ($(wildcard $(AMAZON_KERNEL_SOURCE)/Makefile),)
$(error TARGET_KERNEL_SOURCE points to missing kernel source: $(AMAZON_KERNEL_SOURCE))
endif

AMAZON_KERNEL_DEFCONFIG_FILE := $(AMAZON_KERNEL_SOURCE)/arch/$(AMAZON_KERNEL_ARCH)/configs/$(AMAZON_KERNEL_DEFCONFIG)
ifeq ($(wildcard $(AMAZON_KERNEL_DEFCONFIG_FILE)),)
$(error TARGET_KERNEL_CONFIG points to missing defconfig: $(AMAZON_KERNEL_DEFCONFIG_FILE))
endif

AMAZON_KERNEL_OUT := $(PRODUCT_OUT)/obj/KERNEL_OBJ
AMAZON_KERNEL_CONFIG := $(AMAZON_KERNEL_OUT)/.config
AMAZON_KERNEL_IMAGE := $(AMAZON_KERNEL_OUT)/arch/$(AMAZON_KERNEL_ARCH)/boot/$(BOARD_KERNEL_IMAGE_NAME)
AMAZON_KERNEL_MODULES_STAGING := $(call intermediates-dir-for,PACKAGING,amazon_kernel_modules)
AMAZON_KERNEL_MODULES_OUT := $(TARGET_OUT_VENDOR)/lib/modules
AMAZON_KERNEL_MODULES_STAMP := $(AMAZON_KERNEL_MODULES_STAGING)/modules.timestamp

AMAZON_KERNEL_CROSS_COMPILE := $(strip $(AMAZON_KERNEL_CROSS_COMPILE_PREFIX))
ifeq ($(AMAZON_KERNEL_CROSS_COMPILE),)
AMAZON_KERNEL_CROSS_COMPILE := $(strip $(TARGET_KERNEL_CROSS_COMPILE_PREFIX))
endif
ifeq ($(AMAZON_KERNEL_CROSS_COMPILE),)
ifeq ($(AMAZON_KERNEL_ARCH),arm64)
AMAZON_KERNEL_CROSS_COMPILE := $(abspath prebuilts/gcc/$(HOST_PREBUILT_TAG)/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-)
else ifeq ($(AMAZON_KERNEL_ARCH),arm)
AMAZON_KERNEL_CROSS_COMPILE := $(abspath prebuilts/gcc/$(HOST_PREBUILT_TAG)/arm/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-)
endif
endif

AMAZON_KERNEL_MAKE_FLAGS := O=$(abspath $(AMAZON_KERNEL_OUT)) ARCH=$(AMAZON_KERNEL_ARCH) ROOTDIR=$(abspath .)
ifneq ($(AMAZON_KERNEL_CROSS_COMPILE),)
AMAZON_KERNEL_MAKE_FLAGS += CROSS_COMPILE=$(AMAZON_KERNEL_CROSS_COMPILE)
endif

ifeq ($(AMAZON_KERNEL_ARCH),arm64)
AMAZON_KERNEL_CROSS_COMPILE_ARM32 := $(strip $(AMAZON_KERNEL_CROSS_COMPILE_ARM32_PREFIX))
ifeq ($(AMAZON_KERNEL_CROSS_COMPILE_ARM32),)
AMAZON_KERNEL_CROSS_COMPILE_ARM32 := $(strip $(TARGET_KERNEL_CROSS_COMPILE_ARM32_PREFIX))
endif
ifeq ($(AMAZON_KERNEL_CROSS_COMPILE_ARM32),)
AMAZON_KERNEL_CROSS_COMPILE_ARM32 := $(abspath prebuilts/gcc/$(HOST_PREBUILT_TAG)/arm/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-)
endif
AMAZON_KERNEL_MAKE_FLAGS += CROSS_COMPILE_ARM32=$(AMAZON_KERNEL_CROSS_COMPILE_ARM32)
AMAZON_KERNEL_MAKE_FLAGS += CROSS_COMPILE_COMPAT=$(AMAZON_KERNEL_CROSS_COMPILE_ARM32)
endif

ifneq ($(SHOW_COMMANDS),)
AMAZON_KERNEL_MAKE_FLAGS += V=1
endif

$(AMAZON_KERNEL_CONFIG): $(AMAZON_KERNEL_DEFCONFIG_FILE)
	@echo "Kernel config: $@"
	$(hide) mkdir -p $(AMAZON_KERNEL_OUT)
	$(hide) $(MAKE) -C $(AMAZON_KERNEL_SOURCE) $(AMAZON_KERNEL_MAKE_FLAGS) $(AMAZON_KERNEL_DEFCONFIG)

$(AMAZON_KERNEL_IMAGE): $(AMAZON_KERNEL_CONFIG)
	@echo "Building kernel image: $(BOARD_KERNEL_IMAGE_NAME)"
	$(hide) $(MAKE) -C $(AMAZON_KERNEL_SOURCE) $(AMAZON_KERNEL_MAKE_FLAGS) $(BOARD_KERNEL_IMAGE_NAME)

$(INSTALLED_KERNEL_TARGET): $(AMAZON_KERNEL_IMAGE) | $(ACP)
	@echo "Install kernel: $@"
	$(copy-file-to-target)

ifeq ($(shell grep -q '^CONFIG_MODULES=y' $(AMAZON_KERNEL_DEFCONFIG_FILE) 2>/dev/null && echo true),true)
$(AMAZON_KERNEL_MODULES_STAMP): $(AMAZON_KERNEL_IMAGE)
	@echo "Building kernel modules"
	$(hide) rm -rf $(AMAZON_KERNEL_MODULES_STAGING) $(AMAZON_KERNEL_MODULES_OUT)
	$(hide) mkdir -p $(AMAZON_KERNEL_MODULES_STAGING) $(AMAZON_KERNEL_MODULES_OUT)
	$(hide) $(MAKE) -C $(AMAZON_KERNEL_SOURCE) $(AMAZON_KERNEL_MAKE_FLAGS) modules
	$(hide) $(MAKE) -C $(AMAZON_KERNEL_SOURCE) $(AMAZON_KERNEL_MAKE_FLAGS) INSTALL_MOD_PATH=$(abspath $(AMAZON_KERNEL_MODULES_STAGING)) INSTALL_MOD_STRIP=1 modules_install
	$(hide) find $(AMAZON_KERNEL_MODULES_STAGING)/lib/modules -type f -name '*.ko' -exec cp -f {} $(AMAZON_KERNEL_MODULES_OUT)/ \;
	$(hide) touch $@

kernel: $(AMAZON_KERNEL_MODULES_STAMP)
ifneq ($(strip $(INSTALLED_VENDORIMAGE_TARGET)),)
$(INSTALLED_VENDORIMAGE_TARGET): $(AMAZON_KERNEL_MODULES_STAMP)
endif
ifneq ($(strip $(BUILT_TARGET_FILES_PACKAGE)),)
$(BUILT_TARGET_FILES_PACKAGE): $(AMAZON_KERNEL_MODULES_STAMP)
endif
endif

.PHONY: kernel kernelconfig kernelsavedefconfig clean-kernel
kernel: $(INSTALLED_KERNEL_TARGET)

kernelconfig: $(AMAZON_KERNEL_CONFIG)
	$(hide) $(MAKE) -C $(AMAZON_KERNEL_SOURCE) $(AMAZON_KERNEL_MAKE_FLAGS) menuconfig

kernelsavedefconfig: $(AMAZON_KERNEL_CONFIG)
	$(hide) $(MAKE) -C $(AMAZON_KERNEL_SOURCE) $(AMAZON_KERNEL_MAKE_FLAGS) savedefconfig
	$(hide) cp $(AMAZON_KERNEL_OUT)/defconfig $(AMAZON_KERNEL_DEFCONFIG_FILE)

clean-kernel:
	$(hide) rm -rf $(AMAZON_KERNEL_OUT) $(AMAZON_KERNEL_MODULES_STAGING) $(AMAZON_KERNEL_MODULES_OUT) $(INSTALLED_KERNEL_TARGET)

endif
endif
