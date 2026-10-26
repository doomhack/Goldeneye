# Makefile to build Goldeneye 007

### Default target ###
default: all

### Default Build Options ###
# Version of the game to build
VERSION := US
# Optimisation level for game code
OPTIMIZATION := -O2

# Include Terminal Codes for colourising text.
include include/make/VT100Codes.make
include include/make/Gui.make

# set toolchain based on current OS
ifeq ($(shell type mips-linux-gnu-ld >/dev/null 2>/dev/null; echo $$?), 0)
  TOOLCHAIN := mips-linux-gnu-
else ifeq ($(shell type mips64-linux-gnu-ld >/dev/null 2>/dev/null; echo $$?), 0)
  TOOLCHAIN := mips64-linux-gnu-
else
  TOOLCHAIN := mips64-elf-
endif

# other tools
TOOLS_DIR := tools
DATASEG_COMP := $(TOOLS_DIR)/data_compress.sh
RZ_COMP := $(TOOLS_DIR)/1172compress.sh
N64CKSUM := $(TOOLS_DIR)/n64cksum

# Convert AI Print commands from readable strings to byte arrays automatically.
ConvertAIPRINT = sed -E -e ':loop s/PRINT\("(..*?)(.)"/PRINT\("\1",\x27\2\x27/g; tloop; \
                s/(PRINT\(.*?)\x27\\\x27,\x27(.)\x27(.*)\)/\1\x27\\\2\x27\3\)/g; \
                s/(PRINT\()"(.)"(.*)\)/\1\x27\2\x27\3\)/g; \
                s/PRINT\((.*)\)/PRINT\(\1,\x27\\0\x27\,)/g; \
                s/PRINT\((.*)\)/AI_PRINT,\1/g'

# per VERSION flags
# BACKPORT_R1: turns on the JP/PAL bug fixes we chose to take into the NTSC build.
ifeq ($(VERSION), US)
 COUNTRYCODE := u
 OUTCODE := $(COUNTRYCODE)
 LANG := US
 LCDEFS := -DVERSION_US -DLANG_US -DREFRESH_NTSC -DLEFTOVERDEBUG -DBUGFIX_R0 -DBACKPORT_R1
 ASMDEFS := --defsym VERSION_US=1 --defsym LANG_US=1 --defsym REFRESH_NTSC=1 --defsym LEFTOVERDEBUG=1 --defsym BUGFIX_R0=1 --defsym BACKPORT_R1=1
 LDFILEOPTS := -DVERSION_$(LANG) -DOUTCODE=$(OUTCODE)
endif

ifeq ($(VERSION), EU)
 COUNTRYCODE := e
 OUTCODE := $(COUNTRYCODE)
 LANG := EU
 LCDEFS := -DVERSION_EU -DLANG_EU -DREFRESH_PAL -DBUGFIX_R1 -DBUGFIX_R2
 ASMDEFS := --defsym VERSION_EU=1 --defsym LANG_EU=1 --defsym REFRESH_PAL=1 --defsym BUGFIX_R1=1 --defsym BUGFIX_R2=1
 LDFILEOPTS := -DVERSION_$(LANG) -DOUTCODE=$(OUTCODE)
endif

# make UNLOCK_ALL=1: every mission and difficulty (007 too) is open in
# every save folder, for testing
ifeq ($(UNLOCK_ALL), 1)
  LCDEFS += -DUNLOCK_ALL_LEVELS
endif

ALLOWED_VERSIONS := US EU
ALLOWED_COUNTRYCODE := u e
ifeq ($(filter $(VERSION),$(ALLOWED_VERSIONS)),)
  $(error VERSION "$(VERSION)" not supported, use one of: $(ALLOWED_VERSIONS))
endif

BUILD_DIR_BASE := build
# BUILD_DIR is the location where all build artefacts are placed
BUILD_DIR      := $(BUILD_DIR_BASE)/$(OUTCODE)

# this file references variables defined above: BUILD_DIR, RZ_COMP
# this file defines and builds $(MUSIC_RZ_FILES)
include assets/Makefile.obseg
# this file references variables defined above: BUILD_DIR, RZ_COMP, COUNTRYCODE, LD, CC, CFLAGS, OBJCOPY, ConvertAIPRINT, OPTIMIZATION
# this file defines and builds OBSEGMENT, BG_SEG_FILES, BRIEF_RZ_FILES, CHR_RZ_FILES, GUN_RZ_FILES, PROP_RZ_FILES, ,SETUP_BUILD_FILES, STAN_BUILD_FILES, TEXT_RZ_FILES
include assets/Makefile.music

## Collect Objects ##

APPELF := $(BUILD_DIR)/ge007.$(OUTCODE).elf
APPROM := $(BUILD_DIR)/ge007.$(OUTCODE).z64
APPBIN := $(BUILD_DIR)/ge007.$(OUTCODE).bin

HEADERFILES := $(foreach dir,src,$(wildcard $(dir)/*.s))
HEADEROBJECTS := $(foreach file,$(HEADERFILES),$(BUILD_DIR)/$(file:.s=.o))

RSPCODE := $(foreach dir,rsp,$(wildcard $(dir)/*.s))
RSPOBJECTS := $(foreach file,$(RSPCODE),$(BUILD_DIR)/$(file:.s=.bin))

CODEFILES := $(foreach dir,src,$(wildcard $(dir)/*.c))
CODEOBJECTS := $(foreach file,$(CODEFILES),$(BUILD_DIR)/$(file:.c=.o))

GAMEFILES_C := $(foreach dir,src/game,$(wildcard $(dir)/*.c))
GAMEFILES_S := $(foreach dir,src/game,$(wildcard $(dir)/*.s))
GAMEOBJECTS := $(foreach file,$(GAMEFILES_S),$(BUILD_DIR)/$(file:.s=.o)) \
				$(foreach file,$(GAMEFILES_C),$(BUILD_DIR)/$(file:.c=.o))


ASSET_DATAFILES := assets/oddtextures.c assets/animationtable_data.c assets/animationtable_entries.c assets/font_dl.c assets/font_chardataj.c assets/font_chardatae.c assets/rarewarelogo.c
ASSET_DATAOBJECTS := $(foreach file,$(ASSET_DATAFILES),$(BUILD_DIR)/$(file:.c=.o))

ROMFILES2 := assets/romfiles2.s
ROMOBJECTS2 := $(BUILD_DIR)/assets/romfiles2.o

RAMROM_FILES := assets/ramrom/ramrom.s
RAMROM_OBJECTS := $(BUILD_DIR)/assets/ramrom/ramrom.o


FONTFILES_C := $(foreach dir,assets/font,$(wildcard $(dir)/*.c))
FONTOBJECTS := $(foreach file,$(FONTFILES_C),$(BUILD_DIR)/$(file:.c=.o))


MUSIC_FILES := $(foreach dir,assets/music,$(wildcard $(dir)/*.s))
MUSIC_OBJECTS := $(foreach file,$(MUSIC_FILES),$(BUILD_DIR)/$(file:.s=.o))

OBSEG_FILES := assets/obseg/ob_seg.s
OBSEG_OBJECTS := $(BUILD_DIR)/assets/obseg/ob_seg.o
OBSEG_RZ := $(BG_SEG_FILES) $(CHR_RZ_FILES) $(GUN_RZ_FILES) $(PROP_RZ_FILES) $(STAN_RZ_FILES) $(BRIEF_RZ_FILES) $(SETUP_RZ_FILES) $(TEXT_RZ_FILES)

IMAGE_BINS := assets/images/combined/combined.bin
IMAGE_OBJS := $(foreach file,$(IMAGE_BINS),$(BUILD_DIR)/$(file:.bin=.o))

RZFILES := inflate/inflate.c
RZOBJECTS := $(foreach file,$(RZFILES),$(BUILD_DIR)/src/$(file:.c=.o))

# libgcc helper functions GCC expects
CODEOBJECTS += $(BUILD_DIR)/src/gcc/libgcc_compat.o

OBJECTS := $(RSPOBJECTS) $(CODEOBJECTS) $(GAMEOBJECTS) $(RZOBJECTS) $(OBSEGMENT) $(ROMOBJECTS) $(RAMROM_OBJECTS) $(FONTOBJECTS) $(MUSIC_OBJECTS) $(IMAGE_OBJS)

## Command Line args for builders ##

INCLUDE := -I . -I include -I include/ultra64 -I include/PR -I src -I src/game -I src/inflate

CC := $(TOOLCHAIN)gcc
# Code generation flags shared by the game, libultra and the asset data.
# -nostdinc keeps the host's C library headers out; GCC's own freestanding headers are added back.
# -G 8 puts variables of up to 8 bytes in .sdata/.sbss, which code reaches with one instruction from $gp
# (see _gp in ge007.ld) instead of a lui + offset pair.
# The -fno-* flags on the third and fourth lines keep GCC from optimising away decomp-era undefined
# behaviour (type punning, signed overflow, null and out-of-bounds accesses).
# -fno-toplevel-reorder keeps globals in source order: some code still treats neighbouring globals
# as one array or struct (without it the menu text comes out garbled and the game hangs at file select).
# -mno-check-zero-division drops the trap GCC puts after every integer divide.
# GCC 14 made several old-C diagnostics into errors (implicit declarations, int/pointer
# mixing). The decomp code still has plenty of those, so keep them warnings for now; they
# go as files are cleaned up. -std=gnu17 holds the language at C17 until the move to C23.
GCC14_RELAX := -Wno-error=implicit-function-declaration -Wno-error=incompatible-pointer-types \
  -Wno-error=int-conversion -Wno-error=implicit-int -Wno-error=return-mismatch
CC_BASEFLAGS := -nostdinc -isystem $(shell $(CC) -print-file-name=include) -EB -march=vr4300 -mtune=vr4300 -mabi=32 -mfix4300 -mno-abicalls -fno-pic -G 8 \
  -ffreestanding -fno-builtin -fno-common -fms-extensions -fno-strict-aliasing -fwrapv -funsigned-char -fno-toplevel-reorder \
  -fno-delete-null-pointer-checks -fno-isolate-erroneous-paths-dereference -fno-aggressive-loop-optimizations -fno-math-errno \
  -fno-tree-loop-distribute-patterns -mno-check-zero-division -w -DTARGET_N64 -MMD -MP \
  -std=gnu17 $(GCC14_RELAX)
# Asset data (and the two boot-time blobs below) is copied out of the object byte for byte, so it
# must keep every variable in .data, zeros and duplicate constants included, and stay off $gp.
LAYOUT_CFLAGS := -G 0 -fno-zero-initialized-in-bss -fno-merge-constants
CFLAGS := $(CC_BASEFLAGS) $(INCLUDE) $(LCDEFS) $(OPTIMIZATION)

LD := $(TOOLCHAIN)ld
LD_SCRIPT := $(BUILD_DIR)/ge007.$(OUTCODE).ld

# --no-warn-mismatch is needed to link -mips3 object files (some libultra math) with the regular files compiled with -mips2
LDFLAGS := -T $(LD_SCRIPT) -Map $(BUILD_DIR)/ge007.$(OUTCODE).map --no-warn-mismatch

AS := $(TOOLCHAIN)as
ASFLAGS := -march=vr4300 -mabi=32 $(INCLUDE) $(ASMDEFS)
# Use the system installed armips if available. Otherwise use the one provided with this repository.
ifneq (,$(shell which armips 2>/dev/null))
  ARMIPS              := armips
else
  ARMIPS              := $(TOOLS_DIR)/armips
endif

OBJCOPY := $(TOOLCHAIN)objcopy












## Build Recipes ##

# Don't delete intermediate files from these targets on make completion.
.SECONDARY:
	$(APPELF) $(APPROM) $(APPBIN) $(ULTRAOBJECTS) $(BUILD_DIR)/ge007.$(OUTCODE).map \
	$(HEADEROBJECTS) $(BOOTOBJECTS) $(CODEOBJECTS) $(GAMEOBJECTS) $(RZOBJECTS) \
	$(OBSEG_OBJECTS) $(OBSEG_RZ) $(ROMOBJECTS) $(RAMROM_OBJECTS) $(FONTOBJECTS) $(MUSIC_OBJECTS) $(IMAGE_OBJS) $(MUSIC_RZ_FILES)

# Don't delete these intermediate targets on make cancellation.
.PRECIOUS: %.bin  %.o

# Run the following targets sequentially in this order (unnamed targets will still run in parallel)
.NOTPARALLEL: print_info create_directories $(APPROM)

# Phony Recipes - These targets are not files, Get Make to do something
.PHONY: print_info create_directories build_tools prerequisites all_p1 all default commonclean setupclean stanclean dataclean libultraclean codeclean clean nuke help cmdbuidler context extractassets forceextractassets textures convert_props convert_chrs convert_guns extract_u extract_e force_extract_u force_extract_e extract_rsp


# this file references variables defined above: BUILD_DIR, CFLAGWARNING, INCLUDE, LCDEFS
# this file defines and builds $(ULTRAOBJECTS)
include src/libultrare/Makefile.libultrare

# Build RSP
$(BUILD_DIR)/rsp/%.bin: rsp/*.s
	$(ARMIPS) -sym $@.sym -strequ CODE_FILE $(BUILD_DIR)/rsp/$*.bin -strequ DATA_FILE $(BUILD_DIR)/rsp/$*_data.bin $<

$(BUILD_DIR)/src/rspboot.o: $(BUILD_DIR)/rsp/rspboot.bin

#Build asm files in root
$(BUILD_DIR)/%.o: src/%.s
	$(AS) $(ASFLAGS) -o $@ $<

#Build asm files in src/
$(BUILD_DIR)/src/%.o: src/%.s
	$(AS) $(ASFLAGS) -o $@ $<

#Build Images
# Generate imagelist by syncing imagelist.u.csv (ROM offsets/sizes) with images.def (names)
$(BUILD_DIR)/imagelist.csv: imagelist.u.csv assets/images.def
	@mkdir -p $(BUILD_DIR)
	python3 scripts/make/sync_imagelist_with_def.py $@

assets/images/combined/combined.bin: $(BUILD_DIR)/imagelist.csv
	scripts/make/combine_images_named.sh $(BUILD_DIR)/imagelist.csv assets/images/combined

$(BUILD_DIR)/assets/images/combined/%.o: assets/images/combined/combined.bin
	$(LD) -r -b binary $< -o $@


#Compress Obseg
$(BUILD_DIR)/$(OBSEGMENT): $(OBSEG_RZ) $(IMAGE_OBJS)


#Build GCC support files
$(BUILD_DIR)/src/gcc/%.o: src/gcc/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

#Build C files in src/
# libultra's "memset" is an alias of bzero(ptr, len), so GCC's memset calls are redirected
# to the real memset in src/gcc/libgcc_compat.c.
$(BUILD_DIR)/src/%.o: src/%.c
	$(CC) -c $(CFLAGS) -o $@ $<
	$(OBJCOPY) --redefine-sym memset=gcc_memset $@

# chraidata.c: convert AI_PRINT commands from readable to byte-array first
$(BUILD_DIR)/src/game/chraidata.o: src/game/chraidata.c
	$(ConvertAIPRINT) $< | $(CC) -c $(CFLAGS) -I $(dir $<) -x c -o $@ -


#Build RamRom
$(BUILD_DIR)/assets/ramrom/%.o: assets/ramrom/%.s
	$(AS) $(ASFLAGS) -o $@ $<

#Build fonts
$(BUILD_DIR)/assets/font/%.o: assets/font/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

#Build asm files in assets/
$(BUILD_DIR)/assets/%.o: assets/%.s
	$(AS) $(ASFLAGS) -o $@ $<

#Build Obseg
$(BUILD_DIR)/assets/obseg/%.o: assets/obseg/%.s $(OBSEG_RZ)
	$(AS) $(ASFLAGS) -o $@ $<

# Objects whose sections the linker script or the asset tools take as raw data
$(BUILD_DIR)/assets/%.o: CFLAGS += $(LAYOUT_CFLAGS)
$(BUILD_DIR)/src/c_data_filler.o $(RZOBJECTS): CFLAGS += $(LAYOUT_CFLAGS)

#Build C files in assets/
$(BUILD_DIR)/assets/%.o: assets/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

#Link Files
$(APPELF): $(RSPOBJECTS) $(ULTRAOBJECTS) $(HEADEROBJECTS) $(OBSEG_RZ) $(BUILD_DIR)/$(OBSEGMENT) $(MUSIC_RZ_FILES) $(BOOTOBJECTS) $(CODEOBJECTS) $(GAMEOBJECTS) $(RZOBJECTS) $(ROMOBJECTS) $(ASSET_DATAOBJECTS) $(ROMOBJECTS2) $(RAMROM_OBJECTS) $(FONTOBJECTS) $(MUSIC_OBJECTS) $(OBSEG_OBJECTS) ge007.ld
	cpp $(LDFILEOPTS) -P ge007.ld -o $(BUILD_DIR)/ge007.$(OUTCODE).ld
	@# GCC emits extra sections (.rodata.*, .text.*) that the per-object lines name exactly
	sed -i -E -e '/\.o *\(/s/\((\.text|\.data|\.rodata)\)/(\1 \1.*)/' -e '/\.o *\(/s/\(\.bss\)/(.bss .bss.* COMMON)/' \
		-e 's|^( */DISCARD/ : \{)|\1\n      * (.comment);\n      * (.note*);|' $(BUILD_DIR)/ge007.$(OUTCODE).ld
	@echo "Linking Files into ELF"
	$(LD) $(LDFLAGS) -o $@

$(APPBIN): $(APPELF)
	@echo "Building ROM"
	$(OBJCOPY) $< $@ -O binary --gap-fill=0xff

$(APPROM):	$(APPBIN)
	@echo "Compressing ROM"
	$(DATASEG_COMP) $< $(OUTCODE)
	@echo "Finalizing ROM"
	$(N64CKSUM) $< $@


## Phony Recipes below - Get Make to do something ##

print_info:
	$(info VERSION=$(VERSION))
	$(info Building $(VERSION) ROM...)

create_directories:
	scripts/make/create_directories.sh "$(BUILD_DIR)" "$(COUNTRYCODE)"

build_tools:
	$(info Building tools...)
	scripts/make/build_tools.sh "$(MAKE)"

prerequisites: print_info create_directories build_tools extractassets

combine_images: assets/images/combined/combined.bin

all_p1: prerequisites
all: all_p1 $(APPROM)
	@echo "Rom File Generated in Build Directory."

commonclean:
	rm -f $(APPELF) $(APPROM) $(APPBIN) $(BUILD_DIR)/ge007.$(OUTCODE).map

setupclean: commonclean
	rm -f $(SETUP_BUILD_FILES)

stanclean: commonclean
	rm -f $(STAN_BUILD_FILES)

dataclean: commonclean stanclean setupclean
	rm -f $(OBSEG_OBJECTS) $(OBSEG_RZ) $(ROMOBJECTS) $(RAMROM_OBJECTS) $(FONTOBJECTS) $(MUSIC_OBJECTS) $(IMAGE_OBJS) $(MUSIC_RZ_FILES)
	rm -f $(ASSET_DATAOBJECTS) $(ROMOBJECTS2)
	rm -f $(BUILD_DIR)/imagelist.csv

libultraclean: commonclean
	rm -f $(ULTRAOBJECTS)

codeclean: commonclean libultraclean
	rm -f $(HEADEROBJECTS) $(BOOTOBJECTS) $(CODEOBJECTS) $(GAMEOBJECTS) $(RZOBJECTS) $(RSPOBJECTS)

clean: codeclean dataclean
	find $(BUILD_DIR) -name '*.d' -delete 2>/dev/null || true
	@echo "\nAll Code and Asset Binaries Cleared! Make will Re-Build these next time.\n"

nuke: clean
	scripts/make/clean_nuke.sh "$(ALLOWED_COUNTRYCODE)" "$(BUILD_DIR_BASE)"

help:
	@echo "mmakefile help"
	@echo ""
	@echo "  supported targets:"
	@echo ""
	@echo "    all                            Build all (default)"
	@echo "    clean                          Delete all known build artifacts"
	@echo "    nuke                           Delete all files explicitly listed in Makefile (same as make clean),"
	@echo "                                    all build output for all versions, any .bin file in assets folders,"
	@echo "                                    and asp/rsp bin."
	@echo "    dataclean                      Delete only asset build artifacts"
	@echo "    codeclean                      Delete only code (asm, .c) build artifacts"
	@echo "    libultraclean                  Delete only code (asm, .c) build artifacts "
	@echo "                                    from Rare's libultra files"
	@echo "    stanclean                      Delete only stan build artifacts"
	@echo "    setupclean                     Delete only setup build artifacts"
	@echo "    cmdbuidler                     BuildAI Commands"
	@echo "    context [file]                 BuildContext File from [file]"
	@echo "                                    eg make context src/game/chrai.c"
	@echo ""
	@echo ""
	@echo "  options:"
	@echo ""
	@echo "    VERSION=v                       Region version. (US is default)"
	@echo "                                    Supported values: ${ALLOWED_VERSIONS}\n"

include include/make/cmd.make



ifneq ($(filter-out context,$(MAKECMDGOALS)),)
 CONTEXTFILE := $(filter-out context ,$(MAKECMDGOALS))
else
 CONTEXTFILE := build/ctx.c
endif
context:
	@clear
	@echo Building Context File [ctx.h] from $(CONTEXTFILE)
	@echo "#define TRUE 1" > build/ctx.h
	@echo "#define FALSE 0" >> build/ctx.h
ifeq ($(CONTEXTFILE),build/ctx.c)
	@echo "#include <bondtypes.h>" > build/ctx.c
endif
	@sed -n -E ':x /\\$$/ { N; s/\\\n//g ; bx };''/(^\s*#define)|(\\$$)/p; /(\\$$)/p;' src/bondconstants.h src/bondtypes.h $(CONTEXTFILE) >> build/ctx.h
	@$(CC) $(CFLAGS) $(CONTEXTFILE) -E -P > build/ctx2.h 2> /dev/null || (rm build/ctx2.h && exit 1)
	@sed -E '/^\s*$$/d' build/ctx2.h >> build/ctx.h
	@rm build/ctx.c build/ctx2.h || exit 0
	@echo You can find it in Build [build/ctx.h].

extractassets: extract_u extract_e convert_props convert_chrs convert_guns

forceextractassets: force_extract_u force_extract_e convert_props convert_chrs convert_guns

extract_u:
	@if [ ! -f assets/obseg/ob__ob_end.seg ]; then \
		echo "Extracting assets for u..."; \
		if [ -f baserom.u.z64 ]; then \
			scripts/extract_baserom.u.sh; \
		else \
			echo "Error: baserom.u.z64 not found."; \
		fi \
	else \
		echo "Assets for u already extracted."; \
	fi

force_extract_u:
	@echo "Force extracting assets for u..."; \
	if [ -f baserom.u.z64 ]; then \
		scripts/extract_baserom.u.sh; \
	else \
		echo "Error: baserom.u.z64 not found."; \
	fi

extract_e:
	@if [ ! -f assets/obseg/text/e/LwaxP.bin ]; then \
		echo "Extracting assets for e..."; \
		if [ -f baserom.e.z64 ]; then \
			scripts/extract_diff.e.sh; \
		else \
			echo "Error: baserom.e.z64 not found."; \
		fi \
	else \
		echo "Assets for e already extracted."; \
	fi

force_extract_e:
	@echo "Force extracting assets for e..."; \
	if [ -f baserom.e.z64 ]; then \
		scripts/extract_diff.e.sh; \
	else \
		echo "Error: baserom.e.z64 not found."; \
	fi

extract_rsp:
	@if [ ! -f build/u/rsp/rspboot.bin ]; then \
		echo "Extracting rsp assets..."; \
		if [ -f baserom.u.z64 ]; then \
			scripts/extract_asp_gsp_rsp.sh; \
		else \
			echo "Error: baserom.u.z64 not found."; \
		fi \
	else \
		echo "RSP assets for already extracted."; \
	fi

convert_props:
	@echo "Converting prop binaries to Model.c..."
	@bin_count=$$(ls assets/obseg/prop/P*Z.bin 2>/dev/null | wc -l); \
	if [ $$bin_count -gt 0 ]; then \
		echo "Found $$bin_count prop binaries to convert..."; \
		python3 scripts/generate_prop_model_c.py --force --cleanup || true; \
	else \
		c_count=$$(find assets/obseg/prop -maxdepth 2 -name "Model.c" 2>/dev/null | wc -l); \
		if [ $$c_count -gt 0 ]; then \
			echo "Props already converted ($$c_count Model.c files found)."; \
		else \
			echo "No prop binaries found to convert."; \
		fi \
	fi

convert_chrs:
	@echo "Converting chr binaries to Model.c..."
	@bin_count=$$(ls assets/obseg/chr/C*Z.bin 2>/dev/null | wc -l); \
	if [ $$bin_count -gt 0 ]; then \
		echo "Found $$bin_count chr binaries to convert..."; \
		python3 scripts/generate_chr_c.py --force --cleanup || true; \
	else \
		c_count=$$(find assets/obseg/chr -maxdepth 2 -name "Model.c" 2>/dev/null | wc -l); \
		if [ $$c_count -gt 0 ]; then \
			echo "Chrs already converted ($$c_count Model.c files found)."; \
		else \
			echo "No chr binaries found to convert."; \
		fi \
	fi

convert_guns:
	@echo "Converting gun binaries to Model.c..."
	@bin_count=$$(ls assets/obseg/gun/G*Z.bin 2>/dev/null | wc -l); \
	if [ $$bin_count -gt 0 ]; then \
		echo "Found $$bin_count gun binaries to convert..."; \
		python3 scripts/generate_gun_c.py --force --cleanup || true; \
	else \
		c_count=$$(find assets/obseg/gun -maxdepth 2 -name "Model.c" 2>/dev/null | wc -l); \
		if [ $$c_count -gt 0 ]; then \
			echo "Guns already converted ($$c_count Model.c files found)."; \
		else \
			echo "No gun binaries found to convert."; \
		fi \
	fi

textures: tools/mktex/build/tex2png
	@echo "Processing textures..."
	mkdir -p assets/images/out
	$(foreach x,$(IMAGE_BINS),tools/mktex/build/tex2png $(x) assets/images/out ${\n})


tools/mktex/build/tex2png:
	@if [ ! -f tools/mktex/build/tex2png ]; then \
		echo "Building tex2png..."; \
		cd tools/mktex && $(MAKE); \
	fi

# Header dependencies written by -MMD
-include $(shell find $(BUILD_DIR) -name '*.d' 2>/dev/null)
