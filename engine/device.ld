/* The instrument: the top of SDRAM that the looper and skip-back buffer gave
   up in the patched firmware, from 0x83AC0000 to the firmware's own data at
   0x83FF0000. make_patch30.py moves the buffer's end to 0x83ABA424, which
   leaves it 2.5 MB; the compiled rules need the room. */

ENGINE_BASE = 0x83AC0000;
ENGINE_SIZE = 0x00530000;

INCLUDE engine.ld
