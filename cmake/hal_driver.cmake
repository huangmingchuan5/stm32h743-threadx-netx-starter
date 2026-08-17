set(HAL_DRIVER_DEFINITIONS
        USE_FULL_LL_DRIVER
        USE_HAL_DRIVER
        STM32H723xx
        $<$<CONFIG:Debug>:DEBUG>
)

set(HAL_DRIVER_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc
        ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
        ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32H7xx/Include
        ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
        ${CMAKE_SOURCE_DIR}/User/core
)

file(GLOB HAL_DRIVER_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/Drivers/STM32H7xx_HAL_Driver/Src/*.c
)
list(FILTER HAL_DRIVER_SOURCES EXCLUDE REGEX "_template\\.c$")

set(SYSTEM_SOURCES
        ${CMAKE_SOURCE_DIR}/cmake/startup_stm32h723xx.s
        ${CMAKE_SOURCE_DIR}/User/core/system_stm32h7xx.c
        ${CMAKE_SOURCE_DIR}/User/core/stm32h7xx_hal_msp.c
        ${CMAKE_SOURCE_DIR}/User/core/stm32h7xx_hal_timebase_tim.c
        ${CMAKE_SOURCE_DIR}/User/core/stm32h7xx_it.c
        ${CMAKE_SOURCE_DIR}/User/core/syscalls.c
        ${CMAKE_SOURCE_DIR}/User/core/sysmem.c
)

add_library(hal_config INTERFACE)
target_compile_definitions(hal_config INTERFACE ${HAL_DRIVER_DEFINITIONS})
target_include_directories(hal_config INTERFACE ${HAL_DRIVER_INCLUDE_DIRS})

add_library(hal_drivers OBJECT)
target_sources(hal_drivers PRIVATE ${HAL_DRIVER_SOURCES} ${SYSTEM_SOURCES})
target_link_libraries(hal_drivers PUBLIC hal_config)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE hal_drivers ${TOOLCHAIN_LINK_LIBRARIES})

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map
)
