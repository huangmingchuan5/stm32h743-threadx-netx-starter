# Generate standalone HEX and BIN files for one firmware image.
function(add_firmware_outputs target)
    set(hex_file ${CMAKE_BINARY_DIR}/${target}.hex)
    set(bin_file ${CMAKE_BINARY_DIR}/${target}.bin)

    add_custom_command(TARGET ${target} POST_BUILD
            BYPRODUCTS ${hex_file} ${bin_file}
            COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${target}> ${hex_file}
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> ${bin_file}
            COMMENT "Generating ${target}.hex and ${target}.bin"
            VERBATIM
    )
endfunction()

add_firmware_outputs(boot)
add_firmware_outputs(app)

set(FIRMWARE_HEX ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex)
set(FIRMWARE_BIN ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin)
set(FIRMWARE_MAP ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map)
set(BOOT_BIN ${CMAKE_BINARY_DIR}/boot.bin)
set(APP_BIN ${CMAKE_BINARY_DIR}/app.bin)
set(BOOT_IMAGE_OBJECT ${CMAKE_BINARY_DIR}/boot_image.o)
set(APP_IMAGE_OBJECT ${CMAKE_BINARY_DIR}/app_image.o)

# Wrap one linked firmware image in a loadable object section.
function(add_firmware_image_object target section_name image_bin image_object)
    add_custom_command(
            OUTPUT ${image_object}
            COMMAND ${CMAKE_OBJCOPY}
                -I binary
                -O elf32-littlearm
                -B arm
                --rename-section .data=${section_name},alloc,load,readonly,data,contents
                ${image_bin}
                ${image_object}
            DEPENDS ${target} ${image_bin}
            COMMENT "Wrapping ${target} firmware image"
            VERBATIM
    )

    set_source_files_properties(${image_object} PROPERTIES
            GENERATED TRUE
            EXTERNAL_OBJECT TRUE
    )
endfunction()

add_firmware_image_object(boot .boot_image ${BOOT_BIN} ${BOOT_IMAGE_OBJECT})
add_firmware_image_object(app .app_image ${APP_BIN} ${APP_IMAGE_OBJECT})

add_executable(${CMAKE_PROJECT_NAME}
        ${CMAKE_CURRENT_LIST_DIR}/firmware_container.S
        ${BOOT_IMAGE_OBJECT}
        ${APP_IMAGE_OBJECT}
)

add_dependencies(${CMAKE_PROJECT_NAME} boot app)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        LINKER_LANGUAGE C
        ADDITIONAL_CLEAN_FILES ${FIRMWARE_MAP}
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
        ${TARGET_OPTIONS}
        -nostdlib
        -Wl,-z,noexecstack
        -Wl,--just-symbols=$<TARGET_FILE:boot>
        -Wl,--defsym=__boot_image_origin__=${BOOT_FLASH_ORIGIN}
        -Wl,--defsym=__app_image_origin__=${APP_FLASH_ORIGIN}
        -T${CMAKE_CURRENT_LIST_DIR}/firmware_container.ld
        -Wl,-Map=${FIRMWARE_MAP}
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        BYPRODUCTS ${FIRMWARE_HEX} ${FIRMWARE_BIN}
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${FIRMWARE_HEX}
        COMMAND ${CMAKE_OBJCOPY} -O binary --gap-fill=0xFF $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${FIRMWARE_BIN}
        COMMENT "Generating combined boot and app firmware outputs"
        VERBATIM
)
