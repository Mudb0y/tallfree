# OpenEVV for the SP-404MKII's Cortex-M7.
#
# The engine library comes from OpenEVV's own Makefile with PORT=none, so its
# source list cannot drift from the desktop build's; this directory adds the
# platform layer and the way in. Run inside nix develop. OpenEVV is the
# submodule in openevv/, pinned to the commit this was built and tested
# against: git submodule update --init fetches it.
#
#   make qemu      build the QEMU image
#   make check     run it and compare with the desktop engine, byte for byte
#   make sim       run the instrument's resident service under QEMU against
#                  a scenario, sim/basic unless SIM= names another directory
#                  holding sim_say.txt and sim_draws.txt
#   make sims      every scenario in sim/, one line each: PASS or FAIL

OPENEVV ?= $(CURDIR)/openevv
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

# Compiled rules: three times faster than bytecode on the unit's core, the
# same samples, 0.9 MB bigger.
LIB := $(OPENEVV)/build/m7c/libevv.a
COMMON := port_coop.c sys.c start.c say.c say.h

TEXT ?= Hello, I am the SP four oh four, and I can talk now.

.PHONY: qemu device check sim sims lib clean
qemu: $(BUILD)/qemu.elf
device: $(BUILD)/TALLFREE.BIN

lib:
	$(MAKE) -C $(OPENEVV) PORT=none BUILD=build/m7c RULES=c \
	  CC=$(CC) AR=arm-none-eabi-ar CFLAGS="$(LIBFLAGS)" build/m7c/libevv.a

$(LIB): lib

$(BUILD)/qemu.elf: $(COMMON) qemu.c qemuboot.c qemu.ld engine.ld $(LIB)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DSEMIHOST $(LDFLAGS) -Wl,-Map=$(BUILD)/qemu.map \
	  -T qemu.ld $(filter %.c,$(COMMON)) qemu.c qemuboot.c $(LIB) -lm -lc -lgcc -o $@

$(BUILD)/sim.elf: $(COMMON) service.c screen.c menu.c pagestubs.h sim.c qemuboot.c device.h qemu.ld engine.ld $(LIB)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DSEMIHOST -DSIM $(LDFLAGS) -Wl,-Map=$(BUILD)/sim.map \
	  -T qemu.ld $(filter %.c,$(COMMON)) service.c screen.c menu.c sim.c qemuboot.c $(LIB) -lm -lc -lgcc -o $@

$(BUILD)/device.elf: $(COMMON) device.c device.h service.c screen.c menu.c pagestubs.h diag.c kernel.c kernel.h device.ld engine.ld $(LIB)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks $(LDFLAGS) -Wl,-Map=$(BUILD)/device.map \
	  -T device.ld $(filter %.c,$(COMMON)) device.c service.c screen.c menu.c diag.c kernel.c $(LIB) -lm -lc -lgcc -o $@

$(BUILD)/TALLFREE.BIN: $(BUILD)/device.elf mkimage.py
	python3 mkimage.py $< $@

# The run leaves out.raw in this directory; the desktop evv built from the
# same tree speaks the same text, and the two are compared as samples.
check: $(BUILD)/qemu.elf
	rm -f out.raw
	qemu-system-arm -M mps2-an500 -nographic -monitor none -serial none \
	  -semihosting-config enable=on,target=native -kernel $(BUILD)/qemu.elf
	./compare.sh out.raw "$(TEXT)"

SIM ?= sim/basic
sim: $(BUILD)/sim.elf
	cd $(SIM) && rm -f sim.raw sim_*.TXT sim_*.CFG && qemu-system-arm -M mps2-an500 -nographic -monitor none \
	  -serial none -semihosting-config enable=on,target=native -kernel $(CURDIR)/$(BUILD)/sim.elf

sims: $(BUILD)/sim.elf
	@for d in sim/*/; do \
	  printf '%-8s ' $$(basename $$d); \
	  $(MAKE) -s sim SIM=$$d 2>&1 | grep -E '^(PASS|FAIL)' || echo 'no expectations'; \
	done

clean:
	rm -rf $(BUILD) out.raw
