/*
 * re1_types.h - Capcom data structures of RE1 (Dual Shock), reconstructed from usage.
 *
 * Field offsets and types: dualshock/tools/infer_struct.py over all functions (the types are the ones the
 * matching code was built with); field names from the RE1 PC decomp where it has the same structure
 * (Entity), otherwise from what the code does; see docs/game_notes.md.
 * Types-only header, included by every src file. The globals of these types are declared in
 * src/main/globals.h (main program) and src/<overlay>/globals.h.
 */
#ifndef RE1_TYPES_H
#define RE1_TYPES_H

/* BYTES(n): n raw bytes with no alignment (an anonymous struct; typedef it when two places must share the
   type, e.g. both sides of a copy). Only used where the record's real type compiles differently:
   - a copy of a record as raw bytes: the assignment compiles to unaligned word copies (lwl/lwr + swl/swr),
     while the record's own (aligned) type would compile to lw/sw;
   - a pointer stepped n bytes at a time where a plain pointer of the element type changes the -O2 code
     (GCC 2.6.3 schedules struct-typed memory accesses differently).
   Joints / entities / vectors / matrices are copied and stepped with their own types (Joint, Entity,
   SVECTOR, VECTOR, MATRIX, GsRVIEW2, RoomLight, ...) wherever that matched. */
#define BYTES(n) struct { u8 b[n]; }

/* Game object ("entity"): player / enemies / room objects. *g_CurrentEntity = current object. */
/* Billboard (0x78 bytes): a flat textured quad drawn on the floor, made by billboard_init and changed by
   billboard_set_color / billboard_adj_size / billboard_set_size, drawn by billboard_draw (at the entity's
   position + offset, floor height, turned with the entity). Every entity has one at 0xE4: its round drop
   SHADOW (subtractive blend, grey 0x808080). When an enemy dies, its death code recolours it (0xFFFF50:
   subtracting that leaves dark red), shrinks it and grows it a little every frame: the spreading BLOOD
   POOL (both confirmed in game). The PC decomp calls offset_x/y/z "pushVelocity". */
typedef struct Billboard {
    /* 0x00 */ u16 offset_x;              /* position relative to the entity */
    /* 0x02 */ u16 offset_y;
    /* 0x04 */ u16 offset_z;
    /* 0x06 */ u16 pad06;
    /* 0x08 */ POLY_FT4 poly[2];          /* one quad per frame buffer (r0/g0/b0 = colour) */
    /* 0x58 */ SVECTOR corners[4];        /* the quad's corners (size: billboard_set_size / adj_size) */
} Billboard;

/* Entity layout note (2026-10-16): bytes 0x0C..0x6B are the PS1 SDK's object handle and coordinate system
   for the model: 0x0C GsDOBJ-style handle (attribute, coord2 pointer at 0x10, TMD pointer at 0x14 =
   model_load_buffer, packet at 0x18) and a GsCOORDINATE2 at 0x1C (flg at 0x1C, coord MATRIX 0x20..0x3F whose
   translation t[0..2] IS pos_x/pos_y/pos_z at 0x34/0x38/0x3C, workm 0x40..0x5F, param 0x60, super 0x64:
   a room object attached to the player sets super = 0x800C5140 = the player's coordinate system).
   tmd_link_object(tmd, e + 0xC, e + 0x1C) links them. Kept as flat readable names here. */
typedef struct Entity {
    /* 0x000 */ u8 status_flags;
    /* 0x001 */ u8 id;
    /* 0x002 */ u8 behavior_flags;       /* player: the EQUIPPED WEAPON's item id (inventory[g_EquippedSlot].id;
                                            1 = knife, 0 = none; weapon tables use id - 2). PC decomp: behavior_flags */
    /* 0x003 */ u8 has_enter_switch_zone;
    /* 0x004 */ s32 sca_info;
    /* 0x008 */ s32 sca_hit_data;         /* pointer to the hit-centre offset (3 shorts: x, y, z from the entity position); PC pSca_hit_data */
    /* 0x00C */ u8 pad00C[0x14 - 0x0C];
    /* 0x014 */ s32 model_load_buffer;
    /* 0x018 */ s32 packet;  /* model draw packets (GsPresetObject; drawn from +buf*0x28+8) */
    /* 0x01C */ s32 sca_matrix_head;
    /* 0x020 */ u8 coord_m[0x34 - 0x20];  /* rotation part of the coordinate MATRIX (GsCOORDINATE2 at 0x1C: flg,
                                           then this; its t[] are pos_x/y/z) */
    /* 0x034 */ s32 pos_x;               /* position (passed as &obj->pos_x to the 3D sound code) */
    /* 0x038 */ s32 pos_y;
    /* 0x03C */ s32 pos_z;
    /* 0x040 */ s32 coord_workm;  /* first word of the coordinate system's workm MATRIX (0x40..0x5F; param 0x60, super 0x64, sub 0x68) */
    /* 0x044 */ u8 pad044[0x6C - 0x44];
    /* 0x06C */ u16 position_vx;
    /* 0x06E */ u16 position_vy;
    /* 0x070 */ u16 position_vz;
    /* 0x072 */ u16 rot_x;
    /* 0x074 */ u16 angle;              /* probable direction (Y angle) */
    /* 0x076 */ u16 angle_z;
    /* 0x078 */ s16 speed_vx;
    /* 0x07A */ s16 speed_vy;
    /* 0x07C */ s16 speed_vz;
    /* 0x07E */ u16 prev_angle;  /* the angle before this frame (collision restores it); plant 42 stores a phase in its body copies */
    /* 0x080 */ s16 scd_rot_speed_x;      /* room script (SCD) rotation speeds: added to rot_x / angle each frame */
    /* 0x082 */ s16 scd_rot_speed_y;
    /* 0x084 */ u8 state;             /* 4-level routine (state machine) indices; */
    /* 0x085 */ s8 sub_state;             /*  sometimes reset together with one 32-bit store */
    /* 0x086 */ u8 action_behavior;
    /* 0x087 */ u8 action_state;
    /* 0x088 */ s16 health;
    /* 0x08A */ s8 hit_state;
    /* 0x08B */ u8 pad08B;
    /* 0x08C */ s8 blend_counter;
    /* 0x08D */ u8 joint_count;
    /* 0x08E */ s16 floor_y;  /* floor height under the entity (PC posY): doors, stairs, shadow, landing effects, footsteps;
                                           room objects: collision half-depth */
    /* 0x090 */ s32 anim_header;
    /* 0x094 */ s32 anim_base;
    /* 0x098 */ s32 joints;
    /* 0x09C */ s32 weapon_part_anim_slot;
    /* 0x0A0 */ s32 weapon_part_anim_object;
    /* 0x0A4 */ u8 pad0A4[0xAC - 0xA4];
    /* 0x0AC */ s32 mirror_joints;  /* the mirror reflection's copy of the skeleton (SCD 0x0F mirror_set) */
    /* 0x0B0 */ s32 mirror_weapon_tmd;  /* the mirror copy's weapon model (joint 0xE), 0x7A00 bytes */
    /* 0x0B4 */ s32 mirror_weapon_packet;  /* its draw packets, 0x1A00 bytes */
    /* 0x0B8 */ s32 aim_target;         /* player: the entity auto-aim locked on (player_find_aim_target; the tyrant sets itself) */
    /* 0x0BC */ s8 death_timer;
    /* 0x0BD */ s8 animation_id;
    /* 0x0BE */ u8 animation_frame_id;
    /* 0x0BF */ s8 timing_control;
    /* 0x0C0 */ u8 pad0C0;
    /* 0x0C1 */ s8 unused_0C1;  /* only cleared at init, never read */
    /* 0x0C2 */ s16 move_speed_current;
    /* 0x0C4 */ u16 action_ticks_counter;
    /* 0x0C6 */ u16 anim_base_x;
    /* 0x0C8 */ u16 anim_base_z;
    /* 0x0CA */ u16 scale;  /* model scale, 0x1000 = 1.0, 0 = off (adder 3x, black tiger 1.7x, small Neptune 0.5x, plant 42) */
    /* 0x0CC */ s32 scd_pos_x;
    /* 0x0D0 */ s32 scd_pos_y;
    /* 0x0D4 */ s32 scd_pos_z;
    /* 0x0D8 */ s8 look_at_flags;
    /* 0x0D9 */ s8 look_at_yaw_step;
    /* 0x0DA */ s8 look_at_pitch_step;
    /* 0x0DB */ u8 scd_anim_param;
    /* 0x0DC */ u8 collision_flags;      /* 0x10: passes through boundaries of type 5 (set for the player at init);
                                            0x80 (player): an action is running (check / climb / push / door; set when
                                            it starts, cleared with &= 0x7F when it ends; scripted walks wait for it);
                                            player also: 0x02 poisoned (web spinner/adder/wasp; blue-herb-type cure),
                                            0x20 Yawn's poison (serum-type cure, also clears story flag 0x43),
                                            0x40 in poison gas: -2 HP every 7 frames, can kill; set by the gas rooms'
                                            scripts (SCD op 0x33 sub 8 `33 08 40 01`: ROOM2050/7050 statue-and-vents
                                            puzzle, ROOM4090 chemical mixing, ROOM5070 lab), cleared on room entry;
                                            see g_PlayerWork.poison_timer */
    /* 0x0DD */ s8 look_at_joint_idx;
    /* 0x0DE */ u16 scd_timer;
    /* 0x0E0 */ u16 scd_entity_flags;
    /* 0x0E2 */ u16 next_turn_timer;
    /* 0x0E4 */ Billboard shadow;         /* the round drop shadow; the death code turns it into the blood pool */
    /* 0x15C */ s32 pool_buffer;          /* enemies: the entity's allocation in the load pool (zombie: 2 extra billboards); PC sca_data_ptr. The player uses 0x15C.. as g_PlayerWork */
    /* 0x160 */ u8 pad160;
    /* 0x161 */ u8 slot_info;               /* bits 0-3: enemy slot, bits 4-6: sound set (group of 10 room sounds), bit 7: not saved on room exit */
    /* 0x162 */ u8 pad162;
    /* 0x163 */ u8 death_event_id;
    /* 0x164 */ u8 sight_check;  /* bits 0-4: frame counter (frames 0-3 test line of sight to the player, one room quarter each),
                                           bit 5: sight blocked; seeing the player sets the waypoint to the player */
    /* 0x165 */ u8 pad165;
    /* 0x166 */ u16 waypoint_x;
    /* 0x168 */ u16 waypoint_z;
    /* 0x16A */ u8 pad16A;
    /* 0x16B */ s8 dc_double_step;       /* Director's Cut "fast zombie" (PC decomp) */
    /* 0x16C */ u8 sub16C[0x20];         /* enemy work area: each enemy's own struct, see ENEMY_WORK */
} Entity;

/* Sizes the code writes as numbers when it steps through arrays (a sizeof() would be unsigned and can
   compile differently): */
#define ENTITY_SIZE 0x18C   /* one Entity */
#define JOINT_SIZE  0x7C    /* one skeleton joint (Joint, config/locals/joint.txt) */
#define EFFECT_SIZE 0x84    /* one Effect slot of g_EffectPool */

/* Enemy work area: entity bytes 0x16C.. mean something different for every enemy, so each enemy file
   defines its own struct W (ZombieWork in src/shared/zombie.c, ...; tools/enemywork.py) and short
   macros (ZOMBIE(e)->f, ZOMBIE_FIELD(e, f)). Capcom's code reached these bytes in two ways that
   GCC 2.6.3 -O0 compiles differently, so three spellings exist:
     ENEMY_WORK(e, W)->f    base pointer e+0x16C, field offset in the load/store (`addu 364; lh 10(..)`)
     ENEMY_FIELD(e, W, f)   the field's full address first, then the load/store (`addu 374; lh 0(..)`)
     ENEMY_MEMBER(e, W, f)  a member of the entity itself (`lh 374(..)`)
   A field's address is written &ENEMY_WORK(e, W)->f (same code as (u8 *)e + 0x16C + offset).
   The _AS forms read/write the field with another type T (the original code did, e.g. u16 for s16;
   used where a cast compiled differently). (int)&((W *)0)->f is the field offset (no offsetof here). */
/* The player uses entity bytes 0x15C.. for its own data (the enemies use 0x16C.. as their work area):
   g_PlayerWork names those bytes of g_PlayerEntity (0x800C5124 + 0x15C). A macro over g_PlayerEntity,
   not a second linker symbol: the symbol at 0x800C5280 (g_PlayerWeaponAnimHeader) isn't defined in programs whose original code never used it,
   and the macro compiles to the same code as the old separate symbols (-O0 and -O2 tested).
   Names from the code (tools/playersyms.py) and the RE1 PC decomp's PlayerEntity. */
typedef struct PlayerWork {
    /* 0x15C */ s32 weapon_anim_header;  /* animation set of the equipped weapon (load_equipped_weapon_animation) */
    /* 0x160 */ s32 weapon_anim_base;
    /* 0x164 */ s32 action_anim_header;  /* ladder / push / door-open animations */
    /* 0x168 */ s32 action_anim_base;
    /* 0x16C */ s32 grab_anim_header;    /* player animations stored in the enemy's EMD (grabs, bites, kills; load_entity_emd) */
    /* 0x170 */ s32 grab_anim_base;
    /* 0x174 */ u8 poison_timer;         /* frames to the next poison damage (-2 HP; 120, or 7 when severely poisoned) */
    /* 0x175 */ u8 max_health;           /* by character (initialize_game; Chris 140) */
    /* 0x176 */ u8 aim_flags;            /* aim direction: 0x20 up, 0x40 neutral, 0x80 down */
    /* 0x177 */ u8 pad177;
    /* 0x178 */ u16 aim_state;           /* result of player_find_aim_target (0 = nothing in range) */
} PlayerWork;
#define g_PlayerWork (*(PlayerWork *)((u8 *)&g_PlayerEntity + 0x15C))   /* g_PlayerEntity: see globals.h */

/* A field at a byte offset from a pointer, in the two spellings GCC 2.6.3 compiles differently at -O0:
     OFFSET_MEMBER(p, T *, off)  a real (anonymous) struct member: `sb $v1,0x85($v0)`
     OFFSET_DEREF(p, T *, off)   pointer arithmetic: `addiu; move; sb ..,0(..)` (array-element style)
   (from m2c's M2C_SFIELD / M2C_PFIELD; the macros below are built on them). */
#define OFFSET_MEMBER(expr, type_ptr, offset) (((struct { char pad_[(offset) > 0 ? (offset) : 0]; __typeof__(*(type_ptr)0) f_; } *)((offset) < 0 ? (void *)((s8 *)(expr) + (offset)) : (void *)(expr)))->f_)
#define OFFSET_DEREF(expr, type_ptr, offset) (*(type_ptr)((s8 *)(expr) + (offset)))

#define ENEMY_WORK(e, W)            ((W *)((u8 *)(e) + 0x16C))
#define ENEMY_WORK_AS(e, W, f, T)   OFFSET_MEMBER((u8 *)(e) + 0x16C, T *, (int)&((W *)0)->f)
#define ENEMY_FIELD(e, W, f)        (*(__typeof__(((W *)0)->f) *)((s8 *)(e) + (0x16C + (int)&((W *)0)->f)))
#define ENEMY_FIELD_AS(e, W, f, T)  (*(T *)((s8 *)(e) + (0x16C + (int)&((W *)0)->f)))
#define ENEMY_MEMBER(e, W, f)       OFFSET_MEMBER(e, __typeof__(((W *)0)->f) *, 0x16C + (int)&((W *)0)->f)
#define ENEMY_MEMBER_AS(e, W, f, T) OFFSET_MEMBER(e, T *, 0x16C + (int)&((W *)0)->f)

/* The original code also reads/writes Entity's state bytes as one value (pointer-arithmetic form,
   exactly as compiled - a struct member would compile differently at -O0):
     0x84-0x87 state, sub_state, action_behavior, action_state as one s32 (set together),
     0x86-0x87 action_behavior + action_state as one 16-bit value. */
#define ENTITY_STATE_WORD(e)  OFFSET_DEREF(e, s32 *, 0x84)
#define ENTITY_ACTION_S16(e)  OFFSET_DEREF(e, s16 *, 0x86)
#define ENTITY_ACTION_U16(e)  OFFSET_DEREF(e, u16 *, 0x86)

/* A named Entity field read/written with another type, or in pointer-arithmetic form - exactly as the
   original compiled (tools/entityas.py; the plain `e->f` / a cast compile differently at -O0 there):
     ENTITY_AS(e, f, T)        member form with type T        (OFFSET_MEMBER)
     ENTITY_FIELD(e, f)        pointer-arithmetic form         (was OFFSET_DEREF(e, typeof f *, offset))
     ENTITY_FIELD_AS(e, f, T)  pointer-arithmetic form, type T (was OFFSET_DEREF(e, T *, offset))
   (int)&((Entity *)0)->f is the field's offset. */
#define ENTITY_OFF(f)               ((int)&((Entity *)0)->f)
#define ENTITY_AS(e, f, T)          OFFSET_MEMBER(e, T *, ENTITY_OFF(f))
#define ENTITY_FIELD(e, f)          (*(__typeof__(((Entity *)0)->f) *)((s8 *)(e) + ENTITY_OFF(f)))
#define ENTITY_FIELD_AS(e, f, T)    (*(T *)((s8 *)(e) + ENTITY_OFF(f)))

/* Effect: the 0x84-byte effect/billboard records of g_EffectPool at the literal address 0x800C3024
   (64 slots; field names from the RE1 PC decomp's struct Effect where clear; main program), indexed by the byte at
   0x800CF86E: written as ((Effect *)0x800C3024)[*(u8 *)0x800CF86E].field - the literal base keeps
   the constant in a register for field 0 (effect_behavior_gravity_rise, effect_behavior_flash_marker).
   Layout and names from the RE1 PC decomp's struct Effect (names only); types = what the PS1 code uses.
   The current slot is EFFECT (re1_views.h). */
typedef struct Effect {
    /* 0x00 */ u8 pre_behavior;               /* g_EffectBehaviorTable index run before the move step (0 = free slot) */
    /* 0x01 */ u8 post_behavior;             /* g_EffectBehaviorTable index run after the move step */
    /* 0x02 */ u8 type;                  /* 1 = re-copy the parent matrix every frame (follows it), 2 = keep the copy */
    /* 0x03 */ u8 scale;                 /* sprite scale (create_billboard's last argument; fades shrink it) */
    /* 0x04 */ u8 hdr04;                 /* 0x04..0x15: rest of the animation header (copied from frame data) */
    /* 0x05 */ u8 hdr05;
    /* 0x06 */ u8 hdr06;
    /* 0x07 */ u8 hdr07;
    /* 0x08 */ u16 accel_x;               /* acceleration, added to vel_* each frame */
    /* 0x0A */ u16 accel_y;
    /* 0x0C */ u16 accel_z;
    /* 0x0E */ u16 draw_flags;            /* bit0 animate frames, bit1 recompute position, bit2/3 y snapped to 0 /
                                             the player's floor; (>> 6) << 22 = GsSPRITE attribute: 0x4000 semi-
                                             transparent, 0x3000 blend rate, 0x8000 hidden */
    /* 0x10 */ u16 vel_x;                 /* velocity, added to disp_* each frame */
    /* 0x12 */ s16 vel_y;
    /* 0x14 */ u16 vel_z;
    /* 0x16 */ s16 yaw;                  /* yaw rotation angle */
    /* 0x18 */ s16 disp_x;                /* displacement (local position): rotated by yaw, + local_offset, then
                                             through the 0x30 matrix (PC: rot_speed - not a rotation speed) */
    /* 0x1A */ s16 disp_y;
    /* 0x1C */ s16 disp_z;
    /* 0x1E */ u8 frame_delay;           /* frame delay counter */
    /* 0x1F */ u8 frame_index;           /* current animation frame */
    /* 0x20 */ s16 pos_x;                /* world position */
    /* 0x22 */ s16 pos_y;
    /* 0x24 */ s16 pos_z;
    /* 0x26 */ u8 effect_type;           /* effect type id (spawn parameter) */
    /* 0x27 */ u8 depth_group;           /* depth group + flags (spawn parameter) */
    /* 0x28 */ s16 local_offset_x;       /* local offset (truncated spawn position) */
    /* 0x2A */ s16 local_offset_y;
    /* 0x2C */ s16 local_offset_z;
    /* 0x2E */ s16 depth_scaled;
    /* 0x30 */ MATRIX transform;          /* m: rotation (copied from parent_matrix); t (0x44..0x4F): the sprite's
                                             world offset */
    /* 0x50 */ s32 proj_depth;           /* projected depth */
    /* 0x54 */ s32 spawn_x;              /* spawn position (full precision) */
    /* 0x58 */ s32 spawn_y;
    /* 0x5C */ s32 spawn_z;
    /* 0x60 */ s32 spawn_w;
    /* 0x64 */ s32 parent_matrix;          /* MATRIX pointer or identity */
    /* 0x68 */ s32 clut_info;            /* CLUT texture header pointer */
    /* 0x6C */ s32 frame_list;            /* VRAM sprite info pointer */
    /* 0x70 */ s32 frame_cur;
    /* 0x74 */ s32 uv_list;              /* UV data pointer */
    /* 0x78 */ s32 uv_cur;
    /* 0x7C */ s32 anim_data_base;       /* animation frame data base */
    /* 0x80 */ s32 anim_data_frame;      /* current animation frame pointer */
} Effect;

/* Field f (read/written as type T) of the effect slot at byte offset `off` (= slot * EFFECT_SIZE), computed
   the way much of effect.c's original code did: the field's address in slot 0, plus the offset (compiles
   differently from LITP(Effect, g_EffectPool)[slot].f). EFFECT_CUR_OFF = the current slot's offset. */
#define EFFECT_AT(off, f, T)        (*(T *)(LITP(u8, g_EffectPool) + FIELD_OFF(Effect, f) + (off)))
#define EFFECT_CUR_OFF              (LIT(u8, g_ActiveEffectIndex) * EFFECT_SIZE)

/* Field of any struct W through a pointer p, exactly as the original compiled (tools/structview.py;
   ENTITY_* / ENEMY_* above are the same idea for Entity and the enemy work areas):
     MEMBER_AS(p, W, f, T)   member form, read/written as type T   (OFFSET_MEMBER)
     FIELD_OF(p, W, f)       pointer-arithmetic form               (OFFSET_DEREF)
     FIELD_OF_AS(p, W, f, T) pointer-arithmetic form with type T */
#define FIELD_OFF(W, f)             ((int)&((W *)0)->f)
#define MEMBER_AS(p, W, f, T)       OFFSET_MEMBER(p, T *, FIELD_OFF(W, f))
#define FIELD_OF(p, W, f)           (*(__typeof__(((W *)0)->f) *)((s8 *)(p) + FIELD_OFF(W, f)))
#define FIELD_OF_AS(p, W, f, T)     (*(T *)((s8 *)(p) + FIELD_OFF(W, f)))
/* The same with a member PATH relative to a W pointer, e.g. `->world.t[0]` or `[1].flags` (an element
   after the one p points at; tools/localview.py for local pointer variables such as joints): */
#define PATH_OFF(W, path)           ((int)&(((W *)0)path))
#define MEMBER_AT(p, W, path, T)    OFFSET_MEMBER(p, T *, PATH_OFF(W, path))
#define FIELD_AT(p, W, path, T)     (*(T *)((s8 *)(p) + PATH_OFF(W, path)))

/* g_MenuWork (0x800BC5A8): the status / inventory menu and item viewer work block. Many parts are also
   reached through their own globals (names in the comments). */
typedef struct MenuWork {
    /* 0x000 */ u8 mode;                 /* why the menu was opened: MENU_MODE_* */
    /* 0x001 */ u8 state;                /* g_MainMenuState */
    /* 0x002 */ u8 action_result;        /* g_MenuActionResult: the menu can't be closed while non-zero */
    /* 0x003 */ u8 flags;                /* g_MenuFlags: 0x41 forces the close, 0x80 = an item was used, 2 = used */
    /* 0x004 */ u8 *frames_ptr;          /* g_CurrentMenuFramesDataPtr */
    /* 0x008 */ u8 pad008;
    /* 0x009 */ u8 char_flags;           /* g_MenuCharacterFlags: bit 1 = the 8-slot inventory layout */
    /* 0x00A */ u8 slot_count;           /* g_TotalInventorySlots: 6 or 8 */
    /* 0x00B */ u8 item_id;              /* g_ItemMenuSelectedItemId */
    /* 0x00C */ s8 is_weapon;            /* status menu: item_id < 11 (EQUIP row); item box: g_ItemboxScrollDir */
    /* 0x00D */ s8 loaded_res_id;        /* g_MenuLoadedResourceId */
    /* 0x00E */ s8 model_loaded;         /* g_MenuModelLoaded: 0x80 = the player model was swapped out */
    /* 0x00F */ u8 has_radio;            /* g_MenuHasRadio: the RADIO tab is greyed while 0 */
    /* 0x010 */ u8 sub_state;            /* g_ItemboxMenuState */
    /* 0x011 */ u8 submenu_state;        /* g_ItemSubmenuState (EQUIP/USE, CHECK, COMBINE box) */
    /* 0x012 */ u8 action_state;         /* g_ItemActionMenuState */
    /* 0x013 */ u8 cursor;               /* g_MenuCursor: 8 = the first item slot */
    /* 0x014 */ u8 submenu_row;          /* g_ItemboxScroll, reused as the submenu row */
    /* 0x015 */ u8 pad015[2];
    /* 0x017 */ u8 submenu_timer;        /* g_MenuSubTimer[1]: box open/close size */
    /* 0x018 */ u8 move_anim[3];         /* g_ItemMoveAnim: item move animation (offset x, y) */
    /* 0x01B */ u8 pad01B[3];
    /* 0x01E */ s8 health_anim_state;    /* g_HealthBarAnimState */
    /* 0x01F */ u8 health_status;        /* g_HealthStatus */
    /* 0x020 */ u8 ekg_color[3];         /* g_EkgColorR.. */
    /* 0x023 */ u8 ekg_color2[3];        /* g_EkgColor2R.. */
    /* 0x026 */ s16 ekg_primary_x0;      /* g_EkgPrimaryX0 */
    /* 0x028 */ s16 ekg_primary_x1;
    /* 0x02A */ s16 ekg_secondary_x0;
    /* 0x02C */ s16 ekg_secondary_x1;
    /* 0x02E */ u8 pad02E[2];
    /* 0x030 */ s32 ekg_wave_primary;    /* g_EkgWavePrimary */
    /* 0x034 */ s32 ekg_wave_secondary;
    /* 0x038 */ u8 face_anim;            /* g_HealthBarFaceAnim */
    /* 0x039 */ u8 viewer_state;         /* g_ItemViewerState */
    /* 0x03A */ u8 viewer_action;        /* g_ItemViewerActionIndex */
    /* 0x03B */ u8 obj_count;            /* g_ItemViewerObjCount: objects of the item model */
    /* 0x03C */ s16 spin_input_x;        /* g_ViewerSpinInputX.. */
    /* 0x03E */ s16 spin_input_y;
    /* 0x040 */ s16 spin_input_z;
    /* 0x042 */ u8 pad042[2];
    /* 0x044 */ SVECTOR obj_rot[2];      /* g_ViewerObjRot; [1].vy = g_ViewerExamineSpin */
    /* 0x054 */ SVECTOR rot;             /* the model rotation (rot.vy = g_ViewerRotY) */
    /* 0x05C */ GsDOBJ5 obj[2];          /* the item model's objects */
    /* 0x084 */ GsCOORDINATE2 coord[3];  /* their coordinate systems */
    /* 0x174 */ u8 timer;                /* zoom in/out frames */
} MenuWork;

/* g_SelectedFmvId: index into g_MovieTable (ZMOVIE/*.STR; ids 3..14 = DM*.STR, chosen by room scripts) */
#define FMV_TITLE           0       /* OJ.STR: title intro (corridor, eye, RESIDENT EVIL logo) */
#define FMV_INTRO           1       /* PJ.STR: live-action intro + cast; for Jill the doors / main hall shot is
                                       skipped (Chris narrates it) */
#define FMV_SELF_DESTRUCT   2       /* DMF.STR: the explosion when the self-destruct countdown runs out (no skip) */
#define FMV_ENDING_BASE     0xE     /* DME.STR; + ending id 1..8 = ED1..ED8.STR */

/* g_MenuWork+0: why the menu was opened (PC DAT_00ae9f10 / MSF_MENU_MODE_*; status_menu.c `sel`, item.c `mode`) */
#define MENU_MODE_STATUS            0   /* START: status screen (its Examine uses the item viewer) */
#define MENU_MODE_KEY_USED_UP       1   /* a key item is used up: "drop it?" */
#define MENU_MODE_ITEMBOX           2
#define MENU_MODE_PICKUP            3   /* an item lying in the room: model + "Will you take it?" */
#define MENU_MODE_GOT_ITEM          4   /* given by a room script (SCD 0x2D; e.g. a crank handed back after use):
                                           model + message, taken without asking */
#define MENU_MODE_MAP               5   /* set by room scripts: the map display */
#define MENU_MODE_KEY_USED_UP_VIEW  6   /* mode 1 for item ids >= 0x33: the used-up item in the viewer (msg 0xC7) */

/* The view (GsRVIEW2 at +8) of camera record `cut` of the loaded room: the RDT's camera records (RdtCamera,
   0x2C bytes each) start at RDT +0x94. Main program (g_RoomData: src/main/globals.h). */
#define ROOM_CAMERA_VIEW(cut)       ((cut) * 0x2C + g_RoomData + 0x9C)
/* the same as a number (g_RoomData read as an s32; another order of the additions compiles differently) */
#define ROOM_CAMERA_VIEW_ADDR(cut)  (LIT(s32, g_RoomData) + (cut) * 0x2C + 0x9C)

/* One of the 3 room lights in the loaded room's header (RDT +0x0C + i * 0x14; the same bytes as RoomHeader's
   light_* fields, config/structs/room_header.txt). update_entity_lighting reads them; the computer-lab
   terminal saves / replaces them whole. */
typedef struct RoomLight {
    /* 0x00 */ GsF_LIGHT l;      /* position / direction and colour, as GsSetFlatLight takes it */
    /* 0x10 */ u16 type;         /* 0 = point light with radial falloff (update_entity_lighting), else directional */
    /* 0x12 */ u16 range;        /* falloff radius */
} RoomLight;                     /* 0x14 */

/* One row of art in a computer-lab terminal window (0x10 bytes; the g_ClRows* tables, a window's row_data):
   drawn width * 8 by height * 16 pixels (a character grid). Names from the PC decomp's ClRow. */
typedef struct ClRow {
    /* 0x00 */ u8 u;
    /* 0x01 */ u8 pad1[3];
    /* 0x04 */ u8 v;
    /* 0x05 */ u8 pad5[3];
    /* 0x08 */ s16 width;            /* 0 hides the row (the login window's password line) */
    /* 0x0A */ s16 height;
    /* 0x0C */ s16 tpage;
    /* 0x0E */ s16 pad0E;
} ClRow;

/* One of the 12 door-animation objects of g_DoorOrderTable (0x80 bytes; PC DoorOrderEntry), set up and moved
   by the door script's order_* commands (door.c) and drawn by door_anim_render. */
typedef struct DoorOrder {
    /* 0x00 */ u16 flags;            /* 0x8000 drawn, 0x4000 rotation -> matrix each frame, 0x2000 lit */
    /* 0x02 */ s16 id;
    /* 0x04 */ GsDOBJ5 dobj;         /* the model (dobj.tmd / dobj.packet: door_op_order_tpage) */
    /* 0x18 */ GsCOORDINATE2 coord;  /* coord.coord.t = position (door_op_order_pos) */
    /* 0x68 */ SVECTOR rot;          /* rotation (door_op_order_rot) */
    /* 0x70 */ s16 vel[3];           /* added to the position by door_op_order_pos_add_vel */
    /* 0x76 */ s16 rotvel[3];        /* added to the rotation by door_op_order_rot_add_vel */
    /* 0x7C */ u8 *vert;             /* model vertices (door_op_order_vert_add) */
} DoorOrder;

/* Named bits of the flag words (g_StatusFlags, g_GameStateFlags) and the pad words */
#include "re1_flags.h"
/* Structs reached through one pointer/global, with a short "view" macro each (generated from
   config/structs/*.txt by tools/structview.py) */
#include "re1_views.h"
/* Structs reached through local pointer variables (config/locals/*.txt, tools/localview.py) */
#include "re1_locals.h"
/* RAM addresses the code writes as numbers, with names: LIT(T, name), LITP(T, name), ADDR_name
   (config/addrs.txt, tools/litaddr.py) */
#include "re1_addrs.h"

#endif /* RE1_TYPES_H */
