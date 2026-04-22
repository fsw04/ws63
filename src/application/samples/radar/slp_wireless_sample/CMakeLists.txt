#===============================================================================
# @brief    cmake file
# Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2025-2025. All rights reserved.
#===============================================================================
set(SOURCES
    "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/radar_slp_udp_socket.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/radar_slp_tcp_socket.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/radar_slp_wireless_sample.c"
    PARENT_SCOPE)

set(PUBLIC_HEADER
    "${PUBLIC_HEADER}"
    PARENT_SCOPE)
