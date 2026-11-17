/* Reserve the existing 8 KiB STM32duino bootloader. Use a conservative
 * 56 KiB application region so no flash-backed settings are overwritten. */
MEMORY
{
    rom (rx) : ORIGIN = 0x08002000, LENGTH = 56K
    ram (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}
INCLUDE cortex-m-generic.ld

