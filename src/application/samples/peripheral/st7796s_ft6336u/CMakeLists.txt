#===============================================================================
# @brief    ST7796S LCD + FT6336U touch sample
#===============================================================================
set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/display.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ft6336u.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd_bus.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/st7796s.c"
    PARENT_SCOPE)
