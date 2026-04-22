#===============================================================================
# @brief    cmake file
# Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2022. All rights reserved.
#===============================================================================
set(COMPONENT_NAME "watchcontrol")

# 注意：SOURCES中的文件列表不需要逗号结尾
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/wifi_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mqtt_task.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sensor_task.c
)

set(PUBLIC_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 在这里添加所有的依赖头文件路径。这里使用 ${ROOT_DIR} 指向 src/ 根目录
set(PRIVATE_HEADER
    ${ROOT_DIR}/kernel/osal/include
    ${ROOT_DIR}/kernel/osal/include/msgqueue
    ${ROOT_DIR}/include
    ${ROOT_DIR}/open_source/lwip/lwip_v2.1.3/src/include
    ${ROOT_DIR}/open_source/mqtt/paho.mqtt.c/src
    ${ROOT_DIR}/middleware/services/wifi_service/include
    ${ROOT_DIR}/kernel/osal/src/liteos
    ${ROOT_DIR}/drivers/chips/ws63/porting/inc
    ${ROOT_DIR}/include/driver
)

set(PRIVATE_DEFINES
)

set(PUBLIC_DEFINES
)

# use this when you want to add ccflags like -include xxx
set(COMPONENT_PUBLIC_CCFLAGS
)

set(COMPONENT_CCFLAGS
)

set(WHOLE_LINK
    true
)

set(MAIN_COMPONENT
    true
)

build_component()