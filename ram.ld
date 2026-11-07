MEMORY {
    iram (rx) : ORIGIN = 0x40378000, LENGTH = 0x18000
    dram (rw) : ORIGIN = 0x3fca0000, LENGTH = 0x20000
}
ENTRY(game_main)
SECTIONS {
    .text : ALIGN(4) {
        *(.literal .literal.*)
        *(.text .text.*)
        . = ALIGN(4);
    } > iram
    .data : ALIGN(4) {
        *(.data .data.* .rodata .rodata.* .sdata .sdata.*)
        . = ALIGN(4);
    } > dram
    .bss (NOLOAD) : ALIGN(4) {
        _bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > dram
    /DISCARD/ : { *(.comment .note.* .eh_frame) }
}
INCLUDE rom.ld
ASSERT(SIZEOF(.text) < 0x18000, "Game exceeds instruction RAM")
ASSERT(SIZEOF(.data) + SIZEOF(.bss) < 0x20000, "Game exceeds data RAM")
