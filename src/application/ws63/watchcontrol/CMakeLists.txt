#===============================================================================
# @brief    cmake file
# Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2022. All rights reserved.
#===============================================================================
set(COMPONENT_NAME "watchcontrol")

# 核心修改：在这里判断 Kconfig 开关
if(DEFINED CONFIG_ENABLE_WATCHCONTROL)
    # 如果勾选了，正常编译这4个C文件，并作为主组件链接
    set(SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/wifi_task.c
        ${CMAKE_CURRENT_SOURCE_DIR}/mqtt_task.c
        ${CMAKE_CURRENT_SOURCE_DIR}/sle_speed_server.c    
        ${CMAKE_CURRENT_SOURCE_DIR}/sle_speed_server_adv.c
    )
    set(MAIN_COMPONENT true)
else()
    # 如果没有勾选，设置SOURCES为空，编译系统会自动生成一个空壳库骗过链接器
    set(SOURCES "")
    set(MAIN_COMPONENT false)
endif()

set(PUBLIC_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 在这里添加所有的依赖头文件路径。这里使用 ${ROOT_DIR} 指向 src/ 根目录
set(PRIVATE_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ROOT_DIR}/kernel/osal/include
    ${ROOT_DIR}/kernel/osal/include/msgqueue
    ${ROOT_DIR}/include
    ${ROOT_DIR}/open_source/lwip/lwip_v2.1.3/src/include
    ${ROOT_DIR}/open_source/mqtt/paho.mqtt.c/src
    ${ROOT_DIR}/middleware/services/wifi_service/include
    ${ROOT_DIR}/kernel/osal/src/liteos
    ${ROOT_DIR}/drivers/chips/ws63/porting/inc
    ${ROOT_DIR}/include/driver
    ${ROOT_DIR}/middleware/services/bts/sle/inc
    ${ROOT_DIR}/middleware/services/bts/ble/inc
)

set(PRIVATE_DEFINES)
set(PUBLIC_DEFINES)
set(COMPONENT_PUBLIC_CCFLAGS)
set(COMPONENT_CCFLAGS)

set(WHOLE_LINK true)

build_component()