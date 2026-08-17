set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# Some default GCC settings
# arm-none-eabi- must be part of path environment
set(TOOLCHAIN_PREFIX arm-none-eabi-)

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_LINKER ${TOOLCHAIN_PREFIX}ld)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)

set(CMAKE_EXECUTABLE_SUFFIX_ASM ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_OPTIONS
        -mthumb
        -mcpu=cortex-m7
        -mfpu=fpv5-d16
        -mfloat-abi=hard
)
string(JOIN " " TARGET_FLAGS ${TARGET_OPTIONS})
set(COMMON_FLAGS "${TARGET_FLAGS} -Wall -fdata-sections -ffunction-sections -fstack-usage")

set(CMAKE_C_FLAGS_INIT "${COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${TARGET_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_CXX_FLAGS_INIT "${COMMON_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

# The cyclomatic-complexity parameter must be defined for the Cyclomatic complexity feature in STM32CubeIDE to work.
# However, most GCC toolchains do not support this option, which causes a compilation error; for this reason, the feature is disabled by default.
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcyclomatic-complexity")

set(CMAKE_C_FLAGS_DEBUG_INIT "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE_INIT "-Os -g0")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Os -g0")

set(TOOLCHAIN_LINK_LIBRARIES "m")
