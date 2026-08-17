set(NXD_ENABLE_FILE_SERVERS OFF CACHE BOOL "Build NetX Duo file-server protocols" FORCE)

add_subdirectory(
        ${CMAKE_SOURCE_DIR}/Middlewares/netxduo
        ${CMAKE_BINARY_DIR}/netxduo_build
        EXCLUDE_FROM_ALL
)

target_link_libraries(app PRIVATE azrtos::netxduo)
