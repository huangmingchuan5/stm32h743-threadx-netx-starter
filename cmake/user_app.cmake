set(APP_SOURCES
        ${CMAKE_SOURCE_DIR}/User/main.c
        ${CMAKE_SOURCE_DIR}/User/bsp/src/bsp_eth.c
        ${CMAKE_SOURCE_DIR}/User/app/src/threadx_kernel.c
        ${CMAKE_SOURCE_DIR}/User/app/src/threadx_printf.c
        ${CMAKE_SOURCE_DIR}/User/app/src/threadx_task_manager.c
        ${CMAKE_SOURCE_DIR}/User/app/src/app_eth.c
        ${CMAKE_SOURCE_DIR}/User/app/src/app_eth_config.c
        ${CMAKE_SOURCE_DIR}/User/app/src/app_eth_dhcp.c
        ${CMAKE_SOURCE_DIR}/User/app/src/app_eth_link_check.c
        ${CMAKE_SOURCE_DIR}/User/app/src/app_tcp_echo.c
)

set(APP_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/User/app/inc
        ${CMAKE_SOURCE_DIR}/User/netxduo
)

configure_user_common(app)
target_include_directories(app PRIVATE ${APP_INCLUDE_DIRS})
target_sources(app PRIVATE ${APP_SOURCES})
configure_hal_firmware(app ${APP_FLASH_ORIGIN} ${APP_FLASH_LENGTH} ${APP_VECTOR_OFFSET})
target_link_options(app PRIVATE
        -Wl,-u,_printf_float
        -Wl,-u,_scanf_float
)
