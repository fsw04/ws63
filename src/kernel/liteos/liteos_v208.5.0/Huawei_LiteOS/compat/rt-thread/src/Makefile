include $(RTOSLITETOPDIR)/config.mk

MODULE_NAME := $(notdir $(shell pwd))


LOCAL_SRCS_$(LOSCFG_COMPAT_RT_THREAD) += $(wildcard rt_*.c)
LOCAL_SRCS_$(LOSCFG_COMPAT_RT_THREAD) += $(wildcard core-extend/rt_*.c)
LOCAL_SRCS_$(LOSCFG_COMPAT_RT_THREAD) += $(RTOSLITETOPDIR)/open_source/rt-thread/components/drivers/ipc/pipe.c
LOCAL_SRCS_$(LOSCFG_COMPAT_RT_THREAD) += $(RTOSLITETOPDIR)/open_source/rt-thread/components/drivers/ipc/ringbuffer.c
LOCAL_SRCS_$(LOSCFG_COMPAT_RT_THREAD) += $(RTOSLITETOPDIR)/open_source/rt-thread/components/drivers/ipc/ringblk_buf.c
LOCAL_SRCS_$(LOSCFG_COMPAT_RT_THREAD) += $(RTOSLITETOPDIR)/open_source/rt-thread/components/drivers/ipc/condvar.c
LOCAL_SRCS_$(LOSCFG_COMPAT_RT_THREAD) += $(RTOSLITETOPDIR)/open_source/rt-thread/components/drivers/core/device.c
LOCAL_SRCS_$(LOSCFG_COMPAT_RT_THREAD) += $(RTOSLITETOPDIR)/open_source/rt-thread/src/klibc/kerrno.c
LOCAL_SRCS_$(LOSCFG_COMPAT_RT_THREAD) += $(RTOSLITETOPDIR)/open_source/rt-thread/src/kservice.c
LOCAL_SRCS += $(LOCAL_SRCS_y)
LOCAL_SRCS_NEED_SORT := y

LOCAL_INCLUDE += -I $(RTOSLITETOPDIR)/open_source/rt-thread/components/drivers/include/ipc
LOCAL_INCLUDE += -I $(RTOSLITETOPDIR)/open_source/rt-thread/components/libc/posix/libdl
LOCAL_INCLUDE += -I $(RTOSLITETOPDIR)/open_source/rt-thread/components/drivers/include
LOCAL_INCLUDE += -I $(RTOSLITETOPDIR)/open_source/rt-thread/include
LOCAL_INCLUDE += -I $(RTOSLITETOPDIR)/open_source/rt-thread/include/klibc
LOCAL_INCLUDE += -I $(RTOSLITETOPDIR)/open_source/rt-thread/components/utilities/ulog
LOCAL_INCLUDE += -I $(RTOSLITETOPDIR)/self_src/compat/rt-thread/include


LOCAL_FLAGS := $(LOCAL_INCLUDE) $(LITEOS_GCOV_OPTS) $(LITEOS_NON_SECURE_LOCAL_OPTS)
LOCAL_FLAGS += -Wno-sign-compare
LOCAL_FLAGS += -Wno-date-time
LOCAL_FLAGS += -Wno-cpp

include $(MODULE)
