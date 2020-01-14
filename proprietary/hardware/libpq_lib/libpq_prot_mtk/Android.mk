LOCAL_PATH := $(call my-dir)

ifeq (,$(TARGET_BRM_PLATFORM))
    $(error TAREGT_BRM_PLATFORM is undefined. Please make sure the device-vendor.mk is correctly included.)
endif

include $(wildcard $(LOCAL_PATH)/$(TARGET_BRM_PLATFORM)/Android.mk)
