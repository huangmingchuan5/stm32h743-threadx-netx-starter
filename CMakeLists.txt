# =============================================================================
#  STM32H743-ThreadX-NetXDuo_Demo
#  STM32H743XIHx (安富莱 V7) + ThreadX + NetX Duo 单镜像例程
#
#  改编自 Cmake_threadX_Template，面向初学者：
#   1. 单镜像（去掉原工程的 boot/app 双镜像固件容器，降低认知负担）。
#   2. 各组件用 CMake OBJECT 库分层：ThreadX / NetXDuo / HAL / BSP / SEGGER_RTT。
#      ThreadX 的 PendSV_Handler / SysTick_Handler 是「强符号覆盖启动文件弱符号」，
#      OBJECT 库的目标文件无条件参与链接，天然避开「调度心跳丢失」这个坑。
#   3. 工程名 = 所在目录名，复制整个文件夹重命名即可生成新工程。
#   4. 调试输出走 SEGGER RTT（虚拟串口），免接线即可看 printf。
#
#  构建：  cmake --preset Debug  &&  cmake --build build
#          （或直接 ninja -C build）
# =============================================================================
cmake_minimum_required(VERSION 3.22)

# 注意：toolchain 文件是在 project() 时才被 CMake 读取的，所以此处只能检查变量本身
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR "${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
    message(FATAL_ERROR
        "未检测到交叉编译工具链。请用以下任一方式配置：\n"
        "  cmake --preset Debug\n"
        "  cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake -B build\n")
endif()

get_filename_component(_AUTO_PROJECT_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
set(CMAKE_PROJECT_NAME "${_AUTO_PROJECT_NAME}")
project(${CMAKE_PROJECT_NAME} LANGUAGES C ASM)

set(CMAKE_C_STANDARD           11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS         ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Project      : ${CMAKE_PROJECT_NAME}")
message(STATUS "Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "Toolchain    : ${CMAKE_C_COMPILER}")

# 避免 CMake 把 ARM 遗留库 ob 塞进隐式链接
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# -----------------------------------------------------------------------------
# 全局编译配置：一个 INTERFACE 库统一分发给所有组件，避免宏/头文件路径各写一份
# -----------------------------------------------------------------------------
add_library(netx_app_config INTERFACE)

target_compile_definitions(netx_app_config INTERFACE
    USE_HAL_DRIVER                     # 使用 STM32 HAL 库
    STM32H743xx                        # 器件型号
    USE_FULL_LL_DRIVER                 # 使用 LL 驱动
    TX_ENABLE_FPU_SUPPORT              # ThreadX 上下文切换保存 S16-S31（C 与 ASM 都要）
    # 注意：TX_ENABLE_STACK_CHECKING 已在 User/threadx/tx_user.h 中开启，这里不再重复定义，
    # 否则每个文件都会刷一条 “redefined” 警告。tx_user.h 由 TX_INCLUDE_USER_DEFINE_FILE 引入。
    TX_INCLUDE_USER_DEFINE_FILE        # 使 tx_port.h 包含 User/threadx/tx_user.h
    NX_INCLUDE_USER_DEFINE_FILE        # 使 nx_port.h 包含 User/netxduo/nx_user.h
    # 用户配置文件绝对路径（预处理展开为 #include "..."）
    "TX_USER_FILE=\"${CMAKE_SOURCE_DIR}/User/threadx/tx_user.h\""
    "NX_USER_FILE=\"${CMAKE_SOURCE_DIR}/User/netxduo/nx_user.h\""
)

target_include_directories(netx_app_config INTERFACE
    ${CMAKE_SOURCE_DIR}/User
    ${CMAKE_SOURCE_DIR}/User/bsp
    ${CMAKE_SOURCE_DIR}/User/bsp/inc
    ${CMAKE_SOURCE_DIR}/User/app/inc
    ${CMAKE_SOURCE_DIR}/User/core
    ${CMAKE_SOURCE_DIR}/User/netxduo
    ${CMAKE_SOURCE_DIR}/User/netxduo/lan8720
    ${CMAKE_SOURCE_DIR}/User/dev/lan8742
    ${CMAKE_SOURCE_DIR}/User/threadx
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32H7xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Middlewares/threadx/common/inc
    ${CMAKE_SOURCE_DIR}/Middlewares/threadx/ports/cortex_m7/gnu/inc
    ${CMAKE_SOURCE_DIR}/Middlewares/netxduo/common/inc
    ${CMAKE_SOURCE_DIR}/Middlewares/netxduo/ports/cortex_m7/gnu/inc
    # NetX Duo 的「附加组件 / TSN」不在 common/inc 里，需要单独加路径：
    ${CMAKE_SOURCE_DIR}/Middlewares/netxduo/addons/dhcp   # nxd_dhcp_client.h（DHCP 客户端）
    ${CMAKE_SOURCE_DIR}/Middlewares/netxduo/tsn/inc        # nx_shaper.h（驱动头文件间接引用）
    ${CMAKE_SOURCE_DIR}/Middlewares/SEGGER_RTT/RTT
    ${CMAKE_SOURCE_DIR}/User/SEGGER_RTT/config
)

# -----------------------------------------------------------------------------
# 组件
# -----------------------------------------------------------------------------
include(cmake/threadx.cmake)
include(cmake/netxduo.cmake)
include(cmake/stm32h7.cmake)
include(cmake/segger_rtt.cmake)
include(cmake/bsp.cmake)

# -----------------------------------------------------------------------------
# 可执行目标
#   startup 必须直接放在可执行目标里（静态/OBJECT 库中的向量表 .o 有被丢弃风险，
#   且链接脚本依赖 .isr_vector 段位于 FLASH 起始地址）
# -----------------------------------------------------------------------------
file(GLOB APP_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/User/app/src/*.c")

add_executable(${CMAKE_PROJECT_NAME}
    ${CMAKE_SOURCE_DIR}/startup_stm32h743xx.s          # 中断向量表（.isr_vector）
    ${CMAKE_SOURCE_DIR}/User/main.c
    # --- 内核 / 系统层 ---
    ${CMAKE_SOURCE_DIR}/User/core/system_stm32h7xx.c
    ${CMAKE_SOURCE_DIR}/User/core/stm32h7xx_hal_msp.c
    ${CMAKE_SOURCE_DIR}/User/core/stm32h7xx_hal_timebase_tim.c
    ${CMAKE_SOURCE_DIR}/User/core/stm32h7xx_it.c
    ${CMAKE_SOURCE_DIR}/User/core/syscalls.c
    ${CMAKE_SOURCE_DIR}/User/core/sysmem.c
    # --- 业务逻辑层（ThreadX 任务 + NetX 网络）---
    ${APP_SRC}
    ${CMAKE_SOURCE_DIR}/User/bsp/src/bsp_eth.c
    ${CMAKE_SOURCE_DIR}/User/netxduo/nx_stm32_eth_driver.c
    ${CMAKE_SOURCE_DIR}/User/netxduo/lan8720/nx_stm32_phy_driver.c
    ${CMAKE_SOURCE_DIR}/User/dev/lan8742/lan8742.c
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    netx_app_config
    threadx
    netxduo
    stm32h7
    segger_rtt
    bsp
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    "-Wl,-Map=${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map"
)

# -----------------------------------------------------------------------------
# 构建后处理：生成 .hex / .bin 并打印尺寸
# -----------------------------------------------------------------------------
# 注意：POST_BUILD 用 $<TARGET_FILE_NAME>（仅文件名，不含目录）。
# 因为本工程路径含中文/全角括号，若用完整绝对路径传给 objcopy，在 Windows 的
# cmd.exe /C 链路下会因编码问题报 “No such file or directory”。
# 构建时 cwd 即为 build 目录，使用纯文件名即可正确定位 .elf。
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}>
    COMMENT "生成 ${CMAKE_PROJECT_NAME}.hex / .bin 并统计 Flash/RAM 占用"
    VERBATIM
)
