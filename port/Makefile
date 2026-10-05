CC = clang
CFLAGS = -std=c11 -Wall -Wextra -O2 -g
SDL = $(shell brew --prefix sdl3)
B = ../build
GAME = $(filter-out src/main.c src/platform.c,$(wildcard src/*.c))
SRC = $(wildcard src/*.c src/*.h)

all: $(B)/dm $(B)/libdm.dylib

$(B)/dm: $(SRC)
	@mkdir -p $(B)
	$(CC) $(CFLAGS) -I$(SDL)/include -o $@ src/main.c src/platform.c $(GAME) -L$(SDL)/lib -lSDL3

$(B)/libdm.dylib: $(SRC)
	@mkdir -p $(B)
	$(CC) $(CFLAGS) -dynamiclib -o $@ $(GAME)

# The personal play folder: the latest game and the game files; the saves there stay.
LEGO = ../.playthrough-lego
lego: $(B)/dm
	@mkdir -p $(LEGO)
	rsync -a --exclude 'DMGAME.*' --exclude 'QUICK.DAT' --exclude 'CSBGAME.*' ../game/ $(LEGO)/
	cp $(B)/dm $(LEGO)/dm

clean:
	rm -rf $(B)

.PHONY: all clean lego
