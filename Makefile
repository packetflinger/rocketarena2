# Native builds target the host OS (Linux or macOS) and, by default, the
# host CPU. ARCH names the CPU in the output filename, game$(ARCH).$(SHLIBEXT),
# using the names Quake 2 engines look for: i386, x86_64, aarch64.
#
#   make                  host arch, e.g. gamex86_64.so / gameaarch64.dylib
#   make ARCH=i386        the original's 32-bit gamei386.so; on an x86_64
#                         Linux host this needs a multilib toolchain
#                         (gcc-multilib/libc6-dev-i386 or equivalent)
#   make ARCH=x86_64      on an Apple Silicon Mac, an Intel/Rosetta build
#
# Some engines (e.g. Yamagi Quake II) load a plain game.so / game.dylib
# instead, so rename the output for those.
OS:=$(shell uname -s)
HOST_ARCH:=$(shell uname -m | sed -e 's/^i.86$$/i386/' -e 's/^amd64$$/x86_64/' \
	-e 's/^arm64$$/aarch64/')
ARCH?=$(HOST_ARCH)

ifeq ($(OS),Darwin)
OSNAME=macos
SHLIBEXT=dylib
# Apple clang cross-compiles between its architectures with -arch,
# which spells aarch64 as arm64.
ARCH_FLAGS?=-arch $(subst aarch64,arm64,$(ARCH))
PLATFORM_LDFLAGS=
ifeq ($(origin CC),default)
CC=cc
endif
else
OSNAME=linux
SHLIBEXT=so
ifneq ($(ARCH),$(HOST_ARCH))
ifeq ($(ARCH),i386)
ARCH_FLAGS?=-m32
endif
ifeq ($(ARCH),x86_64)
ARCH_FLAGS?=-m64
endif
endif
ARCH_FLAGS?=
# dlopen's home before glibc 2.34; harmless (an empty stub) after.
PLATFORM_LDFLAGS=-ldl
ifeq ($(origin CC),default)
CC=gcc
endif
endif

BUILD_DEBUG_DIR=debug-$(OSNAME)-$(ARCH)
BUILD_RELEASE_DIR=release-$(OSNAME)-$(ARCH)
BUILD_WIN32_DEBUG_DIR=debug-win32
BUILD_WIN32_RELEASE_DIR=release-win32
BUILD_WIN64_DEBUG_DIR=debug-win64
BUILD_WIN64_RELEASE_DIR=release-win64

# -fexpensive-optimizations is GCC-only; clang (the default cc on macOS,
# and an option on Linux) warns about it on every file.
ifeq ($(shell $(CC) --version 2>/dev/null | grep -c clang),0)
GCC_ONLY_CFLAGS=-fexpensive-optimizations
else
GCC_ONLY_CFLAGS=
endif

BASE_CFLAGS=-Dstricmp=strcasecmp $(ARCH_FLAGS)
RELEASE_CFLAGS=$(BASE_CFLAGS) -ffast-math -funroll-loops \
	-fomit-frame-pointer $(GCC_ONLY_CFLAGS)
DEBUG_CFLAGS=$(BASE_CFLAGS) -g
LDFLAGS=$(PLATFORM_LDFLAGS) -lm $(ARCH_FLAGS)

SHLIBCFLAGS=-fPIC
SHLIBLDFLAGS=-shared

# MinGW cross-compilers, producing gamex86.dll/gamex64.dll -- same
# game$(ARCH).$(SHLIBEXT) naming as the native targets above, and
# gamex86.dll matches the original Windows RA2 DLL's real filename.
# No -fPIC (meaningless for PE), no -ldl (nothing dlopen's on Windows),
# and stricmp is native to the Windows CRT so the strcasecmp remap
# vanilla Linux needs is dropped too. -lws2_32 covers the Winsock calls
# net_compat.h switches gslog.c/stats.c to under _WIN32.
#
# -DNDEBUG matches the real gamex86.dll, which was built with it: the Windows
# build compiles out every assert() in darray.c/hashtable.c/gbucket.c/q_shared.c,
# where the Linux build keeps them (gamei386.so carries all 31 sites).
CC_WIN32?=i686-w64-mingw32-gcc
CC_WIN64?=x86_64-w64-mingw32-gcc
WIN_BASE_CFLAGS=-DNDEBUG
WIN_RELEASE_CFLAGS=$(WIN_BASE_CFLAGS) -ffast-math -funroll-loops \
	-fomit-frame-pointer -fexpensive-optimizations
WIN_DEBUG_CFLAGS=$(WIN_BASE_CFLAGS) -g
WIN_LDFLAGS=-lm -lws2_32
WIN_SHLIBCFLAGS=

# game.def restricts the DLL's export table to GetGameAPI (the only entry
# point the engine actually looks up), matching the original Windows RA2
# build. Without it, GNU ld's PE auto-export fallback exports every global
# symbol instead -- harmless but needlessly leaky. Native ELF .so builds
# don't use a .def file at all, hence this being empty by default.
EXTRA_LINK_INPUTS?=
WIN_EXTRA_LINK_INPUTS=game.def

DO_CC=$(CC) $(CFLAGS) -o $@ -c $<
DO_SHLIB_CC=$(CC) $(CFLAGS) $(SHLIBCFLAGS) -o $@ -c $<

TARGETS=$(BUILDDIR)/game$(ARCH).$(SHLIBEXT) \

build_debug:
	@mkdir -p $(BUILD_DEBUG_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)"

build_release:
	@mkdir -p $(BUILD_RELEASE_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS)"

build_win32_debug:
	@mkdir -p $(BUILD_WIN32_DEBUG_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_WIN32_DEBUG_DIR) CFLAGS="$(WIN_DEBUG_CFLAGS)" \
		CC=$(CC_WIN32) ARCH=x86 SHLIBEXT=dll SHLIBCFLAGS="$(WIN_SHLIBCFLAGS)" LDFLAGS="$(WIN_LDFLAGS)" \
		EXTRA_LINK_INPUTS="$(WIN_EXTRA_LINK_INPUTS)"

build_win32_release:
	@mkdir -p $(BUILD_WIN32_RELEASE_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_WIN32_RELEASE_DIR) CFLAGS="$(WIN_RELEASE_CFLAGS)" \
		CC=$(CC_WIN32) ARCH=x86 SHLIBEXT=dll SHLIBCFLAGS="$(WIN_SHLIBCFLAGS)" LDFLAGS="$(WIN_LDFLAGS)" \
		EXTRA_LINK_INPUTS="$(WIN_EXTRA_LINK_INPUTS)"

build_win64_debug:
	@mkdir -p $(BUILD_WIN64_DEBUG_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_WIN64_DEBUG_DIR) CFLAGS="$(WIN_DEBUG_CFLAGS)" \
		CC=$(CC_WIN64) ARCH=x64 SHLIBEXT=dll SHLIBCFLAGS="$(WIN_SHLIBCFLAGS)" LDFLAGS="$(WIN_LDFLAGS)" \
		EXTRA_LINK_INPUTS="$(WIN_EXTRA_LINK_INPUTS)"

build_win64_release:
	@mkdir -p $(BUILD_WIN64_RELEASE_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_WIN64_RELEASE_DIR) CFLAGS="$(WIN_RELEASE_CFLAGS)" \
		CC=$(CC_WIN64) ARCH=x64 SHLIBEXT=dll SHLIBCFLAGS="$(WIN_SHLIBCFLAGS)" LDFLAGS="$(WIN_LDFLAGS)" \
		EXTRA_LINK_INPUTS="$(WIN_EXTRA_LINK_INPUTS)"

all: build_debug build_release

win32: build_win32_debug build_win32_release
win64: build_win64_debug build_win64_release
windows: win32 win64

targets: $(TARGETS)

GAME_OBJS = \
	$(BUILDDIR)/arena.o \
	$(BUILDDIR)/darray.o \
	$(BUILDDIR)/g_ai.o \
	$(BUILDDIR)/g_cmds.o \
	$(BUILDDIR)/g_combat.o \
	$(BUILDDIR)/g_func.o \
	$(BUILDDIR)/g_items.o \
	$(BUILDDIR)/g_main.o \
	$(BUILDDIR)/g_misc.o \
	$(BUILDDIR)/g_monster.o \
	$(BUILDDIR)/g_phys.o \
	$(BUILDDIR)/g_save.o \
	$(BUILDDIR)/g_spawn.o \
	$(BUILDDIR)/g_svcmds.o \
	$(BUILDDIR)/g_target.o \
	$(BUILDDIR)/g_trigger.o \
	$(BUILDDIR)/g_turret.o \
	$(BUILDDIR)/g_utils.o \
	$(BUILDDIR)/g_weapon.o \
	$(BUILDDIR)/gbucket.o \
	$(BUILDDIR)/gslog.o \
	$(BUILDDIR)/gstats.o \
	$(BUILDDIR)/hashtable.o \
	$(BUILDDIR)/maploop.o \
	$(BUILDDIR)/md5c.o \
	$(BUILDDIR)/menu.o \
	$(BUILDDIR)/nonport.o \
	$(BUILDDIR)/p_client.o \
	$(BUILDDIR)/p_hud.o \
	$(BUILDDIR)/p_trail.o \
	$(BUILDDIR)/p_view.o \
	$(BUILDDIR)/p_weapon.o \
	$(BUILDDIR)/q_shared.o \
	$(BUILDDIR)/ra2menus.o

$(BUILDDIR)/game$(ARCH).$(SHLIBEXT) : $(GAME_OBJS)
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(GAME_OBJS) $(EXTRA_LINK_INPUTS) $(LDFLAGS)

$(BUILDDIR)/g_ai.o :        g_ai.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/p_client.o :    p_client.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_cmds.o :      g_cmds.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_svcmds.o :    g_svcmds.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_combat.o :    g_combat.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_func.o :      g_func.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_items.o :     g_items.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_main.o :      g_main.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_misc.o :      g_misc.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_monster.o :   g_monster.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_phys.o :      g_phys.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_save.o :      g_save.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_spawn.o :     g_spawn.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_target.o :    g_target.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_trigger.o :   g_trigger.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_turret.o :    g_turret.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_utils.o :     g_utils.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_weapon.o :    g_weapon.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/p_hud.o :       p_hud.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/p_trail.o :     p_trail.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/p_view.o :      p_view.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/p_weapon.o :    p_weapon.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/q_shared.o :    q_shared.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/arena.o :       arena.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/maploop.o :     maploop.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/menu.o :        menu.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ra2menus.o :    ra2menus.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/gslog.o :       gslog.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/darray.o :      darray.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/hashtable.o :   hashtable.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/gbucket.o :     gbucket.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/nonport.o :     nonport.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/md5c.o :        md5c.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/gstats.o :      gstats.c
	$(DO_SHLIB_CC)

#####

clean: clean-debug clean-release clean-win32 clean-win64

clean-debug:
	$(MAKE) clean2 BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)"

clean-release:
	$(MAKE) clean2 BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(DEBUG_CFLAGS)"

clean-win32:
	$(MAKE) clean2 BUILDDIR=$(BUILD_WIN32_DEBUG_DIR) CFLAGS="$(WIN_DEBUG_CFLAGS)"
	$(MAKE) clean2 BUILDDIR=$(BUILD_WIN32_RELEASE_DIR) CFLAGS="$(WIN_RELEASE_CFLAGS)"

clean-win64:
	$(MAKE) clean2 BUILDDIR=$(BUILD_WIN64_DEBUG_DIR) CFLAGS="$(WIN_DEBUG_CFLAGS)"
	$(MAKE) clean2 BUILDDIR=$(BUILD_WIN64_RELEASE_DIR) CFLAGS="$(WIN_RELEASE_CFLAGS)"

clean2:
	-rm -f $(GAME_OBJS)
