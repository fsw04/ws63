#===============================================================================
# @brief    cmake file
#===============================================================================
set(COMPONENT_NAME "lcd_touch")

if(DEFINED CONFIG_ENABLE_LCD_TOUCH)
    set(SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/lcd.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ft6336.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lcd_touch_demo.c
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
    ${ROOT_DIR}/include
    ${ROOT_DIR}/include/driver
    ${ROOT_DIR}/drivers/chips/ws63/porting/inc
    ${ROOT_DIR}/open_source/lvgl
)

set(PRIVATE_DEFINES)
set(PUBLIC_DEFINES)
set(COMPONENT_PUBLIC_CCFLAGS)
set(COMPONENT_CCFLAGS)

set(WHOLE_LINK true)

build_component()
