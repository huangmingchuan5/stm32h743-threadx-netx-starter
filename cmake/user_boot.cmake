set(BOOT_SOURCES
        ${CMAKE_SOURCE_DIR}/User/boot/src/boot_main.c
)

configure_user_common(boot)
target_sources(boot PRIVATE ${BOOT_SOURCES})
target_compile_definitions(boot PRIVATE
        APP_FLASH_START_ADDRESS=${APP_FLASH_ORIGIN}UL
        APP_FLASH_LENGTH=${APP_FLASH_LENGTH}UL
)
configure_hal_firmware(boot ${BOOT_FLASH_ORIGIN} ${BOOT_FLASH_LENGTH} 0x00000000)
