# Builds Nintendo's N64 library (libultra) from the ultralib decompilation in lib/ultralib
# (https://github.com/decompals/ultralib, version 2.0L, see lib/ultralib/README.md) into one
# archive, $(ULTRALIB). The linker takes only the objects the game uses from it.
#
# GoldenEye's own replacements for library files live in lib/ultralib-ge and are built in
# place of the upstream file of the same name (see ULTRALIB_GE_FILES).
#
# This file uses CC, AR, OBJCOPY, CC_BASEFLAGS, OPTIMIZATION and BUILD_DIR from the main Makefile.

ULTRALIB_DIR := lib/ultralib
ULTRALIB_GE_DIR := lib/ultralib-ge
ULTRALIB := $(BUILD_DIR)/lib/libultra.a

# The parts of the library GoldenEye can use. Of debug/ only kdebugserver.c is here: it holds
# __osThreadSave, which the exception handler uses.
ULTRALIB_SRC_DIRS := audio debug gu mgu io libc os vimodes

# Files that are for other builds of the library (debug, emulator boards, development hosts)
# or that version 2.0L replaces with the hand-written mgu versions.
ULTRALIB_EXCLUDE := \
    os/initialize_emu.c os/initialize_isv.c os/initialize_kmc.c os/initialize_msp.c \
    os/initrdb.c os/rdbsend.c os/syncputchars.c os/testhost.c \
    gu/mtxcatf.c gu/normalize.c gu/scale.c gu/translate.c

# GoldenEye's replacements, by path under the source tree
#   audio/env.c       - Rare's envelope mixer: volume ramps are linear (see the file)
#   os/createthread.c - new threads start with $gp set, for small data (-G 8)
#   libc/ll.s, libc/llcvt.s - 64-bit integer maths and conversions in assembly. The upstream C
#                       versions rely on the original compiler using 64-bit registers; GCC turns
#                       them into calls to its own helpers, which call these (src/gcc/libgcc_compat.c).
ULTRALIB_GE_FILES := audio/env.c os/createthread.c libc/ll.s libc/llcvt.s

ULTRALIB_ALL_C := $(foreach dir,$(ULTRALIB_SRC_DIRS),$(patsubst $(ULTRALIB_DIR)/src/%,%,$(wildcard $(ULTRALIB_DIR)/src/$(dir)/*.c)))
ULTRALIB_ALL_S := $(foreach dir,$(ULTRALIB_SRC_DIRS),$(patsubst $(ULTRALIB_DIR)/src/%,%,$(wildcard $(ULTRALIB_DIR)/src/$(dir)/*.s)))
ULTRALIB_GE_REPLACED := $(foreach f,$(ULTRALIB_GE_FILES),$(basename $(f)).c $(basename $(f)).s)
ULTRALIB_C := $(filter-out $(ULTRALIB_EXCLUDE) $(ULTRALIB_GE_REPLACED),$(ULTRALIB_ALL_C))
ULTRALIB_S := $(filter-out $(ULTRALIB_EXCLUDE) $(ULTRALIB_GE_REPLACED),$(ULTRALIB_ALL_S))

ULTRALIB_OBJECTS := $(foreach f,$(ULTRALIB_C:.c=.o) $(ULTRALIB_S:.s=.o),$(BUILD_DIR)/$(ULTRALIB_DIR)/src/$(f)) \
                    $(foreach f,$(basename $(ULTRALIB_GE_FILES)),$(BUILD_DIR)/$(ULTRALIB_GE_DIR)/$(f).o)

# The library's headers depend on its version and on the build kind (_FINALROM: no debugger
# support), so the game builds with these too
ULTRALIB_VERSION_DEFINES := -DBUILD_VERSION=VERSION_L -DBUILD_VERSION_STRING=\"2.0L\" -D_FINALROM
ULTRALIB_DEFINES := $(ULTRALIB_VERSION_DEFINES) -DNDEBUG -DMODERN_CC -D_MIPS_SZLONG=32 -D__USE_ISOC99 -D_ULTRA64
ULTRALIB_INCLUDE := -I $(ULTRALIB_DIR)/include -I $(ULTRALIB_DIR)/include/compiler/modern_gcc \
                    -I $(ULTRALIB_DIR)/include/PR

# The library is old C, so it builds as C17 (C23 changes the meaning of "()" and adds keywords),
# without warnings. It shares the game's code generation flags, -G 8 included, so the game can
# reach the library's small variables through $gp. nosdata.h keeps the variables the boot
# code leaves at fixed addresses, and data defined in assembly, off $gp.
ULTRALIB_CFLAGS = $(filter-out -std=%,$(CC_BASEFLAGS)) -std=gnu17 -include $(ULTRALIB_GE_DIR)/nosdata.h -w -D_LANGUAGE_C -DF3DEX_GBI \
                  $(ULTRALIB_DEFINES) $(ULTRALIB_INCLUDE) $(OPTIMIZATION)
ULTRALIB_ASFLAGS = -x assembler-with-cpp -I $(ULTRALIB_DIR)/include/compiler -EB -march=vr4300 -mabi=32 -mno-abicalls -fno-pic -G 0 -w \
                   -DMIPSEB -D_LANGUAGE_ASSEMBLY -D_MIPS_SIM=1 $(ULTRALIB_DEFINES) $(ULTRALIB_INCLUDE)

# These two are written for the original Fast3D display list format
$(BUILD_DIR)/$(ULTRALIB_DIR)/src/gu/parse_gbi.o: ULTRALIB_CFLAGS += -UF3DEX_GBI -DF3D_GBI

# libultra's "memset" is an alias of bzero(ptr, len), so GCC's own memset calls are redirected
# to the real memset in src/gcc/libgcc_compat.c, as for the game code.
$(BUILD_DIR)/$(ULTRALIB_DIR)/src/%.o: $(ULTRALIB_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(ULTRALIB_CFLAGS) -o $@ $<
	$(OBJCOPY) --redefine-sym memset=gcc_memset $@

$(BUILD_DIR)/$(ULTRALIB_DIR)/src/%.o: $(ULTRALIB_DIR)/src/%.s
	@mkdir -p $(dir $@)
	$(CC) -c $(ULTRALIB_ASFLAGS) -o $@ $<

$(BUILD_DIR)/$(ULTRALIB_GE_DIR)/%.o: $(ULTRALIB_GE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(ULTRALIB_CFLAGS) -I $(ULTRALIB_DIR)/src/$(dir $*) -o $@ $<
	$(OBJCOPY) --redefine-sym memset=gcc_memset $@

$(BUILD_DIR)/$(ULTRALIB_GE_DIR)/%.o: $(ULTRALIB_GE_DIR)/%.s
	@mkdir -p $(dir $@)
	$(AS) -march=vr4300 -mabi=32 -I include -o $@ $<

$(ULTRALIB): $(ULTRALIB_OBJECTS)
	@mkdir -p $(dir $@)
	rm -f $@
	$(AR) rcs $@ $^
