set(BOOT_FLASH_ORIGIN 0x08000000)
set(BOOT_FLASH_LENGTH 0x00020000)
set(APP_FLASH_ORIGIN 0x08020000)
set(APP_FLASH_LENGTH 0x001E0000)
math(EXPR APP_VECTOR_OFFSET "${APP_FLASH_ORIGIN} - ${BOOT_FLASH_ORIGIN}" OUTPUT_FORMAT HEXADECIMAL)

set(USER_COMMON_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/User
        ${CMAKE_SOURCE_DIR}/User/core
        ${CMAKE_SOURCE_DIR}/User/bsp/inc
)

set(USER_COMMON_SOURCES
        ${CMAKE_SOURCE_DIR}/User/bsp/src/bsp.c
        ${CMAKE_SOURCE_DIR}/User/bsp/src/bsp_RTT_viewer.c
)

set(USER_COMMON_LIBRARIES
        segger_rtt
)

function(configure_user_common target)
    target_include_directories(${target} PRIVATE
            ${USER_COMMON_INCLUDE_DIRS}
    )
    target_sources(${target} PRIVATE
            ${USER_COMMON_SOURCES}
    )
    target_link_libraries(${target} PRIVATE
            ${USER_COMMON_LIBRARIES}
    )
endfunction()

include(${CMAKE_CURRENT_LIST_DIR}/user_boot.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/user_app.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/firmware_container.cmake)
