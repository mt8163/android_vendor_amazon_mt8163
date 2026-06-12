# Copyright (C) 2026 The Android Open Source Project
# Copyright (C) 2017-2026 The LineageOS Project
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

# Internal variables
KERNEL_SRC := $(TARGET_KERNEL_SOURCE)
KERNEL_OUT := $(PRODUCT_OUT)/obj/KERNEL_OBJ
KERNEL_CONFIG := $(KERNEL_OUT)/.config
KERNEL_ARCH := $(strip $(TARGET_KERNEL_ARCH))
ifeq ($(KERNEL_ARCH),)
KERNEL_ARCH := $(TARGET_ARCH)
endif

KERNEL_DEFCONFIG := $(word 1,$(TARGET_KERNEL_CONFIG))
ifeq ($(KERNEL_DEFCONFIG),)
$(error TARGET_KERNEL_CONFIG must be set to build the kernel)
endif
ifeq ($(BOARD_KERNEL_IMAGE_NAME),)
$(error BOARD_KERNEL_IMAGE_NAME must be set to build the kernel)
endif
ifeq ($(wildcard $(KERNEL_SRC)/Makefile),)
$(error TARGET_KERNEL_SOURCE points to missing kernel source: $(KERNEL_SRC))
endif

KERNEL_DEFCONFIG_FILE := $(KERNEL_SRC)/arch/$(KERNEL_ARCH)/configs/$(KERNEL_DEFCONFIG)
ifeq ($(wildcard $(KERNEL_DEFCONFIG_FILE)),)
$(error TARGET_KERNEL_CONFIG points to missing defconfig: $(KERNEL_DEFCONFIG_FILE))
endif

KERNEL_IMAGE := $(KERNEL_OUT)/arch/$(KERNEL_ARCH)/boot/$(BOARD_KERNEL_IMAGE_NAME)
KERNEL_MODULES_STAGING := $(call intermediates-dir-for,PACKAGING,kernel_modules)
KERNEL_MODULES_OUT := $(TARGET_OUT_VENDOR)/lib/modules
KERNEL_MODULES_STAMP := $(KERNEL_MODULES_STAGING)/modules.timestamp

# Toolchain setup
KERNEL_MAKE := $(abspath prebuilts/build-tools/linux-x86/bin/make)
KERNEL_PATH_OVERRIDE := PATH=$(abspath prebuilts/build-tools/linux-x86/bin):/usr/bin:$(abspath $(HOST_OUT_EXECUTABLES)):$$PATH

KERNEL_CROSS_COMPILE := $(strip $(AMAZON_KERNEL_CROSS_COMPILE_PREFIX))
ifeq ($(KERNEL_CROSS_COMPILE),)
KERNEL_CROSS_COMPILE := $(strip $(TARGET_KERNEL_CROSS_COMPILE_PREFIX))
endif
ifeq ($(KERNEL_CROSS_COMPILE),)
ifeq ($(KERNEL_ARCH),arm64)
KERNEL_CROSS_COMPILE := $(abspath prebuilts/gcc/$(HOST_PREBUILT_TAG)/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-)
else ifeq ($(KERNEL_ARCH),arm)
KERNEL_CROSS_COMPILE := $(abspath prebuilts/gcc/$(HOST_PREBUILT_TAG)/arm/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-)
endif
endif

KERNEL_MAKE_FLAGS := O=$(abspath $(KERNEL_OUT)) ARCH=$(KERNEL_ARCH) ROOTDIR=$(abspath .) HOSTCC=gcc HOSTCXX=g++
ifneq ($(KERNEL_CROSS_COMPILE),)
KERNEL_MAKE_FLAGS += CROSS_COMPILE=$(KERNEL_CROSS_COMPILE)
endif

ifeq ($(KERNEL_ARCH),arm64)
KERNEL_CROSS_COMPILE_ARM32 := $(strip $(AMAZON_KERNEL_CROSS_COMPILE_ARM32_PREFIX))
ifeq ($(KERNEL_CROSS_COMPILE_ARM32),)
KERNEL_CROSS_COMPILE_ARM32 := $(strip $(TARGET_KERNEL_CROSS_COMPILE_ARM32_PREFIX))
endif
ifeq ($(KERNEL_CROSS_COMPILE_ARM32),)
KERNEL_CROSS_COMPILE_ARM32 := $(abspath prebuilts/gcc/$(HOST_PREBUILT_TAG)/arm/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-)
endif
KERNEL_MAKE_FLAGS += CROSS_COMPILE_ARM32=$(KERNEL_CROSS_COMPILE_ARM32)
KERNEL_MAKE_FLAGS += CROSS_COMPILE_COMPAT=$(KERNEL_CROSS_COMPILE_ARM32)
endif

# Clang support
ifeq ($(TARGET_KERNEL_CLANG_COMPILE),true)
KERNEL_CLANG_PATH := $(abspath prebuilts/clang/host/linux-x86/clang-stable)
KERNEL_PATH_OVERRIDE := PATH=$(KERNEL_CLANG_PATH)/bin:$(abspath prebuilts/build-tools/linux-x86/bin):/usr/bin:$(abspath $(HOST_OUT_EXECUTABLES)):$$PATH
ifeq ($(KERNEL_ARCH),arm64)
KERNEL_CLANG_TRIPLE ?= CLANG_TRIPLE=aarch64-linux-gnu-
else ifeq ($(KERNEL_ARCH),arm)
KERNEL_CLANG_TRIPLE ?= CLANG_TRIPLE=arm-linux-gnu-
endif
KERNEL_MAKE_FLAGS += CC="clang" $(KERNEL_CLANG_TRIPLE)
endif

ifneq ($(SHOW_COMMANDS),)
KERNEL_MAKE_FLAGS += V=1
endif

# Internal implementation of make-kernel-target
define internal-make-kernel-target
$(KERNEL_PATH_OVERRIDE) $(KERNEL_MAKE) -C $(KERNEL_SRC) $(KERNEL_MAKE_FLAGS) $(1)
endef

$(KERNEL_CONFIG): $(KERNEL_DEFCONFIG_FILE)
	@echo "Kernel config: $@"
	$(hide) mkdir -p $(KERNEL_OUT)
	$(hide) $(call internal-make-kernel-target,$(KERNEL_DEFCONFIG))

$(KERNEL_IMAGE): $(KERNEL_CONFIG)
	@echo "Building kernel image: $(BOARD_KERNEL_IMAGE_NAME)"
	$(hide) $(call internal-make-kernel-target,$(BOARD_KERNEL_IMAGE_NAME))

$(INSTALLED_KERNEL_TARGET): $(KERNEL_IMAGE) | $(ACP)
	@echo "Install kernel: $@"
	$(copy-file-to-target)

ifeq ($(shell grep -q '^CONFIG_MODULES=y' $(KERNEL_DEFCONFIG_FILE) 2>/dev/null && echo true),true)
$(KERNEL_MODULES_STAMP): $(KERNEL_IMAGE) | signapk libconscrypt_openjdk_jni
	@echo "Building kernel modules"
	$(hide) rm -rf $(KERNEL_MODULES_STAGING)
	$(hide) mkdir -p $(KERNEL_MODULES_STAGING) $(KERNEL_MODULES_OUT)
	$(hide) $(call internal-make-kernel-target,modules)
	$(hide) $(call internal-make-kernel-target,INSTALL_MOD_PATH=$(abspath $(KERNEL_MODULES_STAGING)) INSTALL_MOD_STRIP=1 DEPMOD=/bin/true modules_install)
	$(hide) for f in $$(find $(KERNEL_MODULES_STAGING)/lib/modules -type f -name '*.ko' -o -name 'modules.builtin*'); do cp -f $$f $(KERNEL_MODULES_OUT)/; done
	$(hide) rm -f $(KERNEL_MODULES_OUT)/modules.load
	$(hide) for f in $$(find $(KERNEL_MODULES_OUT) -type f -name '*.ko'); do \
		name=$$(basename $$f); \
		grep -qw "$$name" $(KERNEL_MODULES_OUT)/modules.load || echo "$$name" >> $(KERNEL_MODULES_OUT)/modules.load; \
	done
	$(hide) touch $@

kernel: $(KERNEL_MODULES_STAMP)
ifneq ($(strip $(INSTALLED_VENDORIMAGE_TARGET)),)
$(INSTALLED_VENDORIMAGE_TARGET): $(KERNEL_MODULES_STAMP)
endif
ifneq ($(strip $(BUILT_TARGET_FILES_PACKAGE)),)
$(BUILT_TARGET_FILES_PACKAGE): $(KERNEL_MODULES_STAMP)
endif
endif

.PHONY: kernel kernelconfig kernelsavedefconfig clean-kernel
kernel: $(INSTALLED_KERNEL_TARGET) signapk libconscrypt_openjdk_jni

kernelconfig: $(KERNEL_CONFIG)
	$(hide) $(call internal-make-kernel-target,menuconfig)

kernelsavedefconfig: $(KERNEL_CONFIG)
	$(hide) $(call internal-make-kernel-target,savedefconfig)
	$(hide) cp $(KERNEL_OUT)/defconfig $(KERNEL_DEFCONFIG_FILE)

clean-kernel:
	$(hide) rm -rf $(KERNEL_OUT) $(KERNEL_MODULES_STAGING) $(KERNEL_MODULES_OUT) $(INSTALLED_KERNEL_TARGET)

endif
endif
