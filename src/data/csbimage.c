/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data/csbimage.h"
#include "data/bytes.h"
#include "data/csbtext.h"
#include "data/kaos_addresses.h"
#include "data/tables.h"
#include <stdlib.h>
#include <string.h>

/* KAOS and APPB addresses of the tables that need a conversion. Sources: docs/csb/engine-diff.md "Data tables". */
#define CSB_APPB_ACTION_SETS         0x1047ac /* 44 records of 6 bytes, no pad bytes */
#define CSB_APPB_DIALOG_MOUSE_INPUTS 0x104650
#define CSB_APPB_DIALOG_TABLES       0x104500
#define DIALOG_TABLES_SIZE           336
#define DIALOG_TABLE_COUNT           8
#define CSB_APPB_SPELLS              0x1046c2
#define CSB_APPB_RESTART_BOXES       0x100dcc /* the endgame fills these two boxes */
#define CSB_APPB_DATA_BASE           0x10511e /* the APPB data base: all APPB tables are below it */

#define KAOS_ACTION_SET_BYTES  8
#define APPB_ACTION_SET_BYTES  6
#define ACTION_SET_ACTIONS     3
#define ACTION_SET_LEVELS      2
#define KAOS_ACTION_SET_LEVELS 4
#define KAOS_ACTION_SET_FLAGS  6
#define APPB_ACTION_SET_LEVELS 3
#define APPB_ACTION_SET_FLAGS  5

/* Tables with the same layout: KAOS address, APPB address, size. The APPB addresses are verified in
   docs/csb/engine-diff.md. Strided or split DM tables have one row per part. */
const CsbCopy g_csb_copies[] = {
    {KAOS_INITIAL_COLORMAP,                0x102f0a,               32                               }, /* interface palette */
    {KAOS_POINTER_PALETTE,                 0x102f2a,               32                               }, /* pointer colors */
    {KAOS_ARROW_POINTER_WIDTH,             0x1005ec,               292                              }, /* the arrow pointer bitmap with its size words */
    {KAOS_HAND_POINTER_WIDTH,              0x100710,               292                              }, /* the hand pointer bitmap with its size words */
    {KAOS_MOUSE_POINTER_ZONES,             0x10084a,               192                              },
    {KAOS_MOUSE_POINTER_HOTSPOTS,          0x10091a,               32                               },
    {KAOS_POINTER_SHADOW_COLORS,           0x100984,               16                               },
    {KAOS_SOUND_VOLUMES,                   0x1009f2,               625                              },
    {KAOS_CREDITS_PALETTE,                 0x100f18,               32                               }, /* CSB colors */
    {KAOS_ENTRANCE_PALETTE,                0x100f38,               32                               }, /* CSB colors */
    {KAOS_DUNGEON_PALETTES,                0x100f78,               192                              },
    {KAOS_ORDERED_CELLS_TO_ATTACK,         0x10103a,               32                               },
    {KAOS_WOUND_PROBABILITY_INDEX_TO_MASK, 0x10105a,               4                                },
    {KAOS_SLOTS_TO_STEAL_FROM,             0x10105e,               8                                },
    {KAOS_ICON_GRAPHIC_FIRST_INDEX,        0x101066,               14                               },
    {KAOS_TORCH_CHARGE_TO_ICON_OFFSET,     0x101074,               16                               },
    {KAOS_SLOT_BOXES,                      0x101084,               184                              },
    {KAOS_SLOT_MASKS,                      0x10113e,               76                               },
    {KAOS_LIGHT_POWER_TO_LIGHT_AMOUNT,     0x10118a,               32                               },
    {KAOS_PALETTE_LIGHT_THRESHOLDS,        0x1011aa,               12                               },
    {KAOS_CHAMPION_COLORS,                 0x1011b6,               4                                },
    {KAOS_WOUND_DEFENSE_FACTOR,            0x1011ba,               6                                },
    {KAOS_SLOT_DROP_ORDER,                 0x1011e4,               60                               },
    {KAOS_SQUARE_TYPE_TO_EVENT_TYPE,       0x101222,               8                                },
    {KAOS_SOUNDS,                          0x10122a,               420                              },
    {KAOS_DIRECTION_TO_STEP_EAST,          0x10153a,               8                                },
    {KAOS_DIRECTION_TO_STEP_NORTH,         0x101542,               8                                },
    {KAOS_NEW_GAME_EXTRA_THINGS,           0x10155a,               16                               },
    {KAOS_OBJECT_INFO,                     0x10156a,               1080                             },
    {KAOS_WEAPON_INFO,                     0x1019a2,               276                              },
    {KAOS_ARMOUR_INFO,                     0x101ab6,               232                              },
    {KAOS_JUNK_WEIGHTS,                    0x101ba0,               53                               },
    {KAOS_FOOD_VALUES,                     0x101bd6,               16                               },
    {KAOS_DOOR_INFO,                       0x101be6,               8                                },
    {KAOS_MESSAGE_ESCAPE_STRINGS,          0x101bee,               256                              },
    {KAOS_ESCAPE_REPLACEMENT_CHARS,        0x101cee,               64                               },
    {KAOS_INSCRIPTION_ESCAPE_STRINGS,      0x101d2e,               256                              },
    {KAOS_GROUP_DIRECTIONS_PACKED,         0x101e2e,               4                                },
    {KAOS_CREATURE_INFO,                   0x101e32,               702                              },
    {KAOS_CREATURE_SOUNDS,                 0x1020f0,               36                               },
    {KAOS_FIXED_POSSESSIONS,               0x102114,               80                               },
    {KAOS_LIST_SEPARATOR,                  0x10264a,               3                                }, /* ", " of the object attributes */
    {KAOS_BONES_SEPARATOR,                 0x10267a,               2                                }, /* " " between champion name and BONES */
    {KAOS_ATTRIBUTES_SUFFIX,               0x1026ec,               2                                }, /* ")" */
    {KAOS_ATTRIBUTES_PREFIX,               0x1026ea,               2                                }, /* "(" */
    {KAOS_INVISIBILITY_PALETTE_CHANGES,    0x102d8a,               16                               },
    {KAOS_LAYOUT_BITMAPS,                  0x102f50,               840                              },
    {KAOS_VIEWPORT_X,                      0x103298,               4                                }, /* viewport origin */
    {KAOS_VIEWPORT_WIDTH,                  0x1032a6,               4                                }, /* width and height */
    {KAOS_POINTER_SHADOW_DX,               0x1032b6,               4                                }, /* shadow dx and dy */
    {KAOS_SLOT_BOX_BORDER,                 0x1032ba,               2                                },
    {KAOS_PORTRAIT_WIDTH,                  0x1032bc,               4                                }, /* width and height */
    {KAOS_CHAMPION_ICON_WIDTH,             0x1032c0,               4                                }, /* width and height */
    {KAOS_DIALOG_MESSAGE_LINE_PAD,         0x1032ca,               2                                }, /* dialog message line pad */
    {KAOS_WALL_TEXT_GLYPH_WIDTH,           0x1032d2,               4                                }, /* wall text glyph size */
    {KAOS_DAMAGE_SPLAT_LARGE_WIDTH,        0x1032dc,               4                                }, /* large damage splat size */
    {KAOS_DAMAGE_SPLAT_SMALL_WIDTH,        0x1032e0,               4                                }, /* small damage splat size */
    {KAOS_BAR_SHADOW_OFFSET,               0x1032e4,               2                                }, /* panel bar shadow */
    {KAOS_WALL_SET_LAYOUT_IDS,             0x10343c,               62                               }, /* layout ids 0-30 */
    {KAOS_MIRRORED_WALL_LAYOUT_IDS,        0x1034dc,               30                               }, /* layout ids 31-45 */
    {KAOS_VIEW_SQUARE_LANE,                0x103616,               23                               },
    {KAOS_VIEW_SQUARE_DEPTH,               0x10362d,               23                               },
    {KAOS_VIEW_SQUARE_OBJECT_COORD,        0x103644,               23                               },
    {KAOS_VIEW_SQUARE_ALCOVE_COORD,        0x10365b,               23                               },
    {KAOS_VIEW_SQUARE_CREATURE_COORD,      0x103672,               23                               },
    {KAOS_VIEW_SQUARE_EXPLOSION_COORD,     0x103689,               23                               },
    {KAOS_VIEW_SQUARE_FIELD_ASPECT,        0x1036a0,               23                               },
    {KAOS_FIELD_ASPECTS,                   0x1036b7,               96                               },
    {KAOS_WALL_ORNAMENT_DEPTHS,            0x10373a,               14                               }, /* wall ornament depth by view */
    {KAOS_FLOOR_ORNAMENT_VIEW_OFFSETS,     0x103748,               11                               }, /* floor ornament graphic offset by view */
    {KAOS_ALCOVE_ORNAMENT_INDICES,         0x103753,               3                                },
    {KAOS_FOUNTAIN_ORNAMENT_INDICES,       0x103757,               1                                },
    {KAOS_WALL_ORNAMENT_COORD_SETS,        0x103758,               60                               },
    {KAOS_FLOOR_ORNAMENT_COORD_SETS,       0x103794,               9                                },
    {KAOS_DOOR_ORNAMENT_COORD_SETS,        0x10379d,               12                               },
    {KAOS_DOOR_BUTTON_COORD_SETS,          0x1037a9,               1                                },
    {KAOS_PALETTE_CHANGES_D3,              0x1037aa,               64                               }, /* the D3 and D2 palette changes, then those of the door ornaments */
    {KAOS_PLAQUE_HEIGHTS + 1,              0x1037ea,               16                               }, /* visible text plaque heights */
    {KAOS_OBJECT_ASPECTS,                  0x1037fa,               340                              }, /* its last 4 bytes are the first projectile aspect */
    {KAOS_PROJECTILE_ASPECTS,              0x10394a,               60                               },
    {KAOS_PALETTE_CHANGES_SMOKE,           0x103986,
     48                                                                                             }, /* the smoke palette changes, then the D3 and D2 object palette changes */
    {KAOS_OBJECT_SCALES,                   0x1039b6,               5                                }, /* object scales */
    {KAOS_PROJECTILE_SCALES,               0x1039bb,               7                                }, /* projectile scales */
    {KAOS_EXPLOSION_SCALES,                0x1039c2,               5                                }, /* explosion scales */
    {KAOS_PILE_SHIFTS,                     0x1039c7,               32                               }, /* pile shifts */
    {KAOS_CREATURE_ASPECTS,                0x1039e8,               162                              },
    {KAOS_CREATURE_REPLACEMENT_COLOR_SETS, 0x103a8a,               182                              },
    {KAOS_PALETTE_CHANGES_CREATURE_D3,     0x103b40,
     56                                                                                             }, /* the D3 and D2 creature palette changes and the creature shift sets */
    {KAOS_REBIRTH_SCALES,                  0x103b78,               7                                }, /* rebirth step 2 scales */
    {KAOS_REBIRTH_STEP_1_SCALES,           0x103b7f,               7                                }, /* rebirth step 1 scales */
    {KAOS_MOUSE_INPUT_ENTRANCE,            0x103c24,               60                               }, /* entrance mouse input: CSB buttons */
    {KAOS_MOUSE_INPUT_ENDGAME,             0x103c60,               24                               }, /* endgame mouse input: CSB has RESTART only */
    {KAOS_MOUSE_INPUT_INTERFACE,           0x103c78,               240                              },
    {KAOS_MOUSE_INPUT_MOVEMENT,            0x103d68,               108                              },
    {KAOS_MOUSE_INPUT_INVENTORY,           0x103dd4,               456                              },
    {KAOS_MOUSE_INPUT_SLEEPING,            0x103f9c,               36                               },
    {KAOS_MOUSE_INPUT_FROZEN,              0x103fc0,               36                               },
    {KAOS_MOUSE_INPUT_ACTION_NAMES,        0x103fe4,               60                               },
    {KAOS_MOUSE_INPUT_ACTION_ICONS,        0x104020,               60                               },
    {KAOS_MOUSE_INPUT_SPELL_AREA,          0x10405c,               120                              },
    {KAOS_MOUSE_INPUT_STATUS_BOX,          0x1040d4,               156                              },
    {KAOS_MOUSE_INPUT_CHEST,               0x104170,               108                              },
    {KAOS_MOUSE_INPUT_RESURRECT,           0x1041dc,               48                               },
    {KAOS_MOUSE_INPUT_NAME_ENTRY,          0x10420c,               432                              },
    {KAOS_KEYS_ENTRANCE,                   0x1043bc,               8                                }, /* entrance keys: CSB has one */
    {KAOS_KEYS_INTERFACE,                  0x1043c4,               28                               }, /* interface keys */
    {KAOS_KEYS_MOVEMENT,                   0x1043e0,               28                               }, /* movement keys */
    {KAOS_KEYS_SLEEPING,                   0x1043fc,               12                               },
    {KAOS_KEYS_FROZEN,                     0x104408,               12                               },
    {KAOS_DIALOG_TABLES,                   CSB_APPB_DIALOG_TABLES, DIALOG_TABLES_SIZE               },
    {KAOS_MOVEMENT_ARROW_TO_STEP_FORWARD,  0x104694,               8                                },
    {KAOS_MOVEMENT_ARROW_TO_STEP_RIGHT,    0x10469c,               8                                },
    {KAOS_SYMBOL_BASE_MANA_COST,           0x1046a4,               24                               },
    {KAOS_SYMBOL_MANA_COST_MULTIPLIER,     0x1046bc,               6                                },
    {KAOS_ACTIONS_DISABLED,                0x1049e0,
     264                                                                                            }, /* the action tables from the disabled ticks to the skill index, 44 bytes each */
    {KAOS_ACTION_EXPERIENCE_GAIN,          0x104ae8,               44                               }, /* CSB values */
    {KAOS_PALETTE_CHANGES_ACTION_ICON,     0x104b52,               16                               },
    {KAOS_DIALOG_MESSAGE_COLORS,           0x104c20,               4                                },
    {KAOS_DIALOG_VERSION_TEXT,             0x104c24,               6                                }, /* version text: V3.1 */
    {CSB_SPELLS,                           CSB_APPB_SPELLS,        CSB_SPELL_COUNT * CSB_SPELL_BYTES},
    {CSB_RESTART_BOXES,                    CSB_APPB_RESTART_BOXES, 2 * CSB_BOX_BYTES                },
};
const int g_csb_copy_count = ARRAY_COUNT(g_csb_copies);

/* The byte of an image at a memory address of the original. */
static uint8_t *appb_at(const FtlImage *img, uint32_t addr)
{
    return img->data + (addr - img->base);
}

/* Copies the tables, converts the action sets, moves the dialog pointers to the KAOS tables, and builds the texts. */
int csb_image_build(const FtlImage *appb, FtlImage *out)
{
    uint32_t size = CSB_IMAGE_END - KAOS_BASE;
    uint8_t *mem = calloc(size, 1);
    if (!mem || appb->base != KAOS_BASE || appb->data_base != CSB_APPB_DATA_BASE) {
        free(mem);
        return -1;
    }
    FtlImage built = {.data = mem, .size = size, .base = KAOS_BASE};
    for (int i = 0; i < g_csb_copy_count; i++)
        memcpy(appb_at(&built, g_csb_copies[i].kaos), appb_at(appb, g_csb_copies[i].appb), g_csb_copies[i].size);
    for (int i = 0; i < ACTION_SET_COUNT; i++) {
        const uint8_t *a = appb_at(appb, CSB_APPB_ACTION_SETS + i * APPB_ACTION_SET_BYTES);
        uint8_t *k = appb_at(&built, KAOS_ACTION_SETS + i * KAOS_ACTION_SET_BYTES);
        memcpy(k, a, ACTION_SET_ACTIONS);
        memcpy(k + KAOS_ACTION_SET_LEVELS, a + APPB_ACTION_SET_LEVELS, ACTION_SET_LEVELS);
        k[KAOS_ACTION_SET_FLAGS] = a[APPB_ACTION_SET_FLAGS];
    }
    /* the dialog pointers move with their tables */
    for (int i = 0; i < DIALOG_TABLE_COUNT; i++) {
        const uint8_t *p = appb_at(appb, CSB_APPB_DIALOG_MOUSE_INPUTS + 4 * i);
        uint32_t table = BE32(p);
        WR32(appb_at(&built, KAOS_DIALOG_MOUSE_INPUTS + 4 * i), table - CSB_APPB_DIALOG_TABLES + KAOS_DIALOG_TABLES);
    }
    if (csb_text_build(appb, &built) != 0) {
        free(mem);
        return -1;
    }
    *out = built;
    return 0;
}
