LOCAL_PATH := $(call my-dir)

ifeq (,$(wildcard vendor/mediatek/proprietary/hardware/libpq))
include $(CLEAR_VARS)
LOCAL_MODULE := libpq_prot_mtk
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/system/lib64
LOCAL_MODULE_SUFFIX := .so
LOCAL_SHARED_LIBRARIES_64 := liblog libc++ libc libm libdl
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_MULTILIB := 64
LOCAL_SRC_FILES_64 := arm64/libpq_prot_mtk.so
include $(BUILD_PREBUILT)
endif

ifeq (,$(wildcard vendor/mediatek/proprietary/hardware/libpq))
include $(CLEAR_VARS)
LOCAL_MODULE := libpq_prot_mtk
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/system/lib
LOCAL_MODULE_SUFFIX := .so
LOCAL_SHARED_LIBRARIES := liblog libc++ libc libm libdl
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_MULTILIB := 32
LOCAL_SRC_FILES_32 := arm/libpq_prot_mtk.so
include $(BUILD_PREBUILT)
endif
