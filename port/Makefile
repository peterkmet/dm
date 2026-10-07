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

clean:
	rm -rf $(B)

.PHONY: all clean lego
