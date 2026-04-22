include $(RTOSLITETOPDIR)/config.mk

MODULE_NAME := $(notdir $(shell pwd))


LOCAL_SRCS_$(LOSCFG_COMPAT_FREERTOS) += $(wildcard *.c)
LOCAL_SRCS_$(LOSCFG_COMPAT_FREERTOS) += $(RTOSLITETOPDIR)/open_source/FreeRTOS/FreeRTOS-Kernel-11.1.0/stream_buffer.c
ifneq ($(LOSCFG_COMPAT_FREERTOS_HEAP), 3)
LOCAL_SRCS_$(LOSCFG_COMPAT_FREERTOS) +=$(filter-out heap_3.c,$(wildcard *.c))
LOCAL_SRCS_$(LOSCFG_COMPAT_FREERTOS) += $(RTOSLITETOPDIR)/open_source/FreeRTOS/FreeRTOS-Kernel-11.1.0/portable/MemMang/heap_${LOSCFG_COMPAT_FREERTOS_HEAP}.c
endif

LOCAL_SRCS += $(LOCAL_SRCS_y)
LOCAL_SRCS_NEED_SORT := y

LOCAL_INCLUDE += -I $(RTOSLITETOPDIR)/open_source/FreeRTOS/FreeRTOS-Kernel-11.1.0/include/
LOCAL_INCLUDE += -I $(RTOSLITETOPDIR)/self_src/compat/freertos/include/

LOCAL_FLAGS := $(LOCAL_INCLUDE) $(LITEOS_GCOV_OPTS) $(LITEOS_NON_SECURE_LOCAL_OPTS)

include $(MODULE)
