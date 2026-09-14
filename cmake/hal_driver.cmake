set(HAL_DRIVER_DEFINITIONS
        USE_FULL_LL_DRIVER
        USE_HAL_DRIVER
        STM32H743xx
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

set(HAL_SYSTEM_SOURCES
        ${CMAKE_SOURCE_DIR}/User/core/stm32h7xx_hal_msp.c
        ${CMAKE_SOURCE_DIR}/User/core/stm32h7xx_hal_timebase_tim.c
        ${CMAKE_SOURCE_DIR}/User/core/stm32h7xx_it.c
        ${CMAKE_SOURCE_DIR}/User/core/syscalls.c
        ${CMAKE_SOURCE_DIR}/User/core/sysmem.c
)

add_library(hal_config INTERFACE)
target_compile_definitions(hal_config INTERFACE ${HAL_DRIVER_DEFINITIONS})
target_include_directories(hal_config INTERFACE ${HAL_DRIVER_INCLUDE_DIRS})

add_library(hal_drivers STATIC)
target_sources(hal_drivers PRIVATE ${HAL_DRIVER_SOURCES})
target_link_libraries(hal_drivers PUBLIC hal_config)

# Configure the STM32H743 platform sources and memory layout for a firmware target.
function(configure_hal_firmware target flash_origin flash_length vector_offset)
    target_sources(${target} PRIVATE
            ${CMAKE_SOURCE_DIR}/cmake/startup_stm32h743xx.s
            ${CMAKE_SOURCE_DIR}/User/core/system_stm32h7xx.c
            ${HAL_SYSTEM_SOURCES}
    )

    target_compile_definitions(${target} PRIVATE
            USER_VECT_TAB_ADDRESS
            VECT_TAB_OFFSET=${vector_offset}U
    )

    target_link_libraries(${target} PRIVATE
            hal_drivers
            hal_config
            ${TOOLCHAIN_LINK_LIBRARIES}
    )

    target_link_options(${target} PRIVATE
            ${TARGET_OPTIONS}
            -T${CMAKE_SOURCE_DIR}/cmake/STM32H743xI_flash.ld
            --specs=nano.specs
            -Wl,--defsym=__flash_region_origin__=${flash_origin}
            -Wl,--defsym=__flash_region_length__=${flash_length}
            -Wl,-Map=${CMAKE_BINARY_DIR}/${target}.map
            -Wl,--gc-sections
            -Wl,--print-memory-usage
    )

    set_target_properties(${target} PROPERTIES
            ADDITIONAL_CLEAN_FILES ${CMAKE_BINARY_DIR}/${target}.map
    )
endfunction()
