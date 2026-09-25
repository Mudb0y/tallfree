ENTRY(boot_hook)
SECTIONS
{
  . = 0x60308000;
  .payload : { *(.text.boot_hook) *(.text*) *(.rodata*) }

  /* The boot loader keeps nothing outside its stack and the engine's region.
     Anything the compiler still puts here is a mistake, since flash is not
     writable. */
  ASSERT(SIZEOF(.payload) > 0, "empty boot loader")
  /DISCARD/ : { *(.data*) *(.bss*) *(COMMON) *(.ARM.attributes) *(.comment) }
}
