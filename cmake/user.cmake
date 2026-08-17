set(USER_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/User
        ${CMAKE_SOURCE_DIR}/User/core
        ${CMAKE_SOURCE_DIR}/User/bsp/inc
        ${CMAKE_SOURCE_DIR}/User/app/inc
)

set(USER_SOURCES
        ${CMAKE_SOURCE_DIR}/User/main.c
        ${CMAKE_SOURCE_DIR}/User/bsp/src/bsp.c
        ${CMAKE_SOURCE_DIR}/User/bsp/src/bsp_RTT_viewer.c
        ${CMAKE_SOURCE_DIR}/User/app/src/threadx_kernel.c
        ${CMAKE_SOURCE_DIR}/User/app/src/threadx_printf.c
        ${CMAKE_SOURCE_DIR}/User/app/src/threadx_task_manager.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${USER_INCLUDE_DIRS})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${USER_SOURCES})

set(HEX_FILE ${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex)
set(BIN_FILE ${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Building ${HEX_FILE}"
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${HEX_FILE}
        COMMAND ${CMAKE_COMMAND} -E echo "Building ${BIN_FILE}"
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${BIN_FILE}
)
