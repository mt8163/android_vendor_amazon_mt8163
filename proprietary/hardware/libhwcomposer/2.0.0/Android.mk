# build hwcomposer static library

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM).2.0.0
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_MODULE_CLASS := STATIC_LIBRARIES

LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_C_INCLUDES += \
	frameworks/native/services/surfaceflinger \
	$(TOP)/$(MTK_ROOT)/hardware/hwcomposer \
	$(TOP)/$(MTK_ROOT)/hardware/hwcomposer/include \
	$(TOP)/$(MTK_ROOT)/hardware/gralloc_extra/include \
	$(TOP)/$(MTK_ROOT)/hardware/dpframework/include \
	$(TOP)/$(MTK_ROOT)/hardware/gpu_ext/ged/include \
	$(TOP)/$(MTK_ROOT)/hardware/libgem/inc \
	$(TOP)/$(MTK_ROOT)/hardware/bwc/inc \
	$(TOP)/$(MTK_ROOT)/hardware/m4u/$(TARGET_BOARD_PLATFORM) \
	$(TOP)/$(MTK_ROOT)/hardware/perfservice/perfservicenative \
	$(LOCAL_PATH)/../$(TARGET_BOARD_PLATFORM) \
	$(LOCAL_PATH)/.. \
	$(TOP)/$(MTK_ROOT)/external/libion_mtk/include \
	$(TOP)/system/core/libion/include \
	$(TOP)/system/core/libsync/include \
	$(TOP)/system/core/libsync \
	$(TOP)/system/core/include \
	$(TOP)/system/core/base/include \
	frameworks/native/libs/nativewindow/include \
	frameworks/native/libs/nativebase/include \
	frameworks/native/libs/arect/include \

LOCAL_SHARED_LIBRARIES := \
	libui \
	libdpframework \
	libged

LOCAL_SRC_FILES := \
	hwc2.cpp \
	dispatcher.cpp \
	worker.cpp \
	display.cpp \
	hwdev.cpp \
	event.cpp \
	overlay.cpp \
	queue.cpp \
	sync.cpp \
	composer.cpp \
	blitdev.cpp \
	bliter.cpp \
	bliter_async.cpp \
	bliter_ultra.cpp \
	platform_common.cpp \
	post_processing.cpp \
	../utils/tools.cpp \
	../utils/debug.cpp \
	../utils/transform.cpp \
	../utils/devicenode.cpp \
	color.cpp \
	asyncblitdev.cpp

LOCAL_SRC_FILES += \
	../$(TARGET_BOARD_PLATFORM)/platform.cpp

LOCAL_CFLAGS:= \
	-DLOG_TAG=\"hwcomposer\"

ifeq ($(strip $(TARGET_BUILD_VARIANT)), user)
LOCAL_CFLAGS += -DMTK_USER_BUILD
endif

ifneq ($(strip $(BOARD_VNDK_SUPPORT)),current)
LOCAL_CFLAGS += -DBOARD_VNDK_SUPPORT
endif

#LOCAL_CFLAGS += -DUSE_WDT_IOCTL

LOCAL_CFLAGS += -DUSE_NATIVE_FENCE_SYNC

LOCAL_CFLAGS += -DUSE_SYSTRACE

LOCAL_CFLAGS += -DMTK_HWC_VER_2_0

LOCAL_CFLAGS += -DUSE_HWC2

ifneq ($(MTK_BASIC_PACKAGE), yes)
	LOCAL_CFLAGS += -DUSE_SWWATCHDOG
endif

ifneq ($(findstring 7.,$(PLATFORM_VERSION)),)
	LOCAL_C_INCLUDES += \
		$(TOP)/$(MTK_ROOT)/frameworks/av/drm/widevine/libwvdrmengine/hdcpinfo/include \
		$(TOP)/$(MTK_ROOT)/hardware/perfservice/perfservicenative
else

ifneq ($(MTK_BASIC_PACKAGE), yes)
	LOCAL_C_INCLUDES += \
		$(TOP)/$(MTK_ROOT)/hardware/pq/v2.0/include \

	LOCAL_SHARED_LIBRARIES += \
		vendor.mediatek.hardware.pq@2.0 \
		android.hardware.power@1.0 \
		vendor.mediatek.hardware.power@2.0

	LOCAL_CFLAGS += -DUSES_PQSERVICE -DUSES_POWERHAL
endif

	gralloc0_platform := #mt8163

ifneq ($(TARGET_BOARD_PLATFORM), $(filter $(TARGET_BOARD_PLATFORM), $(gralloc0_platform)))
	LOCAL_CFLAGS += -DUSES_GRALLOC1
endif

endif

#LOCAL_CFLAGS += -DMTK_HWC_PROFILING


include $(BUILD_STATIC_LIBRARY)

