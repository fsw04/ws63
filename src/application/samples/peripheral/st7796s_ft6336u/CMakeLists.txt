set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/display.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/watch_app.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/ui/watch_ui.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/model/watch_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/business/watch_borrow.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/lcd_bus.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/st7796s.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/ft6336u.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/m1_pn532.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/services/sle_watch_server.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/services/wifi_task.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/services/wifi_provision.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/services/mqtt_task.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/services/vitals_report.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/tasks/watch_nfc_task.c"
    PARENT_SCOPE)