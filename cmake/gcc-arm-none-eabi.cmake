# =============================================================================
#  ARM GCC 工具链文件 —— STM32H743XIHx (Cortex-M7, 双精度 FPU)
#
#  用法：cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake ..
#  或直接使用 CMakePresets.json（推荐）：cmake --preset Debug
#
#  本文件只放「与具体工程无关」的编译器/链接器公共设置；
#  源码清单、宏、头文件路径全部在顶层 CMakeLists.txt / cmake/*.cmake 中。
# =============================================================================

set(CMAKE_SYSTEM_NAME          Generic)
set(CMAKE_SYSTEM_PROCESSOR     arm)

set(TOOLCHAIN_PREFIX           arm-none-eabi-)
set(CMAKE_C_COMPILER           ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER         ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER         ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_OBJCOPY              ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE                 ${TOOLCHAIN_PREFIX}size)

# 交叉编译不做「能否运行可执行文件」探测
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 让最终产物带 .elf 后缀，与 .hex / .bin / .map 以及 VS Code 调试配置保持一致
set(CMAKE_EXECUTABLE_SUFFIX       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_ASM   ".elf")

# -----------------------------------------------------------------------------
# CPU / FPU
#   Keil: CPUTYPE("Cortex-M7") FPU3(DFPU) / --cpu Cortex-M7.fp.dp
#   SEGGER ES: arm_fp_abi="Hard", arm_fpu_type="FPv5-D16"
# -----------------------------------------------------------------------------
set(TARGET_FLAGS "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} ${TARGET_FLAGS} -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -fdata-sections -ffunction-sections")

# ThreadX GNU port 的 .S 里有 #ifdef / #define，必须经过 C 预处理器
set(CMAKE_ASM_FLAGS
    "${TARGET_FLAGS} -x assembler-with-cpp -MMD -MP")

set(CMAKE_C_FLAGS_DEBUG       "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE     "-Os -g0")
set(CMAKE_ASM_FLAGS_DEBUG     "-O0 -g3")
set(CMAKE_ASM_FLAGS_RELEASE   "-Os -g0")

# -----------------------------------------------------------------------------
# 链接
#   CMAKE_SOURCE_DIR 在工具链文件被读取时已可用；CMAKE_PROJECT_NAME 尚不可用，
#   因此 -Map 放到顶层 CMakeLists.txt 里用 target_link_options 追加。
# -----------------------------------------------------------------------------
set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T \"${CMAKE_SOURCE_DIR}/STM32H743XX_FLASH.ld\"")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-warn-rwx-segments")

# main.c 用 printf("%.11f") 打印 double，newlib-nano 默认裁掉了浮点 printf
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u _printf_float -lm")

# 避免 CMake 把 ob（ARM 遗留库）塞进 C++ 隐式链接库
set(CMAKE_C_IMPLICIT_LINK_LIBRARIES "")
