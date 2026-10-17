# dualshock/Makefile - rebuild SLUS_007.47 + the 12 PROG2 overlays byte-for-byte.
#
# Run through the build environment wrapper (from dualshock/; on Windows it enters the project's MSYS2):
#   sh ../tools/buildenv.sh "make split"   # (re)generate asm/ + linker scripts
#   sh ../tools/buildenv.sh "make -j8"     # build everything + SHA1 checks
#   sh ../tools/buildenv.sh "make clean"
#
# Originals are read from ../extracted (sh ../tools/dump_discs.sh).

TOOLS    := ../tools/external
# The project venv: Scripts/*.exe on Windows, bin/ elsewhere. Decided by what exists, not by $(OS):
# environment variables don't reach make inside MSYS2 (see tools/buildenv.sh).
ifneq ($(wildcard $(TOOLS)/venv/Scripts/python.exe),)
VENVBIN  := $(TOOLS)/venv/Scripts
EXE      := .exe
else
VENVBIN  := $(TOOLS)/venv/bin
EXE      :=
endif
SPLAT    := $(VENVBIN)/splat$(EXE)
CROSS    := mips64-elf-
AS       := $(CROSS)as
LD       := $(CROSS)ld
OBJCOPY  := $(CROSS)objcopy

BUILD    := build
DISC     := ../extracted/RE1-Dualshock
OVERLAYS := logo prologue title select ending stage1 stage2 stage3 stage4 stage5 stage6 stage7

ASFLAGS  := -EL -march=r3000 -mtune=r3000 -mabi=32 -no-pad-sections -O1 -G0 -Iinclude
LDFLAGS  := -EL -m elf32ebmip -nostdlib --no-check-sections

# C: cpp -> cc1 (GCC 2.6.3-psx, built by tools/build_old_gcc.sh 2.6.3) -> maspsx -> as.
# Capcom's code was compiled with GCC 2.6.3 (see docs/matching_guide.md). It doesn't use $gp at all,
# hence -G0.
OLDGCC   := $(TOOLS)/old-gcc-build/gcc-2.6.3-psx
CPP      := $(OLDGCC)/cpp
CC1      := $(OLDGCC)/cc1
PYTHON   := $(VENVBIN)/python$(EXE)
MASPSX   := $(PYTHON) $(TOOLS)/maspsx/maspsx.py
CPPFLAGS := -undef -nostdinc -Iinclude -D__GNUC__=2 -Dmips -D__mips__ -D_LANGUAGE_C -DLANGUAGE_C
CFLAGS   := -quiet -G0 -mcpu=3000 -mips1 -fno-builtin -msoft-float
# Optimization per target: main executable -O2; overlays -O0 (every overlay function keeps $fp).
opt_for   = $(if $(filter main/%,$(1)),-O2,-O0)
# -O0 targets use the struct form of m2c's M2C_FIELD (include/m2c_fields.h) - only for decompiler drafts
# (tools/autodecomp.py, refine.py); src/ doesn't include m2c_fields.h (its two forms are OFFSET_MEMBER /
# OFFSET_DEREF in include/re1_types.h).
cppdefs_for = $(if $(filter main/%,$(1)),,-DM2C_FIELD_STRUCT)
MASPSXFLAGS := --aspsx-version=2.56 --expand-div

upper = $(shell echo $(1) | tr a-z A-Z)

.PHONY: all check split clean

all: check

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

# Intermediates (.i, .s, .ms.s) are kept next to the object for inspecting non-matches.
# maspsx reads stdin (it hangs if given a filename from a non-terminal).
# Every C object depends on all headers (no per-file dependency tracking with this old cpp).
# The sed step (overlays only): our GCC aligns switch jump tables to 8 (`.align 3`) within the
# object. Capcom built each overlay from several source files whose .rodata parts were linked
# 4-aligned, so in our single overlay C file the tables are 4-aligned (plus an explicit zero word
# where Capcom's file padded). The main program's tables keep `.align 3`, except after a
# `__asm__("#JTALIGN2");` marker (tools/jtalign.awk, run on cc1's output before maspsx, which drops
# comments: Capcom's main was built from several files too, some of its tables are 4-aligned).
alignfix_for = $(if $(filter main/%,$(1)),cat,sed 's/^\.align\t3$$/.align\t2/')
premaspsx_for = $(if $(filter main/%,$(1)),awk -f tools/jtalign.awk,cat)
# (also rebuilt when a program's globals.h or a shared enemy source changes: tools/splitc.py, sharetwins.py)
$(BUILD)/src/%.o: src/%.c $(wildcard include/*.h) $(wildcard src/*/globals.h) $(wildcard src/shared/*.c)
	@mkdir -p $(dir $@)
	$(CPP) $(CPPFLAGS) $(call cppdefs_for,$*) $< $(BUILD)/src/$*.i
	$(CC1) $(CFLAGS) $(call opt_for,$*) $(BUILD)/src/$*.i -o $(BUILD)/src/$*.s
	$(call premaspsx_for,$*) < $(BUILD)/src/$*.s | $(MASPSX) $(MASPSXFLAGS) | $(call alignfix_for,$*) > $(BUILD)/src/$*.ms.s
	$(AS) $(ASFLAGS) -o $@ $(BUILD)/src/$*.ms.s

# $(call target,<basename>,<asm subdir>,<built exe>,<original exe>)
# asm/<t>/nonmatchings/ holds per-function files pulled in by INCLUDE_ASM from src/<t>/*.c.
define target
$(1)_O := $$(patsubst %.s,$(BUILD)/%.o,$$(shell find asm/$(2) -name '*.s' -not -path '*/nonmatchings/*' 2>/dev/null)) \
          $$(patsubst %.c,$(BUILD)/%.o,$$(shell find src/$(2) -name '*.c' 2>/dev/null))

$(BUILD)/$(1).elf: $$($(1)_O) $(BUILD)/$(1).ld
	$$(LD) $$(LDFLAGS) -T $(BUILD)/$(1).ld \
		-T $(BUILD)/$(1)_undefined_syms_auto.txt -T $(BUILD)/$(1)_undefined_funcs_auto.txt \
		-Map $(BUILD)/$(1).map -o $$@

$(3): $(BUILD)/$(1).elf
	$$(OBJCOPY) -O binary $$< $$@

.PHONY: check-$(1)
check-$(1): $(3)
	@sh tools/check_sha1.sh $(3) $(4)
check: check-$(1)
endef

$(eval $(call target,slus_007.47,main,$(BUILD)/SLUS_007.47,$(DISC)/SLUS_007.47))
$(foreach o,$(OVERLAYS),$(eval $(call target,$(o),$(o),$(BUILD)/$(call upper,$(o)).EXE,$(DISC)/PSXUSA/PROG2/$(call upper,$(o)).EXE)))

check:
	@echo "All 13 executables match the originals."

split:
	$(SPLAT) split slus_007.47.yaml
	$(foreach o,$(OVERLAYS),$(SPLAT) split $(o).yaml &&) true

# compile_commands.json for editor IntelliSense (VS Code: .vscode/c_cpp_properties.json); re-run after
# adding or splitting source files.
.PHONY: compile_commands
compile_commands:
	$(PYTHON) tools/gen_compile_commands.py

# A playable disc image in build/disc/ (Track 1 + Track 2 + .cue): the 13 executables built from the C code
# put into a copy of the original disc (../game_files/RE1-Dualshock). On Windows build_disc.bat does the same.
.PHONY: disc
disc: check
	$(PYTHON) tools/build_disc.py

clean:
	rm -rf $(BUILD)/asm $(BUILD)/src $(BUILD)/*.elf $(BUILD)/*.map $(BUILD)/*.EXE $(BUILD)/SLUS_007.47
