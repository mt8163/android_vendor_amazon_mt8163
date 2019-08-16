LOCAL_PATH := $(call my-dir)


include $(CLEAR_VARS)
LOCAL_MULTILIB := 32
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := thermal_manager.c
LOCAL_SHARED_LIBRARIES := libdl libcutils liblog
LOCAL_MODULE := thermal_manager
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_EXECUTABLE)



