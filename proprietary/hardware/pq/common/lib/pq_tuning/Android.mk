ifneq ($(MTK_BASIC_PACKAGE), yes)
LOCAL_PATH := $(call my-dir)

LOCAL_SRC_FILES := \
	pq_tuning_jni.cpp

LOCAL_C_INCLUDES := $(JNI_H_INCLUDE)

LOCAL_C_INCLUDES += \
    $(TOP)/$(MTK_PATH_SOURCE)/platform/$(MTK_PLATFORM_DIR)/kernel/drivers/dispsys \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/pq/v2.0/include \
    $(TOP)/frameworks/base/include \

LOCAL_SHARED_LIBRARIES := \
  libutils \
  libcutils \
  liblog

ifeq (,$(filter $(strip $(MTK_PQ_SUPPORT)), no PQ_OFF))
    LOCAL_SHARED_LIBRARIES += \
        libhidlbase \
        vendor.mediatek.hardware.pq@2.0
endif

LOCAL_MODULE := libPQjni
LOCAL_MODULE_OWNER := mtk
LOCAL_MULTILIB := both

include $(MTK_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
endif
