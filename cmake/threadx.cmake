# =============================================================================
#  Azure RTOS / ThreadX 6.x  —— 内核 + Cortex-M7 GNU port
#
#  设计要点（来自 Cmake_threadX_Template，已验证可跑通）：
#    ThreadX 的 tx_thread_schedule.S 定义强符号 PendSV_Handler，
#    tx_initialize_low_level.S 定义强符号 SysTick_Handler，
#    用来覆盖 CMSIS 启动文件里的 .weak 空实现，从而拿到调度心跳。
#    因此必须用 OBJECT 库（目标文件无条件参与链接），而不是静态库 .a
#    —— 静态库会被链接器按需抽取，弱符号已满足引用时就不会去取 PendSV 的
#    目标文件，RTOS 会失去调度。
#
#  源码布局与 eclipse-threadx 仓库一致：
#    common/src/*.c            185 个，与编译器无关，全量编译
#    ports/cortex_m7/gnu/src/*.S   7 个（上下文保存/恢复、PendSV、SysTick 等）
#    另有 User/threadx/tx_initialize_low_level.S（官方 GNU 版底层初始化：
#    设置 VTOR、SysTick(1kHz)、异常优先级、给出 first_unused_memory）
# =============================================================================

set(TX_ROOT "${CMAKE_SOURCE_DIR}/Middlewares/threadx")
set(TX_PORT "${TX_ROOT}/ports/cortex_m7/gnu")

file(GLOB TX_COMMON_SOURCES CONFIGURE_DEPENDS "${TX_ROOT}/common/src/*.c")

set(TX_PORT_SOURCES
    ${TX_PORT}/src/tx_thread_context_restore.S
    ${TX_PORT}/src/tx_thread_context_save.S
    ${TX_PORT}/src/tx_thread_interrupt_control.S
    ${TX_PORT}/src/tx_thread_schedule.S
    ${TX_PORT}/src/tx_thread_stack_build.S
    ${TX_PORT}/src/tx_thread_system_return.S
    ${TX_PORT}/src/tx_timer_interrupt.S
    # 覆盖启动文件弱符号的底层初始化（SysTick / PendSV 强符号在这里）
    ${CMAKE_SOURCE_DIR}/User/threadx/tx_initialize_low_level.S
)

list(LENGTH TX_COMMON_SOURCES _tx_c_count)
message(STATUS "ThreadX     : common/src ${_tx_c_count} 个 C + gnu port ${TX_PORT_SOURCES} 个汇编")

add_library(threadx OBJECT ${TX_COMMON_SOURCES} ${TX_PORT_SOURCES})
target_link_libraries(threadx PRIVATE netx_app_config)
