#include <stdlib.h>
#include <string.h>
#include "bytes.h"
#include "csbimage.h"
#include "csbtext.h"
#include "kaos_addresses.h"
#include "spells.h"
#include "tables.h"

/* KAOS and APPB addresses of the tables that need a conversion. Sources: docs/csb-engine-diff.md "Data tables". */
#define APPB_ACTION_SETS         0x1047ac /* 44 records of 6 bytes, no pad bytes */
#define APPB_DIALOG_MOUSE_INPUTS 0x104650
#define APPB_DIALOG_TABLES       0x104500
#define DIALOG_TABLES_SIZE       336
#define DIALOG_TABLE_COUNT       8
#define APPB_SPELLS              0x1046c2
#define APPB_RESTART_BOXES       0x100dcc /* the endgame fills these two boxes */
#define APPB_DATA_BASE           0x10511e /* the APPB data base: all APPB tables are below it */

enum { KAOS_ACTION_SET_BYTES = 8, APPB_ACTION_SET_BYTES = 6, ACTION_SET_ACTIONS = 3, ACTION_SET_LEVELS = 2 };
enum { KAOS_ACTION_SET_LEVELS = 4, KAOS_ACTION_SET_FLAGS = 6, APPB_ACTION_SET_LEVELS = 3, APPB_ACTION_SET_FLAGS = 5 };

/* Tables with the same layout: KAOS address, APPB address, size. The APPB addresses are verified in
   docs/csb-engine-diff.md. Strided or split DM tables have one row per part. */
const CsbCopy csb_copies[] = {
    {0x100170,           0x102f0a,           32                           }, /* g_initial_colormap: interface palette */
    {0x100190,           0x102f2a,           32                           }, /* g_initial_colormap + 32: pointer colors */
    {0x1002ac,           0x1005ec,           292                          }, /* g_arrow_pointer_bitmap with its size words */
    {0x1003d0,           0x100710,           292                          }, /* g_hand_pointer_bitmap with its size words */
    {0x1004f8,           0x10084a,           192                          }, /* g_mouse_pointer_zones */
    {0x1005c8,           0x10091a,           32                           }, /* g_mouse_pointer_hotspots */
    {0x1005fe,           0x100984,           16                           }, /* g_pointer_shadow_colors_object */
    {0x100696,           0x1009f2,           625                          }, /* g_sound_volume_table */
    {0x100a46,           0x100f18,           32                           }, /* g_credits_palette: CSB colors */
    {0x100a66,           0x100f38,           32                           }, /* g_entrance_palette: CSB colors */
    {0x100aa6,           0x100f78,           192                          }, /* g_dungeon_palettes */
    {0x100b66,           0x10103a,           32                           }, /* g_ordered_cells_to_attack */
    {0x100b86,           0x10105a,           4                            }, /* g_wound_probability_index_to_mask */
    {0x100b8a,           0x10105e,           8                            }, /* g_slots_to_steal_from */
    {0x100b92,           0x101066,           14                           }, /* g_icon_graphic_first_index */
    {0x100ba0,           0x101074,           16                           }, /* g_torch_charge_to_icon_offset */
    {0x100bb0,           0x101084,           184                          }, /* g_slot_boxes */
    {0x100c68,           0x10113e,           76                           }, /* g_slot_masks */
    {0x100cb4,           0x10118a,           32                           }, /* g_light_power_to_light_amount */
    {0x100cd4,           0x1011aa,           12                           }, /* g_palette_light_thresholds */
    {0x100ce0,           0x1011b6,           4                            }, /* g_champion_colors */
    {0x100ce4,           0x1011ba,           6                            }, /* g_wound_defense_factor */
    {0x100d0e,           0x1011e4,           60                           }, /* g_slot_drop_order */
    {0x100d4a,           0x101222,           8                            }, /* g_square_type_to_event_type */
    {0x100d52,           0x10122a,           420                          }, /* g_sounds */
    {0x101038,           0x10153a,           8                            }, /* g_direction_to_step_east */
    {0x101040,           0x101542,           8                            }, /* g_direction_to_step_north */
    {0x101058,           0x10155a,           16                           }, /* g_new_game_extra_thing_count */
    {0x101068,           0x10156a,           1080                         }, /* g_object_info */
    {0x1014a0,           0x1019a2,           276                          }, /* g_weapon_info */
    {0x1015b4,           0x101ab6,           232                          }, /* g_armour_info */
    {0x10169c,           0x101ba0,           53                           }, /* g_junk_weights */
    {0x1016d2,           0x101bd6,           16                           }, /* g_food_values */
    {0x1016e2,           0x101be6,           8                            }, /* g_door_info */
    {0x1016ea,           0x101bee,           256                          }, /* g_message_escape_strings */
    {0x1017ea,           0x101cee,           64                           }, /* g_escape_replacement_chars */
    {0x10182a,           0x101d2e,           256                          }, /* g_inscription_escape_strings */
    {0x10192a,           0x101e2e,           4                            }, /* g_group_directions_packed */
    {0x10192e,           0x101e32,           702                          }, /* g_creature_info */
    {0x101bec,           0x1020f0,           36                           }, /* g_creature_sounds */
    {0x101c10,           0x102114,           80                           }, /* g_fixed_possessions */
    {0x101ebe,           0x10264a,           3                            }, /* ", " of the object attributes */
    {0x101ec6,           0x10267a,           2                            }, /* " " between champion name and BONES */
    {0x101ec8,           0x1026ec,           2                            }, /* ")" */
    {0x101eca,           0x1026ea,           2                            }, /* "(" */
    {0x10247a,           0x102d8a,           16                           }, /* g_invisibility_palette_changes */
    {0x1025f2,           0x102f50,           840                          }, /* g_layout_bitmaps */
    {0x10293a,           0x103298,           4                            }, /* viewport origin */
    {0x102948,           0x1032a6,           4                            }, /* g_viewport_width, g_viewport_height */
    {0x102952,           0x1032b6,           4                            }, /* g_pointer_shadow_dx, g_pointer_shadow_dy */
    {0x102956,           0x1032ba,           2                            }, /* g_slot_box_border */
    {0x102958,           0x1032bc,           4                            }, /* g_portrait_width, g_portrait_height */
    {0x10295c,           0x1032c0,           4                            }, /* g_champion_icon_width, g_champion_icon_height */
    {0x102966,           0x1032ca,           2                            }, /* dialog message line pad */
    {0x10296e,           0x1032d2,           4                            }, /* wall text glyph size */
    {0x102978,           0x1032dc,           4                            }, /* large damage splat size */
    {0x10297c,           0x1032e0,           4                            }, /* small damage splat size */
    {0x102980,           0x1032e4,           2                            }, /* panel bar shadow */
    {0x102a22,           0x10343c,           62                           }, /* g_wall_set_layout_ids 0-30 */
    {0x102a60,           0x1034dc,           30                           }, /* g_wall_set_layout_ids 31-45 */
    {0x102a7e,           0x103616,           23                           }, /* g_view_square_lane */
    {0x102a96,           0x10362d,           23                           }, /* g_view_square_depth */
    {0x102aae,           0x103644,           23                           }, /* g_view_square_object_coord_index */
    {0x102ac6,           0x10365b,           23                           }, /* g_view_square_alcove_object_coord_index */
    {0x102ade,           0x103672,           23                           }, /* g_view_square_creature_coord_index */
    {0x102af6,           0x103689,           23                           }, /* g_view_square_explosion_coord_index */
    {0x102b0e,           0x1036a0,           23                           }, /* g_view_square_field_aspect_index */
    {0x102b26,           0x1036b7,           96                           }, /* g_field_aspects */
    {0x102ba6,           0x10373a,           14                           }, /* wall ornament depth by view */
    {0x102bb4,           0x103748,           11                           }, /* floor ornament graphic offset by view */
    {0x102bc0,           0x103753,           3                            }, /* g_alcove_ornament_indices */
    {0x102bc3,           0x103757,           1                            }, /* g_fountain_ornament_indices */
    {0x102bc4,           0x103758,           60                           }, /* g_wall_ornament_coord_sets */
    {0x102c00,           0x103794,           9                            }, /* g_floor_ornament_coord_sets */
    {0x102c0a,           0x10379d,           12                           }, /* g_door_ornament_coord_sets */
    {0x102c16,           0x1037a9,           1                            }, /* g_door_button_coord_sets */
    {0x102c18,           0x1037aa,           64                           }, /* g_palette_changes_d3, d2, door ornament d3, d2 */
    {0x102c58,           0x1037ea,           16                           }, /* visible text plaque heights */
    {0x102c68,           0x1037fa,           340                          }, /* g_object_aspects: its last 4 bytes are the first projectile aspect */
    {0x102db8,           0x10394a,           60                           }, /* g_projectile_aspects */
    {0x102df4,           0x103986,           48                           }, /* g_palette_changes_smoke, object d3, object d2 */
    {0x102e24,           0x1039b6,           5                            }, /* object scales */
    {0x102e2a,           0x1039bb,           7                            }, /* projectile scales */
    {0x102e32,           0x1039c2,           5                            }, /* explosion scales */
    {0x102e38,           0x1039c7,           32                           }, /* pile shifts */
    {0x102e58,           0x1039e8,           162                          }, /* g_creature_aspects */
    {0x102efa,           0x103a8a,           182                          }, /* g_creature_replacement_color_sets */
    {0x102fb0,           0x103b40,           56                           }, /* g_palette_changes_creature_d3, d2, g_creature_shift_sets */
    {0x102fe8,           0x103b78,           7                            }, /* rebirth step 2 scales */
    {0x102ff0,           0x103b7f,           7                            }, /* rebirth step 1 scales */
    {0x10318c,           0x103c24,           60                           }, /* entrance mouse input: CSB buttons */
    {0x1031d4,           0x103c60,           24                           }, /* endgame mouse input: CSB has RESTART only */
    {0x1031f8,           0x103c78,           240                          }, /* g_mouse_input_interface */
    {0x1032e8,           0x103d68,           108                          }, /* g_mouse_input_movement */
    {0x103354,           0x103dd4,           456                          }, /* g_mouse_input_champion_inventory */
    {0x10351c,           0x103f9c,           36                           }, /* g_mouse_input_party_sleeping */
    {0x103540,           0x103fc0,           36                           }, /* g_mouse_input_frozen_game */
    {0x103564,           0x103fe4,           60                           }, /* g_mouse_input_action_area_names */
    {0x1035a0,           0x104020,           60                           }, /* g_mouse_input_action_area_icons */
    {0x1035dc,           0x10405c,           120                          }, /* g_mouse_input_spell_area */
    {0x103654,           0x1040d4,           156                          }, /* g_mouse_input_champion_status_box */
    {0x1036f0,           0x104170,           108                          }, /* g_mouse_input_panel_chest */
    {0x10375c,           0x1041dc,           48                           }, /* g_mouse_input_panel_resurrect_reincarnate */
    {0x10378c,           0x10420c,           432                          }, /* g_mouse_input_champion_name_entry */
    {0x10393c,           0x1043bc,           8                            }, /* entrance keys: CSB has one */
    {0x103948,           0x1043c4,           28                           }, /* interface keys */
    {0x103964,           0x1043e0,           28                           }, /* movement keys */
    {0x103980,           0x1043fc,           12                           }, /* g_keyboard_input_party_sleeping */
    {0x10398c,           0x104408,           12                           }, /* g_keyboard_input_frozen_game */
    {KAOS_DIALOG_TABLES, APPB_DIALOG_TABLES, DIALOG_TABLES_SIZE           },
    {0x103c56,           0x104694,           8                            }, /* g_movement_arrow_to_step_forward_count */
    {0x103c5e,           0x10469c,           8                            }, /* g_movement_arrow_to_step_right_count */
    {0x103c66,           0x1046a4,           24                           }, /* g_symbol_base_mana_cost */
    {0x103c7e,           0x1046bc,           6                            }, /* g_symbol_mana_cost_multiplier */
    {0x103eac,           0x1049e0,           264                          }, /* g_action_disabled_ticks ... g_action_skill_index, 44 bytes each */
    {0x103fb4,           0x104ae8,           44                           }, /* g_action_experience_gain: CSB values */
    {0x103ffa,           0x104b52,           16                           }, /* g_palette_changes_action_area_object_icon */
    {0x104026,           0x104c20,           4                            }, /* g_dialog_message_colors */
    {0x10402c,           0x104c24,           6                            }, /* version text: V3.1 */
    {CSB_SPELLS,         APPB_SPELLS,        CSB_SPELL_COUNT * SPELL_BYTES},
    {CSB_RESTART_BOXES,  APPB_RESTART_BOXES, 2 * CSB_BOX_BYTES            },
};
const int csb_copy_count = sizeof csb_copies / sizeof csb_copies[0];

/* Copies the tables, converts the action sets, moves the dialog pointers to the KAOS tables, and builds the texts. */
int csb_image_build(const FtlImage *appb, FtlImage *out)
{
    uint32_t size = CSB_IMAGE_END - KAOS_BASE;
    uint8_t *mem = calloc(size, 1);
    if (!mem || appb->base != KAOS_BASE || appb->data_base != APPB_DATA_BASE)
        return free(mem), -1;
    const uint8_t *src = appb->data - KAOS_BASE;
    uint8_t *dst = mem - KAOS_BASE;
    for (int i = 0; i < csb_copy_count; i++)
        memcpy(dst + csb_copies[i].kaos, src + csb_copies[i].appb, csb_copies[i].size);
    for (int i = 0; i < ACTION_SET_COUNT; i++) {
        const uint8_t *a = src + APPB_ACTION_SETS + i * APPB_ACTION_SET_BYTES;
        uint8_t *k = dst + KAOS_ACTION_SETS + i * KAOS_ACTION_SET_BYTES;
        memcpy(k, a, ACTION_SET_ACTIONS);
        memcpy(k + KAOS_ACTION_SET_LEVELS, a + APPB_ACTION_SET_LEVELS, ACTION_SET_LEVELS);
        k[KAOS_ACTION_SET_FLAGS] = a[APPB_ACTION_SET_FLAGS];
    }
    /* the dialog pointers move with their tables */
    for (int i = 0; i < DIALOG_TABLE_COUNT; i++) {
        const uint8_t *p = src + APPB_DIALOG_MOUSE_INPUTS + 4 * i;
        uint32_t table = BE32(p);
        WR32(dst + KAOS_DIALOG_MOUSE_INPUTS + 4 * i, table - APPB_DIALOG_TABLES + KAOS_DIALOG_TABLES);
    }
    *out = (FtlImage){mem, size, KAOS_BASE, CSB_SPELLS, 0};
    return csb_text_build(appb, out);
}
