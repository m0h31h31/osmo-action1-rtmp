ENTRY(oa_draw)
SECTIONS {
  . = 0x843b94;
  .text : { *(.text*) }
  . = 0x25caad0;
  .rodata : { *(.rodata*) }
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) }
  ASSERT(SIZEOF(.text) <= 0x1698, "Payload must leave UVC InitZero and Init intact for disabling")
  ASSERT(SIZEOF(.rodata) <= 0x280, "Labels overlap diagnostic ring at 0x25cad50")
  ASSERT(SIZEOF(.data) == 0, "No writable data in code memory")
  ASSERT(SIZEOF(.bss) == 0, "No implicit BSS allowed")
}
