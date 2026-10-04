#ifndef GAMELOOP_H
#define GAMELOOP_H
#include <stdint.h>

/* Game start and the main loop (KAOS initialize_game, start_game, game_loop, process_entrance). */

#define POINTER_SIZE 32   /* largest pointer image, in pixels */

/* Loads KAOS.FTL and Graphics.DAT from dir and sets up the interface. Returns 1 on success. */
int gameloop_load_data(const char *dir);
/* The directory that gameloop_load_data read the game files from. */
const char *gameloop_data_dir(void);
/* Shows the entrance: graphic 4 with the closed doors and the ENTER, RESUME, and QUIT buttons. */
void entrance_draw(void);
/* ENTER (resume 0) or RESUME (resume 1) was clicked: the pressed button, a click, a short wait. */
void entrance_press_button(int resume);
/* After ENTER: the doors open over the view of the start square. Needs the frames of g_frame_hook. */
void entrance_open_doors(void);
/* Loads Dungeon.DAT from dir and starts a new game on the start square. Returns 1 on success. */
int gameloop_new_game(const char *dir);
/* start_game for a game that savegame_load restored: the party map, "GAME LOADED", the interface. */
void gameloop_start_loaded_game(void);
/* gameloop_start_loaded_game without the "GAME LOADED" dialog: the game goes on at once (quick load). */
void gameloop_continue_loaded_game(void);
/* The light amount of the party: light sources in the hands and the light spells. */
int16_t gameloop_party_light(void);
/* set_dungeon_view_palette: sets the darkness from the party light. Returns the party light amount. */
int16_t gameloop_set_light(void);
/* The dungeon view of the party in the light of the party (draw_dungeon_view). */
void gameloop_draw_view(void);
/* Runs the waiting commands of one 50 Hz frame. Returns 1 when the wait for input is over and a
   game tick is due (gameloop_tick). */
int gameloop_frame(void);
/* One game tick: map change, timeline, view, sounds, damage, time, champion effects, interface. */
void gameloop_tick(void);
/* The pointer image at screen position (x, y): px gets w x h colors (0 transparent) of the sprite
   palette. Returns 0 when the pointer is hidden. */
int gameloop_pointer(int16_t x, int16_t y, int in_game, uint8_t *px, int *w, int *h, int *hot_x, int *hot_y);
const uint16_t *gameloop_pointer_palette(void);

#endif
