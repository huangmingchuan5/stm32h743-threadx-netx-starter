set(SEGGER_RTT_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/Middlewares/SEGGER_RTT/RTT
        ${CMAKE_SOURCE_DIR}/User/SEGGER_RTT/config
)

set(SEGGER_RTT_SOURCES
        ${CMAKE_SOURCE_DIR}/Middlewares/SEGGER_RTT/RTT/SEGGER_RTT.c
        ${CMAKE_SOURCE_DIR}/Middlewares/SEGGER_RTT/RTT/SEGGER_RTT_printf.c
)

add_library(segger_rtt STATIC)
target_include_directories(segger_rtt PUBLIC ${SEGGER_RTT_INCLUDE_DIRS})
target_sources(segger_rtt PRIVATE ${SEGGER_RTT_SOURCES})

target_link_libraries(app PRIVATE segger_rtt)
