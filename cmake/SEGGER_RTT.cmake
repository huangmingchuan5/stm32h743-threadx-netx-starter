# =============================================================================
#  SEGGER RTT —— 板子 ↔ 上位机 的「虚拟串口」调试输出通道
#
#  本工程的 printf / threadx_printf 全部走 RTT（见 User/app/src/threadx_printf.c）。
#  RTT 不占用真实 UART，通过 SWD（ST-Link / DAPLink）把数据传到
#  J-Link RTT Viewer / telnet 127.0.0.1:19021 查看。
#  这也是「串口权限留给你」里确定的调试输出方案：免接线、免驱动、零额外外设。
# =============================================================================

set(SEGGER_RTT_SOURCES
    ${CMAKE_SOURCE_DIR}/Middlewares/SEGGER_RTT/RTT/SEGGER_RTT.c
    ${CMAKE_SOURCE_DIR}/Middlewares/SEGGER_RTT/RTT/SEGGER_RTT_printf.c
)

add_library(segger_rtt OBJECT ${SEGGER_RTT_SOURCES})
target_link_libraries(segger_rtt PRIVATE netx_app_config)
