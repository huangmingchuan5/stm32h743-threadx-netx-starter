set(NXD_ENABLE_FILE_SERVERS OFF CACHE BOOL "Build NetX Duo file-server protocols" FORCE)
set(NX_USER_FILE ${CMAKE_SOURCE_DIR}/User/netxduo/nx_user.h)

add_subdirectory(
        ${CMAKE_SOURCE_DIR}/Middlewares/netxduo
        ${CMAKE_BINARY_DIR}/netxduo_build
        EXCLUDE_FROM_ALL
)

target_link_libraries(app PRIVATE azrtos::netxduo)
