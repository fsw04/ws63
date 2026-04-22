STRING( REGEX REPLACE ".*/(.*)" "\\1" CURRENT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR} )
set(MODULE_NAME ${CURRENT_FOLDER})

set(LITEOS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

set(LITEOS_OPENSOURCE_RTT_PATH ${LITEOS_PATH}/open_source/rt-thread)

# 寻找所有rt-thread的.c文件
file(GLOB_RECURSE LITEOS_RTT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

# 设置需要排除的文件
set(REMOVE_SOURCES "")
list(REMOVE_ITEM LITEOS_RTT_PATH ${REMOVE_SOURCES})

# 添加头文件
set(MODULE_INCLUDE_RTT
    ${LITEOS_OPENSOURCE_RTT_PATH}/components/drivers/include
    ${LITEOS_OPENSOURCE_RTT_PATH}/include
    ${LITEOS_OPENSOURCE_RTT_PATH}/include/klibc
    ${LITEOS_OPENSOURCE_RTT_PATH}/components/utilities/ulog
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LITEOS_OPENSOURCE_RTT_PATH}/components/drivers/include/ipc
    ${LITEOS_OPENSOURCE_RTT_PATH}/components/libc/posix/libdl
)

include_directories(
    ${MODULE_INCLUDE_RTT}
)

# add global include, compile macro and options to public target #
set(MODULE_CXXINCLUDE_PUB)
set(MODULE_COPTS_PUB)
set(MODULE_ASOPTS_PUB)
set(MODULE_CXXOPTS_PUB)
set(MODULE_CMACRO_PUB)
set(MODULE_ASMACRO_PUB)
set(MODULE_CXXMACRO_PUB)


# add local include, compile macro and options to private target #
set(MODULE_COPTS_PRI ${LITEOS_NON_SECURE_LOCAL_OPTS})
set(MODULE_ASOPTS_PRI ${LITEOS_NON_SECURE_LOCAL_OPTS})
set(MODULE_CXXOPTS_PRI ${LITEOS_NON_SECURE_LOCAL_OPTS})
set(MODULE_CMACRO_PRI)
set(MODULE_ASMACRO_PRI)
set(MODULE_CXXMACRO_PRI)


# add srcs to private target #
list(APPEND LOCAL_SRCS_${LOSCFG_COMPAT_RT_THREAD} ${LITEOS_RTT_PATH})

list(APPEND LOCAL_SRCS_${LOSCFG_COMPAT_RT_THREAD} ${LITEOS_OPENSOURCE_RTT_PATH}/components/drivers/ipc/pipe.c)
list(APPEND LOCAL_SRCS_${LOSCFG_COMPAT_RT_THREAD} ${LITEOS_OPENSOURCE_RTT_PATH}/components/drivers/ipc/ringbuffer.c)
list(APPEND LOCAL_SRCS_${LOSCFG_COMPAT_RT_THREAD} ${LITEOS_OPENSOURCE_RTT_PATH}/components/drivers/ipc/ringblk_buf.c)
list(APPEND LOCAL_SRCS_${LOSCFG_COMPAT_RT_THREAD} ${LITEOS_OPENSOURCE_RTT_PATH}/components/drivers/ipc/condvar.c)
list(APPEND LOCAL_SRCS_${LOSCFG_COMPAT_RT_THREAD} ${LITEOS_OPENSOURCE_RTT_PATH}/components/drivers/core/device.c)
list(APPEND LOCAL_SRCS_${LOSCFG_COMPAT_RT_THREAD} ${LITEOS_OPENSOURCE_RTT_PATH}/src/klibc/kerrno.c)
list(APPEND LOCAL_SRCS_${LOSCFG_COMPAT_RT_THREAD} ${LITEOS_OPENSOURCE_RTT_PATH}/src/kservice.c)

add_compile_options(-Wno-sign-compare)
add_compile_options(-Wno-error=date-time)
add_compile_options(-Wno-cpp)

set(LOCAL_SRCS ${LOCAL_SRCS_y})
include(${MODULE})
