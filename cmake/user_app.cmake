set(APP_SOURCES
        ${CMAKE_SOURCE_DIR}/User/main.c
        ${CMAKE_SOURCE_DIR}/User/bsp/src/bsp.c
        ${CMAKE_SOURCE_DIR}/User/bsp/src/bsp_RTT_viewer.c
        ${CMAKE_SOURCE_DIR}/User/app/src/threadx_kernel.c
        ${CMAKE_SOURCE_DIR}/User/app/src/threadx_printf.c
        ${CMAKE_SOURCE_DIR}/User/app/src/threadx_task_manager.c
)

target_include_directories(app PRIVATE ${USER_INCLUDE_DIRS})
target_sources(app PRIVATE ${APP_SOURCES})
configure_hal_firmware(app ${APP_FLASH_ORIGIN} ${APP_FLASH_LENGTH} ${APP_VECTOR_OFFSET})
target_link_options(app PRIVATE
        -Wl,-u,_printf_float
        -Wl,-u,_scanf_float
)
