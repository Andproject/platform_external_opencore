LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
 	src/pvwavfileparser.cpp


LOCAL_MODULE := libpvwav

LOCAL_CFLAGS := -DPV_CPU_ARCH_VERSION=0  $(PV_CFLAGS)


LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/fileformats/wav/parser/src \
 	$(PV_TOP)/fileformats/wav/parser/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
 	include/pvwavfileparser.h

include $(BUILD_STATIC_LIBRARY)
