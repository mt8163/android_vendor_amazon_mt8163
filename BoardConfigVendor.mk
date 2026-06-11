#
# Copyright (C) 2019-2020 The Lineageos Project
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
#

TARGET_SPECIFIC_HEADER_PATH += vendor/amazon/mt8163/proprietary/hardware/include

TARGET_BOOTLOADER_BOARD_NAME := mt8163

# MTK Platform Paths
MTK_PATH_COMMON := vendor/amazon/mt8163/proprietary/custom/common
MTK_PATH_CUSTOM_PLATFORM := vendor/amazon/mt8163/proprietary/custom/$(MTK_PLATFORM_DIR)
MTK_PATH_CUSTOM := vendor/amazon/mt8163/proprietary/custom/$(MTK_PROJECT)
MTK_PATH_SOURCE := vendor/amazon/mt8163/proprietary
MTK_ROOT := vendor/amazon/mt8163/proprietary

# Kernel
TARGET_KERNEL_SOURCE ?= kernel/amazon/karnak
TARGET_KERNEL_ARCH ?= arm64
TARGET_KERNEL_CLANG_COMPILE ?= false
AMAZON_KERNEL_CROSS_COMPILE_PREFIX ?= $(abspath prebuilts/linaro/linux-x86/aarch64/aarch64-linux-gnu/bin/aarch64-linux-gnu-)
AMAZON_KERNEL_CROSS_COMPILE_ARM32_PREFIX ?= $(abspath prebuilts/gcc/$(HOST_PREBUILT_TAG)/arm/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-)
TARGET_KERNEL_CROSS_COMPILE_PREFIX ?= $(abspath prebuilts/linaro/linux-x86/aarch64/aarch64-linux-gnu/bin/aarch64-linux-gnu-)
TARGET_KERNEL_CROSS_COMPILE_ARM32_PREFIX ?= $(abspath prebuilts/gcc/$(HOST_PREBUILT_TAG)/arm/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-)
