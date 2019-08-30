#
# Copyright (C) 2019 The LineageOS Project
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
LOCAL_MODULE := build_kernel
TOOLCHAIN_DIR := $(TOP)/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
LOCAL_PATH := $(call my-dir)
KERNEL_OUT := out/target/product/karnak/KERNEL_OBJ
$(shell mkdir -p out/target/product/karnak/KERNEL_OBJ)
$(shell make -C kernel/amazon/karnak  O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(TOOLCHAIN_DIR) karnak_defconfig)
$(shell make O=$(KERNEL_OUT) -C $(TOP)/kernel/amazon/karnak ARCH=arm64  CROSS_COMPILE=$(TOOLCHAIN_DIR) -j8)



