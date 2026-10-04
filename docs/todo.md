# Ideas for later

User wishes for future versions. Not planned yet.

- Optional: CSB intro (game-csb/disk-b/ANIM.DAT); it needs the music chunks (MI, MD, MF, SF) and MIDI/NAKED.AMG.
- Optional: the FTL logo of SWSH.FTL before the DM title: a 320 x 200 packed graphic at 0x100106 and a sample at 0x100a5a (9078 bytes, period 334), both inside SWSH.FTL; a palette script of 30 vertical blanks (table 0x10009a); no skip.
- Entrance buttons as in KAOS process_entrance (0x126fea): ENTER and RESUME show the pressed button (graphic 11 into zone 435 or 436, source y 0 or 13), play sound 1, and wait 20 vertical blanks before the doors open or the game loads.
