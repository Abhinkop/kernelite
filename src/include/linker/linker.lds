#include "linker/linker_defines.h"

/* QEMU virt loads raw Image at RAM_BASE + 0x80000 = 0x40080000 */
#define KERNEL_LOAD_ADDR 0x40080000

ENTRY(primary_entry)

PHDRS
{
    text_perm PT_LOAD FLAGS(5); /* Read (4) + Execute (1) = 5 */
    rodata_perm PT_LOAD FLAGS(4); /* Read (4) = 6 */
    data_perm PT_LOAD FLAGS(6); /* Read (4) + Write (2) = 6 */
}

SECTIONS
{
    . = KERNEL_LOAD_ADDR;

    /* Image start symbol: address of the first byte of the linked image */
    image_start = .;

	.header . : { KEEP(*(.header.arm64)) } :text_perm
	.text : { *(.text) } :text_perm
    .rodata : ALIGN(8) { *(.rodata) } :rodata_perm
    .data   : ALIGN(8) { *(.data) } :data_perm
    .bss : ALIGN(16) {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    } :data_perm
    __bss_size = (__bss_end - __bss_start) >> 3;

    /* Linux pattern: Group early structures and enforce strict page alignment */
    .data.page_tables : ALIGN(LINKER_PAGE_SIZE) {
        idmap_pg_dir_start = .;
        . += ID_MAP_SIZE;

        kernel_pg_dir_root_start = .;
        . += LINKER_PAGE_SIZE;

        page_allocator_bit_map_start = .;
        . += LINKER_BITMAP_SIZE; /* 64 KiB for the page bitmap */
    } :data_perm

    /* Define a small stack area (16KB) */
    .stack (NOLOAD) : ALIGN(16) {
        . += 0x4000;
        stack_top = .;
    } :data_perm

    _end = .;

    /* Image end symbol: address one past the last byte of the linked image */
    image_end = .;
}
