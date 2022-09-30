# make sure that the environment variables ARCH and CROSS_COMPILE
# are set for your architecture and cross compiler
#
# example1
# export ARCH := arm
# export CROSS_COMPILE := arm-linux-gnueabihf-
#
# example2
# export ARCH := arm64
# export CROSS_COMPILE := aarch64-linux-android-

#MODULE_NAME = snd-soc-tfa98xx

# additional debugging
#TFA_DEBUG=1

subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common

# debugging support (also enables trace_printk)
ccflags-y += -Isound/soc/codecs/tfa9878/inc
ifeq ($(CONFIG_SND_SOC_TFA9878),y)
ccflags-y += -DUSE_TFA9878
endif
ccflags-y += -DMPLATFORM
ccflags-y += -DLIMIT_CAL_FROM_DTS
ccflags-y += -DTFADSP_32BITS
ccflags-y += -DTFADSP_DSP_MSG_PACKET_STRATEGY
ccflags-y += -DTFADSP_DSP_BUFFER_POOL
#ccflags-y += -DTFA_EXCEPTION_AT_TRANSITION
ccflags-y += -DTFA_USE_DEVICE_SPECIFIC_CONTROL
ccflags-y += -DTFA_PROFILE_ON_DEVICE
ccflags-y += -DTFA_NO_SND_FORMAT_CHECK
ccflags-y += -DTFA_VOID_APIV_IN_FILE
ccflags-y += -DTFA_SET_EXT_INTERNALLY
#ccflags-y += -DTFA_MUTE_DURING_SWITCHING_PROFILE
ccflags-y += -DTFA_RAMPDOWN_BEFORE_MUTE
ccflags-y += -DTFA_RECONFIG_WITHOUT_RESET
ccflags-y += -DTFA_USE_MANSTATE_TO_CHECK_COLD
#ccflags-y += -DTFA_STOP_AT_START_FAILURE
ccflags-y += -DTFA_BYPASS_AT_START_FAILURE
ccflags-y += -DTFA_REDUCE_BYPASS_GAIN

#ccflags-y += -DTFA_UNCONDITIONAL_PASS_FOR_TEST
ccflags-y += -DTFA_IGNORE_PROBE_FAIL
#ccflags-y += -DDEBUG -DTFA_NON_DSP_SOLUTION
ccflags-y += -Werror
ccflags-y += $(call cc-option,-Wno-date-time,)

snd-soc-tfa98xx-objs += tfa98xx.o
snd-soc-tfa98xx-objs += tfa_container.o
snd-soc-tfa98xx-objs += tfa_dsp.o
snd-soc-tfa98xx-objs += tfa_init.o

ifdef TFA_DEBUG
ccflags-y += -DTFA_DEBUG -DDEBUG
snd-soc-tfa98xx-objs += tfa_debug.o
endif

obj-y += snd-soc-tfa98xx.o

