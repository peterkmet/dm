CC = clang
CFLAGS = -std=c11 -Wall -Wextra -O2 -g
SDL = $(shell brew --prefix sdl3)
B = ../build
LAYERS = data engine ui platform
INCLUDES = $(addprefix -Isrc/,$(LAYERS))
GAME = $(filter-out src/platform/main.c src/platform/platform.c,$(wildcard $(addsuffix /*.c,$(addprefix src/,$(LAYERS)))))
SRC = $(wildcard $(addsuffix /*.[ch],$(addprefix src/,$(LAYERS))))

all: $(B)/dm $(B)/libdm.dylib

$(B)/dm: $(SRC)
	@mkdir -p $(B)
	$(CC) $(CFLAGS) $(INCLUDES) -I$(SDL)/include -o $@ src/platform/main.c src/platform/platform.c $(GAME) -L$(SDL)/lib -lSDL3

$(B)/libdm.dylib: $(SRC)
	@mkdir -p $(B)
	$(CC) $(CFLAGS) $(INCLUDES) -dynamiclib -o $@ $(GAME)

# The personal play folder: the latest game; the saves there stay. Run: playthroughs/lego/dm --data game --saves playthroughs/lego
LEGO = ../playthroughs/lego
lego: $(B)/dm
	@mkdir -p $(LEGO)
	cp $(B)/dm $(LEGO)/dm.new && mv $(LEGO)/dm.new $(LEGO)/dm # a new file: a running game keeps the old one

# Windows: cross-build with MinGW-w64 (brew install mingw-w64) and the SDL3 MinGW package, fetched into build/.
# Windows has no control socket: control_win.c replaces control.c. The gcc-only checks are off; the clang build is the
# reference with 0 warnings.
SDL_MINGW_VERSION = 3.4.18
SDL_MINGW = $(B)/sdl-mingw/SDL3-$(SDL_MINGW_VERSION)/x86_64-w64-mingw32
WIN_CFLAGS = $(CFLAGS) -Wno-format-truncation -Wno-implicit-fallthrough -Wno-unknown-pragmas
WIN_SRC = $(filter-out src/platform/control.c,$(wildcard $(addsuffix /*.c,$(addprefix src/,$(LAYERS)))))

windows: $(B)/dm.exe

$(B)/dm.exe: $(SRC) | $(SDL_MINGW)
	x86_64-w64-mingw32-gcc $(WIN_CFLAGS) $(INCLUDES) -I$(SDL_MINGW)/include -o $@ $(WIN_SRC) -L$(SDL_MINGW)/lib -lSDL3

$(SDL_MINGW):
	@mkdir -p $(B)/sdl-mingw
	curl -sfL https://github.com/libsdl-org/SDL/releases/download/release-$(SDL_MINGW_VERSION)/SDL3-devel-$(SDL_MINGW_VERSION)-mingw.tar.gz | tar xz -C $(B)/sdl-mingw

clean:
	rm -rf $(B)

.PHONY: all clean lego windows
