LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# copy-n-paste from Makefile.am (update for LEX/YACC)
starter_SOURCES := \
starter.c files.h \
parser/parser.c parser/lexer.c parser/conf_parser.c parser/conf_parser.h \
args.c args.h confread.c confread.h keywords.c keywords.h cmp.c cmp.h \
invokecharon.c invokecharon.h starterstroke.c starterstroke.h \
netkey.c netkey.h klips.c klips.h

LOCAL_SRC_FILES := $(filter %.c,$(starter_SOURCES))

# build starter ----------------------------------------------------------------

LOCAL_C_INCLUDES += \
        $(strongswan_PATH)/src/libcharon \
	$(strongswan_PATH)/src/libstrongswan \
	$(strongswan_PATH)/src/starter \
	$(strongswan_PATH)/src/stroke

LOCAL_CFLAGS := $(strongswan_CFLAGS) -DSTART_CHARON \
	-DIPSEC_SCRIPT='"ipsec"' \
	-DPLUGINS='"$(strongswan_STARTER_PLUGINS)"'

LOCAL_MODULE := starter

LOCAL_MODULE_TAGS := optional

LOCAL_ARM_MODE := arm

LOCAL_PRELINK_MODULE := false

LOCAL_REQUIRED_MODULES := stroke

LOCAL_SHARED_LIBRARIES += libstrongswan
# 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [START]
LOCAL_SHARED_LIBRARIES += libcutils
# 2016-03-02 protocol-iwlan@lge.com LGP_DATA_IWLAN [END]

include $(BUILD_EXECUTABLE)

