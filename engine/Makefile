# OpenEVV for the SP-404MKII's Cortex-M7.
#
# The engine library comes from OpenEVV's own Makefile with PORT=none, so its
# source list cannot drift from the desktop build's; this directory adds the
# platform layer and the way in. Run inside
#   nix shell nixpkgs#gcc-arm-embedded nixpkgs#gnumake nixpkgs#python3
#
#   make qemu      build the QEMU image
#   make check     run it and compare with the desktop engine, byte for byte

OPENEVV ?= /home/stas/Projects/openevv-sp404
CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
BUILD   := build

ARCH    := -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16
LIBFLAGS := $(ARCH) -fsigned-char -ffunction-sections -fdata-sections \
            -DEVV_FRAME_STACK=65536
CFLAGS  := $(ARCH) -O2 -std=gnu99 -Wall -Wno-attributes -fsigned-char -ffunction-sections \
           -fdata-sections -I$(OPENEVV)/src/port -I$(OPENEVV)/include
LDFLAGS := $(ARCH) -nostartfiles --specs=nano.specs -Wl,--gc-sections -Wl,--no-warn-rwx-segments \
           -Wl,-Map=$(BUILD)/$*.map

LIB := $(OPENEVV)/build-m7/libevv.a
COMMON := port_coop.c sys.c start.c say.c

TEXT ?= Hello, I am the SP four oh four, and I can talk now.

.PHONY: qemu device check lib clean
qemu: $(BUILD)/qemu.elf
device: $(BUILD)/ENGINE.BIN

lib:
	$(MAKE) -C $(OPENEVV) PORT=none BUILD=build-m7 RULES=bytecode \
	  CC=$(CC) AR=arm-none-eabi-ar CFLAGS="$(LIBFLAGS)" build-m7/libevv.a

$(LIB): lib

$(BUILD)/qemu.elf: $(COMMON) qemu.c qemu.ld engine.ld $(LIB)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DSEMIHOST $(LDFLAGS) -Wl,-Map=$(BUILD)/qemu.map \
	  -T qemu.ld $(COMMON) qemu.c $(LIB) -lm -lc -lgcc -o $@

$(BUILD)/device.elf: $(COMMON) device.c diag.c kernel.c kernel.h device.ld engine.ld $(LIB)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks $(LDFLAGS) -Wl,-Map=$(BUILD)/device.map \
	  -T device.ld $(COMMON) device.c diag.c kernel.c $(LIB) -lm -lc -lgcc -o $@

$(BUILD)/ENGINE.BIN: $(BUILD)/device.elf mkimage.py
	python3 mkimage.py $< $@

# The run leaves out.raw in this directory; the desktop evv built from the
# same tree speaks the same text, and the two are compared as samples.
check: $(BUILD)/qemu.elf
	rm -f out.raw
	qemu-system-arm -M mps2-an500 -nographic -monitor none -serial none \
	  -semihosting-config enable=on,target=native -kernel $(BUILD)/qemu.elf
	./compare.sh out.raw "$(TEXT)"

clean:
	rm -rf $(BUILD) out.raw
