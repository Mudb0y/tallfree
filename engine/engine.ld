/* The engine image: one block at ENGINE_BASE, ENGINE_SIZE long, defined by
   whichever script includes this. Code, constants and initialised data come
   first and are what the loader copies; zeroes, the heap and the engine's
   own stack follow and are only address space. */

ENTRY(engine_start)

STACK_SIZE = 64K;

SECTIONS
{
  . = ENGINE_BASE;
  __image_start = .;

  /* What the loader checks before it jumps: magic "EVV1", the image's
     length, the entry point, and a CRC that mkimage.py fills in. */
  .header : {
    /* "EVV2": linked at image 30's 0x83AC0000. Image 29 engines, "EVV1",
       were linked 1 MB higher and must never be run here. */
    LONG(0x32565645)
    LONG(__image_end - __image_start)
    LONG(engine_start | 1)
    LONG(0)
  }

  .text : {
    KEEP(*(.entry))
    *(.text .text.*)
    *(.rodata .rodata.*)
    . = ALIGN(8);
  }

  .ARM.exidx : {
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;
  }

  .init_array : {
    . = ALIGN(4);
    __init_array_start = .;
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array))
    __init_array_end = .;
  }

  .data : {
    . = ALIGN(8);
    *(.data .data.*)
    . = ALIGN(8);
  }

  .tdata : {
    __tdata_start = .;
    *(.tdata .tdata.*)
    __tdata_end = .;
  }

  .tbss : { *(.tbss .tbss.*) }
  __tbss_size = SIZEOF(.tbss);
  __tls_align = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss));

  __image_end = .;

  .bss (NOLOAD) : {
    . = ALIGN(8);
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(8);
    __bss_end = .;
  }

  __heap_start = ALIGN(8);
  __stack_top = ENGINE_BASE + ENGINE_SIZE;
  __heap_end = __stack_top - STACK_SIZE;

  ASSERT(__heap_end > __heap_start + 1024K, "engine image leaves under a megabyte of heap")

  /DISCARD/ : { *(.ARM.attributes) *(.comment) }
}
