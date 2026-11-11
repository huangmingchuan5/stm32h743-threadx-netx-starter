# =============================================================================
#  Azure RTOS / NetX Duo 6.x  —— TCP/IP 协议栈 + Cortex-M7 GNU port
#
#  与 ThreadX 同样用 OBJECT 库分层编译：
#    common/src/*.c   协议栈全部源码（IP / ARP / TCP / UDP / DHCP ...）
#  用户配置在 User/netxduo/nx_user.h，由 NX_INCLUDE_USER_DEFINE_FILE 引入。
#
#  重要：NetX Duo 的 GNU 端口（ports/cortex_m7/gnu）是「纯头文件」（只有
#  inc/nx_port.h），没有需要单独编译的 .S/.c —— 因为上下文切换、PendSV、
#  SysTick 全部由 ThreadX 的 GNU 端口负责。所以这里只需要编译 common/src，
#  端口头文件路径通过顶层 netx_app_config INTERFACE 库统一包含（见 CMakeLists.txt）。
#
#  NetXDuo 不提供调度强符号，用 OBJECT 库只是和 ThreadX/HAL/BSP 保持一致的
#  分层风格；即便用静态库也不会影响内核调度。
# =============================================================================

set(NX_ROOT "${CMAKE_SOURCE_DIR}/Middlewares/netxduo")

file(GLOB NX_COMMON_SOURCES CONFIGURE_DEPENDS "${NX_ROOT}/common/src/*.c")

# DHCP 客户端属于 NetX Duo 的「附加组件」（addons/），不在 common/src 里。
# 本工程用到的 DHCP 客户端实现就在这个文件；其余 addon（server / dhcpv6 等）
# 按需再往这里加即可。
set(NX_ADDON_SOURCES
    ${NX_ROOT}/addons/dhcp/nxd_dhcp_client.c
)

list(LENGTH NX_COMMON_SOURCES _nx_c_count)
list(LENGTH NX_ADDON_SOURCES  _nx_addon_count)
message(STATUS "NetXDuo     : common/src ${_nx_c_count} 个 C + addon ${_nx_addon_count} 个 C（GNU 端口为纯头文件，无单独编译单元）")

add_library(netxduo OBJECT ${NX_COMMON_SOURCES} ${NX_ADDON_SOURCES})
target_link_libraries(netxduo PRIVATE netx_app_config)
