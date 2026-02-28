LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libneutron
LOCAL_CATEGORY_PATH := libs
LOCAL_DESCRIPTION := Toolkit for developing linux system applications
LOCAL_EXPORT_LDLIBS := -lneutron

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)/include

LOCAL_CMAKE_CONFIGURE_ARGS := \
	-DBUILD_EXAMPLES=OFF

LOCAL_CMAKE_CONFIGURE_ENV +=\
	BUILD_TYPE=Debug \
	VERSION=1.0.0

include $(BUILD_CMAKE)
