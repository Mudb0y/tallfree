/* QEMU's mps2-an500, a Cortex-M7 board: the image in its 16 MB of RAM at
   0x60000000, and a vector table at zero so the core can boot into it. */

ENGINE_BASE = 0x60000000;
ENGINE_SIZE = 16M;

SECTIONS
{
  .qemu_vectors 0x0 : { KEEP(*(.qemu_vectors)) }
}

INCLUDE engine.ld
