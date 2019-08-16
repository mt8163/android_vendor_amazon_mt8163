include $(CLEAR_VARS)
LOCAL_MODULE = libstagefright_color_conversion
LOCAL_MODULE_CLASS = STATIC_LIBRARIES
LOCAL_MODULE_SUFFIX = .a
LOCAL_UNINSTALLABLE_MODULE = true
LOCAL_SHARED_LIBRARIES = libdpframework
LOCAL_SRC_FILES = libstagefright_color_conversion_32.a
include $(BUILD_PREBUILT)

