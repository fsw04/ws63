cmake_minimum_required(VERSION 3.10)
project(watchcontrol_simulator C)

set(CMAKE_C_STANDARD 11)

set(LVGL_DIR "${CMAKE_SOURCE_DIR}/../src/open_source/lvgl")
set(UI_SRC_DIR "${CMAKE_SOURCE_DIR}/../src/application/ws63/watchcontrol")

file(GLOB_RECURSE LVGL_SOURCES ${LVGL_DIR}/src/*.c)

add_executable(${PROJECT_NAME}
    main.c
    ${UI_SRC_DIR}/ui_style.c
    ${UI_SRC_DIR}/ui_status_page.c
    ${UI_SRC_DIR}/ui_devices_page.c
    ${UI_SRC_DIR}/ui_wifi_page.c
    ${UI_SRC_DIR}/ui_sle_page.c
    ${LVGL_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${LVGL_DIR}
    ${LVGL_DIR}/src
    ${UI_SRC_DIR}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    LV_CONF_INCLUDE_SIMPLE
    SIMULATOR=1
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-variable)
endif()

find_package(SDL2 QUIET)
if(SDL2_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES})
    if(SDL2_MAIN_LIBRARY)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_MAIN_LIBRARY})
    endif()
else()
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2_PKG sdl2)
        if(SDL2_PKG_FOUND)
            target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_PKG_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_PKG_LIBRARIES})
        endif()
    endif()
    if(NOT SDL2_PKG_FOUND)
        if(WIN32)
            set(SDL2_DIR "$ENV{SDL2_DIR}" CACHE PATH "Path to SDL2 SDK root")
            if(EXISTS "${SDL2_DIR}/include/SDL.h")
                target_include_directories(${PROJECT_NAME} PRIVATE "${SDL2_DIR}/include")
                if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                    target_link_directories(${PROJECT_NAME} PRIVATE "${SDL2_DIR}/lib/x64")
                else()
                    target_link_directories(${PROJECT_NAME} PRIVATE "${SDL2_DIR}/lib/x86")
                endif()
                target_link_libraries(${PROJECT_NAME} PRIVATE SDL2 SDL2main)
            else()
                message(FATAL_ERROR "SDL2 not found. Set SDL2_DIR environment variable to SDL2 SDK root path.")
            endif()
        else()
            message(FATAL_ERROR "SDL2 not found. Install libsdl2-dev (Debian/Ubuntu) or sdl2 (Homebrew).")
        endif()
    endif()
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE m pthread)
endif()
