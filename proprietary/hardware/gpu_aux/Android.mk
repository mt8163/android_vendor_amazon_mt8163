
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	GpuAuxAPI.cpp \
	GuiExtAux.cpp \
	mtk_queue.cpp \
	mtk_gralloc.cpp \
	mtk_gralloc0.cpp \
	mtk_gralloc1.cpp \
	utils.cpp

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(TOP)/$(MTK_ROOT)/hardware/dpframework/inc \
	$(TOP)/$(MTK_ROOT)/hardware/include

LOCAL_SHARED_LIBRARIES := \
	libdpframework \
	liblog \
	libutils \
	libcutils \
	libhardware \
	libgralloc_extra

LOCAL_CFLAGS += -DLOG_TAG=\"GPUAUX\"

LOCAL_EXPORT_C_INCLUDE_DIRS := \
	$(LOCAL_PATH)/include


LOCAL_MODULE := libgpu_aux
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

