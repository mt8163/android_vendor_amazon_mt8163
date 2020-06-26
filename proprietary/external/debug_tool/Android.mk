LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=   \
    src/debug_tool.c \
    src/functions.c

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -Wno-missing-field-initializers -Wno-type-limits -Wno-tautological-constant-out-of-range-compare -Wno-multichar

LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE:= debug_tool

include $(BUILD_EXECUTABLE)
