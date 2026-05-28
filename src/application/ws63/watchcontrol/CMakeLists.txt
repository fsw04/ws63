#===============================================================================
# @brief    cmake file
# Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022-2022. All rights reserved.
#===============================================================================
set(COMPONENT_NAME "watchcontrol")

if(DEFINED CONFIG_ENABLE_WATCHCONTROL)
    set(SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/wifi_task.c
        ${CMAKE_CURRENT_SOURCE_DIR}/mqtt_task.c
        ${CMAKE_CURRENT_SOURCE_DIR}/sle_speed_server.c
        ${CMAKE_CURRENT_SOURCE_DIR}/sle_speed_server_adv.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/display_port.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/ft6336u.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/touch_port.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_task.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_style.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_status_page.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_devices_page.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_wifi_page.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_sle_page.c
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
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
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
    ${ROOT_DIR}/open_source/lvgl
    ${ROOT_DIR}/open_source/lvgl/src
)

set(PRIVATE_DEFINES
    LV_FONT_MONTSERRAT_10=1
    LV_FONT_MONTSERRAT_12=1
    LV_FONT_MONTSERRAT_16=1
)
set(PUBLIC_DEFINES)
set(COMPONENT_PUBLIC_CCFLAGS)
set(COMPONENT_CCFLAGS)

set(WHOLE_LINK true)

build_component()
