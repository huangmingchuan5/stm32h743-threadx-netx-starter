set(THREADX_ARCH cortex_m7)
set(THREADX_TOOLCHAIN gnu)
set(TX_USER_FILE ${CMAKE_SOURCE_DIR}/User/threadx/tx_user.h)

add_subdirectory(
        ${CMAKE_SOURCE_DIR}/Middlewares/threadx
        ${CMAKE_BINARY_DIR}/threadx_build
        EXCLUDE_FROM_ALL
)

target_sources(threadx PRIVATE
        ${CMAKE_SOURCE_DIR}/User/threadx/tx_initialize_low_level.S
)

target_link_libraries(app PRIVATE azrtos::threadx)
