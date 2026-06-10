#===============================================================================
# @brief    cmake file
# Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2022. All rights reserved.
#===============================================================================
set(COMPONENT_NAME "watchdevice")

if(DEFINED CONFIG_ENABLE_WATCHDEVICE)
    set(SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/sensor_task.c
        ${CMAKE_CURRENT_SOURCE_DIR}/sle_speed_client.c
    )
    set(MAIN_COMPONENT true)
else()
    set(SOURCES "")
    set(MAIN_COMPONENT false)
endif()

set(PUBLIC_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(PRIVATE_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ROOT_DIR}/kernel/osal/include
    ${ROOT_DIR}/kernel/osal/include/msgqueue
    ${ROOT_DIR}/include
    ${ROOT_DIR}/middleware/services/bts/sle/inc
    ${ROOT_DIR}/middleware/services/bts/ble/inc
    ${ROOT_DIR}/middleware/utils/dfx/log/inc
    ${ROOT_DIR}/drivers/chips/ws63/porting/inc
    ${ROOT_DIR}/include/driver
    ${ROOT_DIR}/middleware/services/bts/sle/inc         # <--- 必须加上星闪的头文件路径
    ${ROOT_DIR}/middleware/services/bts/ble/inc
)

set(PRIVATE_DEFINES)
set(PUBLIC_DEFINES)
set(COMPONENT_PUBLIC_CCFLAGS)
set(COMPONENT_CCFLAGS)

set(WHOLE_LINK true)

build_component()