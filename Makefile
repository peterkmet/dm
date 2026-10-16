# The Dungeon Master port: the game, the test library, the Windows build, the checks.
CC          = clang
BASE_CFLAGS = -std=c11 -Wall -Wextra -Werror -Wshadow -Wmissing-prototypes -Wstrict-prototypes -Wvla \
              -Wimplicit-fallthrough -O2 -g
CFLAGS      = $(BASE_CFLAGS)
CPPFLAGS    = -Isrc -isystem third_party -MMD -MP
LDFLAGS     =
ifeq ($(SANITIZE),1)
CFLAGS  += -fsanitize=address,undefined
LDFLAGS += -fsanitize=address,undefined
endif
# Extra flags for one build, for example a developer check: make lib CFLAGS_EXTRA=-DVIEW_MIRROR_CHECK
CFLAGS += $(CFLAGS_EXTRA)

# SDL3 from Homebrew. Recursively expanded: only the targets that compile or link SDL code run brew.
SDL_PREFIX ?= $(shell brew --prefix sdl3)

B   = build
OBJ = $(B)/obj

SOURCES      = $(wildcard src/*/*.c)
SDL_SOURCES  = src/platform/main.c src/platform/platform.c
WINDOWS_ONLY = src/platform/control_win.c
POSIX_ONLY   = src/platform/control.c
GAME_SOURCES = $(filter-out $(WINDOWS_ONLY),$(SOURCES))
LIB_SOURCES  = $(filter-out $(SDL_SOURCES) $(WINDOWS_ONLY),$(SOURCES))
WIN_SOURCES  = $(filter-out $(POSIX_ONLY),$(SOURCES))
C_FILES      = $(wildcard src/*/*.[ch])

objects = $(patsubst src/%.c,$(1)/%.o,$(2))

all: $(B)/dm $(B)/libdm.dylib

lib: $(B)/libdm.dylib

$(OBJ)/%.o: src/%.c Makefile
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(call objects,$(OBJ),$(SDL_SOURCES)): CPPFLAGS += -I$(SDL_PREFIX)/include

$(B)/dm: $(call objects,$(OBJ),$(GAME_SOURCES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L$(SDL_PREFIX)/lib -lSDL3

$(B)/libdm.dylib: $(call objects,$(OBJ),$(LIB_SOURCES))
	$(CC) $(CFLAGS) $(LDFLAGS) -dynamiclib -o $@ $^

# Windows: cross-build with MinGW-w64 (brew install mingw-w64) and the SDL3 MinGW package, fetched into build/.
# Windows has no control socket: control_win.c replaces control.c.
WIN_CC            = x86_64-w64-mingw32-gcc
WIN_OBJ           = $(B)/obj-win
SDL_MINGW_VERSION = 3.4.18
SDL_MINGW         = $(B)/sdl-mingw/SDL3-$(SDL_MINGW_VERSION)/x86_64-w64-mingw32

windows: $(B)/dm.exe

$(WIN_OBJ)/%.o: src/%.c Makefile | $(SDL_MINGW)
	@mkdir -p $(@D)
	$(WIN_CC) $(CPPFLAGS) -I$(SDL_MINGW)/include $(BASE_CFLAGS) -c -o $@ $<

$(B)/dm.exe: $(call objects,$(WIN_OBJ),$(WIN_SOURCES))
	$(WIN_CC) $(BASE_CFLAGS) -o $@ $^ -L$(SDL_MINGW)/lib -lSDL3

$(SDL_MINGW):
	@mkdir -p $(B)/sdl-mingw
	curl -sfL https://github.com/libsdl-org/SDL/releases/download/release-$(SDL_MINGW_VERSION)/SDL3-devel-$(SDL_MINGW_VERSION)-mingw.tar.gz | tar xz -C $(B)/sdl-mingw

test:
	uv run pytest -q -n auto

format:
	uv run clang-format -i $(C_FILES)

check:
	uv run clang-format --dry-run --Werror $(C_FILES)
	uv run pytest -q tests/test_standard.py
	$(MAKE) test

dist:
	tools/dist/make-dist.sh

dist-win:
	tools/dist/make-dist-win.sh

# The personal play folder: the latest game; the saves there stay. Run: playthroughs/local/dm --data game --saves playthroughs/local
PLAY_DIR ?= playthroughs/local
play: $(B)/dm
	@mkdir -p $(PLAY_DIR)
	cp $(B)/dm $(PLAY_DIR)/dm.new && mv $(PLAY_DIR)/dm.new $(PLAY_DIR)/dm # a new file: a running game keeps the old one

clean:
	rm -rf $(B)

-include $(wildcard $(OBJ)/*/*.d $(WIN_OBJ)/*/*.d)

.PHONY: all lib windows test check format dist dist-win play clean
