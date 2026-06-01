#===============================================================================
# @brief    ST7796S LCD + FT6336U touch sample
#===============================================================================
set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/display.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ft6336u.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd_bus.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/mqtt_task.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sle_watch_server.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/st7796s.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/watch_app.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/watch_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/watch_ui.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/wifi_task.c"
    PARENT_SCOPE)
