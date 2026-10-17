/*
 * re1_flags.h - named bits of the game's flag words and pad words (main program).
 *
 * Plain numbers: each name expands to exactly the literal the code used before, so the build can't change.
 * Every bit the code uses has a name (2026-10-17); masks of several bits stay numbers where they only
 * reset groups (e.g. `&= ~0xF` at room load clears the low four bits of both words).
 */
#ifndef RE1_FLAGS_H
#define RE1_FLAGS_H

/* ---- g_StatusFlags (0x800C3004) --------------------------------------------------------------------- */
#define STATUS_IN_WATER          0x1         /* player stands in a water zone (room_action_water_zone sets it,
                                                game_loop clears it every frame): halves speed, splash effects */
#define STATUS_SCREEN_SHAKE      0x2         /* shake the screen (room scripts; boulder corridors) */
#define STATUS_SCREEN_BORDER     0x4         /* draw the room background with borders (options menu) */
#define STATUS_SHADOW_DEPTH_SORT 0x8         /* billboard_draw: shadows depth-sorted into the main OT (can be hidden
                                                behind scenery) instead of a fixed slot behind every model (room
                                                scripts set it; cleared at room load). E.g. the dining
                                                room, where the shadows go behind the dining table */
#define STATUS_BG_LOADING        0x10        /* load_room_bg_image is reading a camera background from the CD:
                                                the DualShock vibration update pauses */
#define STATUS_ADVANCED_HOLD     0x10000     /* save tag "AT": Advanced mode started holding Right (with ADVANCED) */
#define STATUS_ADVANCED          0x20000     /* save tag "Ad": Advanced (arrange) mode */
#define STATUS_TRAINING          0x40000     /* save tag "Tr": Training mode */
#define STATUS_SCRIPT_BIT19      0x80000     /* no code reads or writes it; kept by game_start / the ending with
                                                STATUS_NEW_GAME (mask 0x20080000), so room scripts may use it */
#define STATUS_BLOCKING_LOAD     0x100000    /* load_file_blocking: load_file waits with VSync instead of task_sleep */
#define STATUS_SFX_BANK1_SMALL   0x200000    /* sound bank 1 holds 16 sounds instead of 48 (memcard load, sfx bank 2) */
#define STATUS_STAIRS_ZONE       0x400000    /* set by the staircase zones (room_action_stairs_zone_x / _z, room
                                                action types 5 / 6) with has_enter_switch_zone 0x20: the player
                                                is turned to the stairs and walks the whole flight up / down in one
                                                animation (behaviour 0x11, player_climb_sequence: anim 0x33 / 0x35,
                                                floor height +-0xB45; +-0x57A in rooms 0x0E / 0x05). Staircases (seen in game): stage 2
                                                room 0x03 (2F main hall), 0x07 (2F far left, down to the west wing
                                                save room), 0x0E (2F right, near the first Yawn fight); stage 3
                                                room 0x05 (courtyard fountain, once opened); stage 6 room 0x10 (1F,
                                                down to the kitchen); + stage 7 / Advanced copies (tools/rdt_zones.py; room
                                                pictures: tools/bss_view.py; confirmed in game: hall + fountain) */
#define STATUS_DOOR_WAIT_BGM     0x800000    /* set when a door animation starts: the door script's sound op waits
                                                until update_room_bgm has set up the next room's music */
#define STATUS_BOX_CLIMB         0x1000000   /* set while climbing onto a box (climb step 0 with GAME_ON_BOX) */
#define STATUS_ROOM_SPRITES_OFF  0x2000000   /* draw_room_spr draws nothing */
#define STATUS_ALT_COSTUME       0x4000000   /* the player uses the alternate costume model (id 0x33) */
#define STATUS_SELF_DESTRUCT     0x8000000   /* the self-destruct countdown is running (game_loop) */
#define STATUS_ATTRACT_DEMO      0x10000000  /* attract-mode demo playback (title screen idle) */
#define STATUS_NEW_GAME          0x20000000  /* set by init_player_data (a new game); kept by the resets */
#define STATUS_GAME_CLEARED      0x80000000  /* the game is cleared (the final escape's script): game_loop's game-over
                                                exit then goes to the ending, without the death screen
                                                (in game: after the Tyrant the ending FMV, credits and results follow) */

/* ---- g_GameStateFlags (0x800C3000) ------------------------------------------------------------------ */
#define GAME_MIRROR_ON           0x1         /* this room has a mirror (SCD mirror_set writes bits 0-1) */
#define GAME_MIRROR_AXIS         0x2         /* mirror plane axis (passed as (flags & 2) >> 1) */
#define GAME_CAMERA_DEFER        0x4         /* a camera cut sets GAME_CAMERA_REDRAW (redraw next frame) instead of
                                                redrawing at once (room scripts; cleared at room load) */
#define GAME_SCRIPT_BIT3         0x8         /* set by room scripts only; no code reads it (cleared at room load) */
#define GAME_LADDER              0x10        /* set by a ladder zone (set_ladder_zone, room action type 12): the
                                                player climbs the ladder (behaviour 0xB, player_behavior_0b_ladder)
                                                instead of a staircase (0x11). Ladders: stage 3 room 0x01 (to the
                                                right after entering the guardhouse stage). Ladders that lead to
                                                another room are doors. Type 17 (slope_height_update) is a ramp:
                                                stage 4 room 0x0D */
#define GAME_CAMERA_REDRAW       0x20        /* redraw the camera background once (game_loop consumes it) */
#define GAME_PUSHING             0x40        /* the player has pushed a movable object (statue, crate) for 9 frames:
                                                behaviour 0x10 (push) until it clears */
#define GAME_ON_BOX              0x80        /* the player stands on a climbable box: only turning, no doors, box
                                                footstep sound */
#define GAME_FILE_PICKUP         0x100       /* open the menu on the document (file) pickup screen */
/* 0x200..0x2000: "open the menu in mode m" requests, bit = 0x4000 >> MENU_MODE_* (main_menu scans them) */
#define MENU_REQUEST(mode)       (0x4000 >> (mode))
/* (MENU_REQUEST(MENU_MODE_STATUS) = 0x4000 is also set by about 14 stage-3 room scripts - perhaps the radio
   calls; not confirmed whether the game opens the radio by itself) */
#define GAME_MENU_REQUESTS       0x7F00      /* any menu / message request pending */
#define GAME_MENU_OPEN           0x8000      /* the menu task is running */
#define GAME_MENU_BYTE           0xFF00      /* the whole menu byte (requests + open) */
#define GAME_SCREEN_DIM          0x10000     /* darken the screen (g_ScreenDimLevel ramps up while set) */
#define GAME_VOICE_PLAYING       0x20000     /* an XA voice line is playing */
#define GAME_FMV_REQUEST         0x40000     /* the main loop plays g_SelectedFmvId and clears it */
#define GAME_GRAPHICS_RESET      0x80000     /* the main loop calls init_graphics(1), which clears it (prologue,
                                                ending) */
#define GAME_CUT_LOCKED          0x100000    /* camera cut switching is disabled (room script) */
#define GAME_UNUSED_21           0x200000    /* never set or tested */
#define GAME_OPTIONS_REQUEST     0x400000    /* open the options menu instead of the status menu */
#define GAME_ALT_RDT             0x800000    /* load the room's alternate RDT file (file index + 1) */
#define GAME_OVER                0x1000000   /* the player died / the countdown ran out: game over fade */
#define GAME_ROOM_CHANGE         0x2000000   /* load a room at the next pause step (a door was entered; also set when game_loop starts) */
#define GAME_DOOR_ANIM           0x4000000   /* the door animation is running */
#define GAME_UNUSED_27           0x8000000   /* never set or tested (only cleared by masks) */
#define GAME_LOADED_SAVE         0x10000000  /* the game was started from a memory card save */
#define GAME_FADE_ACTIVE         0x20000000  /* set_fading started a screen fade: the main loop draws only the fade
                                                (task 0 suspended) and clears it when done; menus wait for it */
/* top 2 bits: what the main loop draws behind the frame */
#define GAME_SCREEN_CLEAR        0x40000000  /* clear to the clear colour */
#define GAME_SCREEN_BG           0x80000000  /* draw the room background image (the word is negative) */
#define GAME_SCREEN_MODE         0xC0000000  /* both bits */

/* ---- Game action bits: g_PadHeld / g_PadPressed / g_PadHeldPrev (pad_update maps the raw buttons
   through the key configuration) ------------------------------------------------------------------- */
#define PAD_FORWARD              0x1
#define PAD_RIGHT                0x2
#define PAD_BACK                 0x4
#define PAD_LEFT                 0x8
#define PAD_DIRECTIONS           0xF         /* all four */
#define PAD_AIM_UP               0x10        /* while aiming (d-pad up) */
#define PAD_AIM_DOWN             0x20        /* while aiming (d-pad down) */
#define PAD_FIRE                 0x40        /* fire while aiming */
#define PAD_ACTION               0x80        /* check / open / climb (same button as fire by default) */
#define PAD_AIM                  0x100       /* ready the weapon */
#define PAD_RUN                  0x200
#define PAD_CONFIRM              0x4000      /* menus: confirm */
#define PAD_CANCEL               0x8000      /* menus: cancel / back */

/* ---- Raw PSX controller bits: g_PadHeldRaw / g_PadTrigger / g_PadRaw32 / g_PadRawPressed32 (low half) */
#define PSX_L2                   0x1
#define PSX_R2                   0x2
#define PSX_L1                   0x4
#define PSX_R1                   0x8
#define PSX_TRIANGLE             0x10
#define PSX_CIRCLE               0x20
#define PSX_CROSS                0x40
#define PSX_SQUARE               0x80
#define PSX_SELECT               0x100
#define PSX_L3                   0x200
#define PSX_R3                   0x400
#define PSX_START                0x800
#define PSX_UP                   0x1000
#define PSX_RIGHT                0x2000
#define PSX_DOWN                 0x4000
#define PSX_LEFT                 0x8000
#define PSX_DPAD                 0xF000      /* all four directions */

/* ---- play_sfx(bank, sound) --------------------------------------------------------------------------- */
#define SFX_BANK_SYSTEM          3           /* sound bank 3 (g_SndBank3HedBuf): system sounds */
#define SE_MENU_MOVE             4           /* menu cursor moved */
#define SE_MENU_CANCEL           5           /* menu cancel / back */
#define SE_MENU_CONFIRM          6           /* menu confirm */

#endif /* RE1_FLAGS_H */
