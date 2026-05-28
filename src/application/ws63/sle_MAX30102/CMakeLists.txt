set(COMPONENT_NAME "sle_MAX30102")

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_LED_SERVER)
    set(SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/SLE_LED_Server/src/SLE_LED_Server.c
        ${CMAKE_CURRENT_SOURCE_DIR}/SLE_LED_Server/src/aht20_test.c
        ${CMAKE_CURRENT_SOURCE_DIR}/SLE_LED_Server/src/ssd1306_fonts.c
        ${CMAKE_CURRENT_SOURCE_DIR}/SLE_LED_Server/src/ssd1306.c
        ${CMAKE_CURRENT_SOURCE_DIR}/SLE_LED_Server/src/MAX30102.c
        ${CMAKE_CURRENT_SOURCE_DIR}/SLE_LED_Server/src/blood.c
        ${CMAKE_CURRENT_SOURCE_DIR}/SLE_LED_Server/src/algorithm.c
    )
    set(MAIN_COMPONENT true)
else()
    set(SOURCES "")
    set(MAIN_COMPONENT false)
endif()

set(PUBLIC_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/SLE_LED_Server/inc
)

set(PRIVATE_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/SLE_LED_Server/inc
)

set(PRIVATE_DEFINES)
set(PUBLIC_DEFINES)
set(COMPONENT_PUBLIC_CCFLAGS)
set(COMPONENT_CCFLAGS)

set(WHOLE_LINK true)

build_component()
