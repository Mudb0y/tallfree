/* The instrument: the top of SDRAM that the looper and skip-back buffer gave
   up in the patched firmware, from 0x83BC0000 to the firmware's own data at
   0x83FF0000. make_patch27.py moves the buffer's end to 0x83BBA424. */

ENGINE_BASE = 0x83BC0000;
ENGINE_SIZE = 0x00430000;

INCLUDE engine.ld
