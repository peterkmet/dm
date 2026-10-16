#ifndef STARTEND_H
#define STARTEND_H
/* Prefixes: startend_ wait_ */
#include <stdbool.h>
#include <stdint.h>

/* Sequences outside the game loop (KAOS STARTEND.C and save_game): the save dialog, resuming a
   saved game, the endgame screens, and the fuse sequence with Lord Chaos. They wait frame by
   frame like the original; the program shows each frame through g_frame_hook. */

/* Shows one 50 Hz frame and reads the input into the command queue. NULL (tests, headless):
   waits end at once and waits for input end without input. */
extern void (*g_frame_hook)(void);
extern bool g_quit_requested;     /* the player quits: the program ends */
extern bool g_restart_requested;  /* RESTART THIS GAME on the endgame screen */
extern bool g_return_to_entrance; /* the endgame ended: the program shows the entrance */

/* The directory of the save file of the adventure (DMGAME.DAT or CSBGAME.DAT). The default is the game data directory. */
void startend_set_save_dir(const char *dir);
const char *startend_save_dir(void);

/* Returns true after one frame, false when there are no frames or the player quits. */
bool wait_frame(void);
/* Not 0 while wait_frame runs a frame: a dialog, a sequence, or an endgame wait. */
extern int16_t g_waiting_frame;
void wait_frames(int16_t count);

/* Command 140: CANCEL, SAVE, QUIT. QUIT ends the game through the endgame. CSB: CANCEL, SAVE AND
   PLAY, SAVE AND QUIT. */
void startend_save_game(void);
enum { SAVED_GAME = 1, SAVED_NEW_ADVENTURE = 2 };
/* Writes the save file of the adventure. In the CSB prison it makes the new adventure (CSBGAME.DAT)
   and loads it; the caller starts it (gameloop_start_loaded_game). Returns SAVED_GAME,
   SAVED_NEW_ADVENTURE, or 0. On 0, *reason is the reason why the prison party cannot make the
   adventure, or NULL when the file cannot be written. */
int startend_save_current_game(const char **reason);
/* RESUME: loads the saved game with its dialogs. Returns true when the game is loaded. */
bool startend_load_saved_game(void);
/* The end of the game: game over (party_dead 1), won (g_game_won), or quit (party_dead 0). It
   returns after RESTART THIS GAME loaded the saved game, or after the credits with
   g_return_to_entrance set. */
void startend_run_endgame(int16_t party_dead);
/* The party fused Lord Chaos: the final explosions, the texts of square (0, 0), then the endgame. */
void startend_run_fuse_sequence(void);
/* Sensor 18: waits delay seconds, then the won endgame. CSB: the ending animation (ENDA.DAT) at once. */
void startend_run_endgame_sensor(uint16_t delay);

#endif
