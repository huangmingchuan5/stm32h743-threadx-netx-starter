# =============================================================================
#  CMSIS + STM32H7xx HAL Driver（ST 固件库）
#
#  默认编译 Drivers/STM32H7xx_HAL_Driver/Src 下全部 .c（排除 *_template.c），
#  新手最省心：不会因为漏加某个 HAL 源文件而报 undefined reference。
#  代价是编译稍慢；若想更快，可改成只编译用到的几个文件（参考模板 stm32h7.cmake）。
#
#  注意：system_stm32h7xx.c 不放这里，而是作为可执行目标源码的一部分编译
#  （见顶层 CMakeLists.txt），避免和 Drivers 里自带的 template 版本重复定义。
# =============================================================================

set(HAL_DIR "${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver")

file(GLOB HAL_SOURCES CONFIGURE_DEPENDS "${HAL_DIR}/Src/*.c")
list(FILTER HAL_SOURCES EXCLUDE REGEX "_template\\.c$")

list(LENGTH HAL_SOURCES _hal_count)
message(STATUS "HAL         : ${_hal_count} 个源文件")

add_library(stm32h7 OBJECT ${HAL_SOURCES})
target_link_libraries(stm32h7 PRIVATE netx_app_config)
