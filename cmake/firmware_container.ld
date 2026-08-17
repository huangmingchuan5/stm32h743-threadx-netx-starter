OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(Reset_Handler)

__boot_image_origin__ = DEFINED(__boot_image_origin__) ? __boot_image_origin__ : 0x08000000;
__app_image_origin__ = DEFINED(__app_image_origin__) ? __app_image_origin__ : 0x08020000;

SECTIONS
{
    .boot_image __boot_image_origin__ :
    {
        _firmware_start = .;
        KEEP(*(.boot_image))
    }

    .app_image __app_image_origin__ :
    {
        KEEP(*(.app_image))
    }
}
