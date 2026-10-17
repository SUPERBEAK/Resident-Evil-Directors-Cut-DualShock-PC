/* game.c - Game start and the main game loop (game_loop): pause / menus, room transitions, death and game
   over, the self-destruct countdown; lighting, loading helpers and the die screen. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void voice_command(s32 cmd, s32 clip);
s32 cd_read_file(s32 file, u32 *dst);
s32 cd_verify_file_checksum(void);
void room_setup_camera(void);
void load_room_bg_image(void);
void check_camera_switch(s32 refresh);
void display_room_camera_bg(void);
void door_anim_task(void);
void load_shoot_direction_data(void);
void update_2d_effects(void);
void update_entities(void);
void build_enemy_snap(void);
void room_events_check(void);
void fade_update(void);
void set_fading(s8 fade_type, s16 step);


void game_start(void) {
    void initialize_game(void);
    s32 game_loop_a1() __asm__("game_loop");
    void reset_clear_color(void);
    s32 pad_vibration_reset();
    int VSync(int mode);
    s32 loop_ret;
    s32 status;

    reset_clear_color();
    LIT(u16, g_UpdateFlags) &= 0xFDFF;
    sound_init();
    initialize_game();
    loop_ret = game_loop_a1();
    pad_vibration_reset();
    g_GameStateFlags = 0;
    status = LIT(s32, g_StatusFlags);
    if (status & STATUS_ATTRACT_DEMO) {
        LIT(s32, g_StatusFlags) = status & 0x20080000;
        load_and_schedule_overlay(0x15E);
    }
    if (loop_ret == 0) {
        LIT(s32, g_PlayTime) = (VSync(-1) - g_PlayTimeVSync) + LIT(s32, g_PlayTime);
        load_and_schedule_overlay(0x153);
    }
    LIT(s32, g_StatusFlags) &= 0x20080000;
    load_and_schedule_overlay(0x154);
}

void initialize_game(void) {
    void task_sleep();
    s32 load_shoot_direction_data_a1() __asm__("load_shoot_direction_data");
    void load_attract_mode_player_data();
    void init_player_data(void);
    void load_file_blocking();
    s32 task_execute();
    s32 pad_vibration_reset();
    s32 setup_character_data();
    s32 sound_set_stereo();
    s32 stage_init();
    s32 vram_clear_rect();
    void display_game_loading_message(void);
  s32 *pool_next;
  s32 is_jill;
  s32 status;
  s32 is_jill2;
  u16 hp3;
  u16 hp;
  u16 hp2;
  int flag_mask;
  pad_vibration_reset();
  vram_clear_rect(0, 0, 0x140, 0x1E0);
  LIT(s32, g_GameStateFlags) = ((LIT(s32, g_GameStateFlags)) & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
  task_sleep(1);
  g_VSyncMode = 2;
  flag_mask = 0xD4E900FF;
  LIT(s32, g_GameStateFlags) = (((LIT(s32, g_GameStateFlags)) & flag_mask) & (~0xF)) | GAME_DOOR_ANIM;
  mem_clear_words(ADDR_g_DefaultItemSlot, ADDR_g_SaveData);
  g_SpecialRoomLightDelta = 0;
  g_FadeStep = 0;
  LIT(s32, g_LoadPointer) = ADDR_g_OverlayArea;
  task_execute(1, &display_game_loading_message);
  load_file_blocking(1, LIT(s32, g_LoadPointer));
  if ((LIT(s32, g_GameStateFlags)) & GAME_LOADED_SAVE)
  {
    mem_copy(ADDR_g_SaveData, LIT(s32, g_LoadPointer), 0x200);
    sound_set_stereo((((u8) g_KeyConfigType) >> 7) ^ 1);
    g_PlayerEntity.position_vx = g_SavePlayerX;
    g_PlayerEntity.position_vz = g_SavePlayerZ;
    g_PlayerEntity.angle = g_SavePlayerAngle;
    g_PlayerEntity.collision_flags = g_SavePlayerCollisionFlags;
    g_PlayerEntity.health = g_SavedHealth;
    g_PlayerEntity.id = g_SavePlayerId;
    LIT(u8, g_CharacterModelId) = g_SavePlayerId;
    if (Flg_ck(ADDR_g_ScenarioFlags, 0x2A) != 0)
    {
      LIT(u8, g_CharacterModelId) += 8;
    }
    if ((Flg_ck(ADDR_g_ScenarioFlags, 0x6F) != 0) || (Flg_ck(ADDR_g_ScenarioFlags, 0x70) != 0))
    {
      LIT(u8, g_CharacterModelId) += 8;
    }
    LIT(u8, g_SaveCount) += 1;
  }
  else
  {
    g_PlayerEntity.collision_flags = 0x10;
    g_PlayerEntity.id = g_SavePlayerId;
    mem_copy(ADDR_g_SaveData, LIT(s32, g_LoadPointer), 0x41C);
    g_CharacterModelId = g_PlayerEntity.id;
    if (g_StatusFlags & STATUS_ATTRACT_DEMO)
    {
      load_attract_mode_player_data();
    }
    else
    {
      init_player_data();
    }
    is_jill = g_PlayerEntity.id & 1;
    hp = 0x8C - ((long long) (is_jill * 0x2C));
    g_PlayerEntity.health = hp;
    LIT(u16, g_SavedHealth) = hp;
    if ((LIT(s32, g_StatusFlags)) & STATUS_TRAINING)
    {
      hp2 = 0xB4 - (is_jill * 0x1E);
      g_PlayerEntity.health = hp2;
      LIT(u16, g_SavedHealth) = hp2;
      LIT(s8, g_SaveDcMode) = 1;
      g_ItemBoxSlots = 0x1E0B1E0B;
    }
    status = LIT(s32, g_StatusFlags);
    if (status & STATUS_ADVANCED)
    {
      LIT(s8, g_SaveDcMode) = 2;
      hp3 = 0x64 - ((g_PlayerId & 1) * 0x1E);
      g_PlayerEntity.health = hp3;
      LIT(u16, g_SavedHealth) = hp3;
      if (status & STATUS_ADVANCED_HOLD)
      {
        LIT(s8, g_SaveDcMode) = 3;
        g_ItemBoxSlots = 0x1E0B1E0B;
      }
    }
  }
  g_DeadMoveValue = &g_IdentityMatrix;
  g_pWorldScreenMatrix = (s32 *)&g_WorldScreenMatrix;
  g_pLightMatrix = &g_LightMatrix;
  g_UnusedInitWord = 0;
  g_UsedItemId = 0;
  g_PickedItemId = 0;
  g_DefaultItemSlotCount = 0;
  g_DefaultItemSlot = 0;
  g_ItemSlotsPointer = ADDR_g_InventorySlots;
  g_DeathState = 0;
  load_held_items_images(ADDR_g_DoorFile);
  g_RoomActionTail = (u32)&g_RoomActionTable;
  g_ScaHitPoolPtr = (s32)&g_ScaHitPool;
  is_jill2 = g_PlayerEntity.id & 1;
  g_PlayerWork.max_health = (-0x74) - (is_jill2 * 0x2C);
  if (g_StatusFlags & STATUS_TRAINING)
  {
    g_PlayerWork.max_health = (-0x4C) - (is_jill2 * 0x1E);
  }
  if (g_StatusFlags & STATUS_ADVANCED)
  {
    g_PlayerWork.max_health = 0x64 - (is_jill2 * 0x1E);
  }
  pool_next = (s32 *)((&g_ScaHitPool) + 6);
  g_PlayerScaHitData = (s32 *)&g_ScaHitPool;
  g_ScaHitPoolPtr = (s32)pool_next;
  g_ScaHitPoolBase = pool_next;
  g_PlayerEntity.sca_info = g_DefaultScaInfo;
  task_sleep(1, g_StatusFlags, &g_ScaHitPool);
  setup_character_data();
  LIT(s32, g_LoadPointer) = ADDR_g_CoreEspBuf;
  load_shoot_direction_data_a1();
  LIT(s32, g_LoadPointer) = ADDR_g_OverlayArea;
  g_SndFadeActive = 0;
  g_BgmState = 0xFF;
  sound_load_bank0(0);
  sound_load_bank3(g_PlayerId & 1);
  sound_load_bank1(g_PlayerEntity.behavior_flags, ADDR_g_OverlayArea);
  LIT(s32, g_GameStateFlags) = (s32) ((LIT(s32, g_GameStateFlags)) & ~GAME_DOOR_ANIM);
  stage_init();
}

/* The in-game main loop, one pass per frame: room scripts, camera, pause / menu requests (and the room change
   behind a door), entity / player / room object updates and rendering, effects, the countdown, death.
   Returns when the game ends (die screen or demo end). */
s32 game_loop(void) {
    int VSync(int mode);
    int rand(void);
    void srand(unsigned int seed);
    int sprintf();
    void task_sleep();
    void attract_demo_watchdog(void);
    void room_transition_load(void);
    void check_menus_state(void);
    s32 Flg_ck(s32 *, u32);
    void Flg_off(s32, u32);
    s32 check_desk_state(void);
    void check_itembox_state(void);
    s32 check_typewriter_state(void);
    volatile unsigned int check_event_item_usage(void);
    void print_text_8x14(s32, s32, s32, u8);
    void scd_run_command_functions(u8 *);
    void room_state_reset(void);
    void check_and_display_interactive_screen(void);
    void update_player_anim(void);
    s32 check_room_actions(s32, s32);
    s32 update_room_objects(void);
    void render_room_objects(void);
    void entity_compute_joint_world_matrices(s32);
    void entity_apply_look_at_rotation(void);
    void render_entity(Entity *);
    void draw_room_spr(void);
    void timeout_death_fade_out(void);
    void die_state(void);
    u16 cnt;
    u16 save;
    u16 t;
    u16 n;
    u32 m;

    cnt = 0;
    while (LIT(u8, g_MessageStatus) & 0x80) {
        task_sleep(1);
    }
    LIT(s32, g_PlayTimeVSync) = VSync(-1);
    LIT(u16, g_MenuMessageFlagsBackup) = 0xFFFF;
    LIT(u32, g_GameStateFlags) |= GAME_ROOM_CHANGE;
    save = 0xFFFF;
    if (g_StatusFlags & STATUS_ATTRACT_DEMO) {
        srand(0);
    } else {
        srand(VSync(-1));
    }
    LIT(u16, g_RoomSpriteHideMask) = 0;
    goto start;
    for (;;) {
        attract_demo_watchdog();
        switch (LIT(u8, g_OpenMenuFlag)) {
        case 1:
            if (LIT(u8, g_MessageStatus) & 0x80) {
                break;
            }
            if (g_GameStateFlags & GAME_ROOM_CHANGE) {
                set_fading(2, 0x600);
                save = 0xFFFF;
            } else {
                save = LIT(u16, g_Fade2Level);
                LIT(u8, g_SoftResetCounter) = 0;
                set_fading(2, 0xC00);
            }
            LIT(s16, g_Fade2Level) = -1;
            LIT(u32, g_GameStateFlags) = (LIT(u32, g_GameStateFlags) & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
            if (LIT(u32, g_GameStateFlags) & GAME_ROOM_CHANGE) {
                room_transition_load();
                LIT(u16, g_MenuMessageFlagsBackup) = 0xFFFF;
            } else {
                check_menus_state();
            }
        start:
            g_FadeTypeId = 2;
            LIT(u32, g_GameStateFlags) = (LIT(u32, g_GameStateFlags) & ~GAME_SCREEN_MODE) | GAME_SCREEN_BG;
            if (Flg_ck(LITP(s32, g_ScenarioFlags), 0x7D) != 0) {
                LIT(s16, g_FadeStep) = -0xA3;
            } else {
                LIT(s16, g_FadeStep) = -0x1800;
            }
            fade_update();
            LIT(u16, g_Fade2Level) = save;
            task_sleep(1);
            if (g_GameStateFlags & GAME_ROOM_CHANGE) {
                LIT(u16, g_UpdateFlags) = 0xFD3F;
            } else {
                LIT(u16, g_UpdateFlags) = 1;
            }
            do {
                LIT(u16, g_PadPressed) = 0;
                LIT(u16, g_PadHeld) = 0;
                g_OpenMenuFlag = 2;
                LIT(u32, g_GameStateFlags) &= 0xF5FFFFFF;
            } while (0);
            break;
        case 2:
            LIT(u16, g_UpdateFlags) = 0;
            LIT(u8, g_OpenMenuFlag) = 3;
            break;
        case 3:
            if (LIT(s16, g_FadeLevel) <= 0) {
                LIT(u8, g_OpenMenuFlag) = 0;
                t = LIT(u16, g_MenuMessageFlagsBackup);
                LIT(volatile u16, g_UpdateFlags) = t;
                LIT(u16, g_UpdateFlags) = t | 0x200;
                Flg_off(ADDR_g_ScenarioFlags, 0x7D);
            }
            break;
        }
        LIT(u16, g_RandSeed) = rand();
        LIT(u32, g_ItemUseFlags) = 0;
        LIT(u32, g_ItemUseFlags_1) = 0;
        LIT(u16, g_SavedHealth) = LIT(u16, g_PlayerHealth);
        check_desk_state();
        check_itembox_state();
        check_typewriter_state();
        check_event_item_usage();
        if (cnt >= 30 || LIT(u16, g_CountdownTimer) == 0x7FFF) {
            cnt = 0;
            if (LIT(u16, g_CountdownTimer) < 0x7FFE) {
                if (!(g_GameStateFlags & GAME_OVER)) {
                    LIT(u16, g_CountdownTimer)++;
                }
            } else {
                LIT(u16, g_CountdownTimer) = 0;
            }
        }
        if (LIT(u32, g_StatusFlags) & STATUS_SELF_DESTRUCT) {
            t = LIT(u16, g_CountdownTimer);
            if (t == 180) {
                m = LIT(u32, g_GameStateFlags) | GAME_OVER;
                LIT(u32, g_StatusFlags) &= ~STATUS_SELF_DESTRUCT;
                g_SelectedFmvId = FMV_SELF_DESTRUCT;
                g_PlayerEntity.status_flags = 0;
                LIT(u32, g_GameStateFlags) = m | GAME_FMV_REQUEST;
                LIT(s32, g_FmvBuffer) = LIT(s32, g_LoadPointer);
                goto check;
            }
            sprintf(LITP(char, sp_text), "%2d^%02d=%d%d", 2 - (u16)(t / 60), 59 - (u16)(t % 60),
                    9 - (u16)(cnt / 3), (cnt + 1) % 10);
            print_text_8x14(0x7C, 0x20, (LIT(u16, g_CountdownTimer) >= 120) ? 2 : 1, 0);
        }
        if (!(LIT(u32, g_GameStateFlags) & GAME_OVER)) {
            cnt++;
        }
        scd_run_command_functions(LIT(u8 *, g_RoomMainScd));
        room_events_check();
        room_state_reset();
        check_and_display_interactive_screen();
        if (LIT(u32, g_GameStateFlags) & GAME_CAMERA_REDRAW) {
            LIT(u32, g_GameStateFlags) &= ~GAME_CAMERA_REDRAW;
            display_room_camera_bg();
        } else {
            check_camera_switch(0);
        }
        if (LIT(u8, g_PlayerHitState) == 0 && (LIT(u16, g_UpdateFlags) & 0x100) && (LIT(u16, g_UpdateFlags) & 0x40)
            && !(LIT(u32, g_GameStateFlags) & GAME_MENU_OPEN)) {
            if (LIT(u8, g_PadBuf1) == 0xFF && !(LIT(u32, g_GameStateFlags) & GAME_MENU_REQUESTS)
                && !(g_StatusFlags & STATUS_ATTRACT_DEMO)) {
                LIT(u32, g_GameStateFlags) |= GAME_OPTIONS_REQUEST;
                goto pause;
            }
            if ((LIT(u16, g_PadHeldRaw) & (PSX_SELECT | PSX_START)) == 0x900 && !(LIT(u32, g_GameStateFlags) & GAME_MENU_REQUESTS)) {
                LIT(u32, g_GameStateFlags) |= GAME_OPTIONS_REQUEST;
                goto pause;
            }
            if ((LIT(u16, g_PadTrigger) & PSX_START) || (g_GameStateFlags & GAME_MENU_REQUESTS)) {
            pause:
                do {
                    LIT(u8, g_OpenMenuFlag) = 1;
                    LIT(u16, g_MenuMessageFlagsBackup) = LIT(u16, g_UpdateFlags);
                } while (0);
                LIT(u16, g_UpdateFlags) &= 0xFF7A;
            }
        }
        update_entities();
        if ((LIT(u8, g_PlayerHasEnterSwitchZone) & 0x20) || !(LIT(u16, g_UpdateFlags) & 0x100)) {
            LIT(u16, g_PadHeld) &= (PAD_CONFIRM | PAD_CANCEL);
            LIT(u16, g_PadPressed) &= (PAD_CONFIRM | PAD_CANCEL);
        }
        update_player_anim();
        LIT(u32, g_StatusFlags) &= ~STATUS_IN_WATER;
        check_room_actions(ADDR_g_PlayerEntity, 1);
        update_room_objects();
        render_room_objects();
        LIT(u8 *, g_CurrentEntity) = LITP(u8, g_EnemiesList);
        n = LIT(u8, g_EnemyCount);
        if (n != 0) {
            do {
                if (*LIT(u8 *, g_CurrentEntity) & 1) {
                    n -= 1;
                    entity_compute_joint_world_matrices(*(u16 *)(LIT(u8 *, g_CurrentEntity) + 0xCA));
                    entity_apply_look_at_rotation();
                    render_entity(LIT(Entity *, g_CurrentEntity));
                }
                LIT(u8 *, g_CurrentEntity) += ENTITY_SIZE;
            } while (n != 0);
        }
        LIT(u8 *, g_CurrentEntity) = LITP(u8, g_PlayerEntity);
        entity_compute_joint_world_matrices(LIT(u16, g_PlayerScale));
        entity_apply_look_at_rotation();
        render_entity(LITP(Entity, g_PlayerEntity));
        update_2d_effects();
        draw_room_spr();
        if (LIT(s16, g_PlayerHealth) < 0) {
            LIT(u32, g_GameStateFlags) |= GAME_OVER;
        }
    check:
        if (g_GameStateFlags & GAME_OVER) {
            switch (LIT(u8, g_DeathState)) {
            case 0:
                timeout_death_fade_out();
                LIT(u8, g_DeathState) = 1;
            case 1:
                if (LIT(s16, g_FadeLevel) < 0) {
                    die_state();
                    return (g_StatusFlags & ({ 0x80000000; })) == 0;
                }
                break;
            }
        }
        task_sleep(1);
    }
}

void timeout_death_fade_out(void) {
    void task_sleep();
    s32 scd_op_4b_bgm_stop_all();
    s16 unused1;
    s32 flags;
    s8 unused2;

    g_MessageStatus = 0;
    g_OpenMenuFlag = 0;
    flags = LIT(s32, g_GameStateFlags) & 0xD1FD003F;
    LIT(s32, g_GameStateFlags) = flags;
    LIT(u16, g_UpdateFlags) &= 0xFEFF;
    if (flags & GAME_FMV_REQUEST) {
        scd_op_4b_bgm_stop_all();
        voice_command(2, 0);
        task_sleep(1);
        g_FadeLevel = -1;
        return;
    }
    if (g_StatusFlags & STATUS_ATTRACT_DEMO) {
        g_FadeTypeId = 2;
        g_FadeStep = 0x800;
    } else {
        if (g_StatusFlags < 0) {
            g_FadeTypeId = 2;
        } else {
            g_FadeTypeId = 1;
        }
        g_FadeStep = 0x100;
    }
    fade_update();
}

void die_state(void) {
    void task_sleep();
    s32 display_die_screen_a1() __asm__("display_die_screen");
    s32 sound_shutdown();
    s32 vram_clear_rect();
    s32 flags;

    g_Fade2Level = -1;
    g_MessageStatus = 0;
    g_ScreenDimLevel = 0;
    flags = LIT(s32, g_GameStateFlags) & ~GAME_SCREEN_DIM;
    LIT(volatile s32, g_GameStateFlags) = flags;
    LIT(s32, g_GameStateFlags) = (flags & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
    if (!(g_StatusFlags & STATUS_ATTRACT_DEMO) && (g_StatusFlags >= 0)) {
        display_die_screen_a1();
    }
    task_sleep(1);
    vram_clear_rect(0, 0, 0x140, 0x1E0);
    if (g_SndFadeActive != 0) {
        do {
            task_sleep(1);
        } while (g_SndFadeActive != 0);
    }
    sound_shutdown();
    g_DemoTimer = 0;
    g_BgmState = 0xFF;
}

void room_transition_load(void) {
    void task_sleep();
    s32 load_room_bg_image_a3() __asm__("load_room_bg_image");
    s32 build_enemy_snap_a1() __asm__("build_enemy_snap");
    s32 task_execute();
    s32 pad_vibration_reset();
    s32 room_sprite_apply_hide_mask();
    s32 stage_init();
    s32 load_texture_from_table();
  s32 door_bits;
  s32 same_room;
  s32 silent;
  u8 door_flags;
  void *door;
  void *door2;
  door = LIT(void *, g_DoorRecord);
  g_DoorAnimId = *((u8 *) (((s8 *) door) + 8));
  g_DoorSndBank = *((u8 *) (((s8 *) door) + 9));
  g_DoorTexIndex = *((u8 *) (((s8 *) door) + 0xA));
  g_DoorAnimType = (*((u8 *) (((s8 *) door) + 0xB))) & 0x3F;
  g_DoorNextStageRoom = *((u8 *) (((s8 *) door) + 0xD));
  door_flags = *((u8 *) (((s8 *) door) + 0xB));
  pad_vibration_reset();
  g_SndPanVol[0][1] = 0x5F;
  g_SndPanVol[0][0] = 0x5F;
  g_SndPanVol[1][1] = 0x5F;
  g_SndPanVol[1][0] = 0x5F;
  g_SndPanVol[2][1] = 0x5F;
  g_SndPanVol[2][0] = 0x5F;
  load_texture_from_table();
  task_execute(1, &door_anim_task);
  task_sleep(1);
  sound_load_bank0(g_DoorSndBank);
  door2 = LIT(void *, g_DoorRecord);
  g_PlayerEntity.pos_x = (s32) (*((u16 *) (((s8 *) door2) + 0xE)));
  g_PlayerEntity.pos_y = (s32) (*((s16 *) (((s8 *) door2) + 0x10)));
  g_PlayerEntity.pos_z = (s32) (*((u16 *) (((s8 *) door2) + 0x12)));
  g_PlayerEntity.angle = *((u16 *) (((s8 *) door2) + 0x14));
  g_PlayerEntity.floor_y = (u16) (*((s16 *) (((s8 *) door2) + 0x10)));
  g_PlayerEntity.state = 0;
  g_PlayerEntity.position_vx = *((u16 *) (((s8 *) door2) + 0xE));
  door_bits = door_flags & 0xC0;
  g_PlayerEntity.position_vy = (u16) (*((s16 *) (((s8 *) door2) + 0x10)));
  g_PlayerEntity.position_vz = *((u16 *) (((s8 *) door2) + 0x12));
  if (!(door_flags & 0x80))
  {
    build_enemy_snap_a1();
    g_PrevRoomId = LIT(u8, g_RoomId);
    LIT(u8, g_RoomId) = g_DoorNextStageRoom & 0x1F;
    g_ScaHitPoolPtr = (s32)g_ScaHitPoolBase;
    if (((u8) g_DoorNextStageRoom) >= 0x20U)
    {
      LIT(u8, g_StageId) = (((u8) g_DoorNextStageRoom) >> 5) + 0xFF;
      if ((Flg_ck(ADDR_g_ScenarioFlags, 0) != 0) && (((u8) (LIT(u8, g_StageId))) < 2U))
      {
        LIT(u8, g_StageId) += 5;
      }
      stage_init();
    }
    else
    {
      room_init();
    }
  }
  same_room = door_bits & 0x80;
  if (g_GameStateFlags & GAME_DOOR_ANIM)
  {
    do
    {
      task_sleep(1);
      ;
    }
    while (g_GameStateFlags & GAME_DOOR_ANIM);
  }
  if ((door_bits & 0x80) == 0)
  {
    room_setup_camera();
    load_room_bg_image_a3();
    room_sprite_apply_hide_mask();
    silent = door_bits & 0x40;
  }
  else
  {
    check_camera_switch(1);
    silent = door_bits & 0x40;
  }
  if (silent == 0)
  {
    play_sfx(0, 1, 0);
  }
}

void check_menus_state(void) {
    s32 task_execute();
    void task_sleep();
    extern s32 options_menu;
    extern s32 main_menu;
    s32 *unused;

    task_sleep(1);
    if (g_GameStateFlags & GAME_OPTIONS_REQUEST) {
        task_execute(1, &options_menu);
    } else {
        task_execute(1, &main_menu);
    }
    LIT(s32, g_GameStateFlags) |= GAME_MENU_OPEN;
    task_sleep(1);
    LIT(s32, g_GameStateFlags) &= ~GAME_OPTIONS_REQUEST;
}

void reset_clear_color(void) {
    g_ClearColorB = 0;
    g_ClearColorG = 0;
    g_ClearColorR = 0;
}

void update_entity_lighting(s32 *pos) {
    int GsSetFlatLight(int id, GsF_LIGHT *lt);
    typedef struct {
        u8 pad[0xC];
        RoomLight light[3];
    } RoomLightsHeader;
    GsF_LIGHT lt;
    u32 i;
    s32 d;
    RoomLightsHeader *r;
    u16 range;

    for (i = 0; i < 3; i++) {
        r = LIT(RoomLightsHeader *, g_RoomData);
        if (r->light[i].type == 0) {
            lt.vx = pos[0] - r->light[i].l.vx;
            lt.vy = pos[1] - r->light[i].l.vy;
            lt.vz = pos[2] - r->light[i].l.vz;
            d = SquareRoot0(lt.vx * lt.vx + lt.vz * lt.vz);
            r = LIT(RoomLightsHeader *, g_RoomData);
            range = r->light[i].range;
            d = range - d;
            if (d < 0) {
                d = 0;
            }
            if (range != 0) {
                lt.r = r->light[i].l.r * d / range;
                lt.g = r->light[i].l.g * d / r->light[i].range;
                lt.b = r->light[i].l.b * d / r->light[i].range;
            } else {
                lt.r = lt.g = lt.b = 0;
            }
            GsSetFlatLight(i, &lt);
        } else {
            GsSetFlatLight(i, (GsF_LIGHT *)(i * 0x14 + (s32)r->light));
        }
    }
}

void load_attract_mode_player_data() {
    void load_file_blocking();
    extern s16 g_DemoSavedKeyConfigType_s16 __asm__("g_DemoSavedKeyConfigType");
    if ((u16) LIT(u16, g_AttractDemoIndex) >= 3U) {
        LIT(u16, g_AttractDemoIndex) = 0;
    }
    load_file_blocking(LIT(u16, g_AttractDemoIndex) + 0x14, LIT(u8 *, g_LoadPointer));
    mem_copy(&g_DemoData, LIT(u8 *, g_LoadPointer), 0x994);
    LIT(u16, g_AttractDemoIndex) += 1;
    g_DemoSavedKeyConfigType_s16 = (s16) LIT(u8, g_KeyConfigType);
    LIT(u8, g_KeyConfigType) = (LIT(u8, g_KeyConfigType) & 0xFC) | 1;
    g_PlayerEntity.id = g_DemoData_PlayerId;
    g_SavePlayerId = g_DemoData_PlayerId;
    g_CharacterModelId = g_DemoData_PlayerId;
    g_StageId = g_DemoData_StageId;
    g_RoomId = g_DemoData;
    g_PrevRoomId = g_DemoData_PrevRoomId;
    g_EquippedSlot = g_DemoData_EquippedSlot;
    g_TotalHeldItems = g_DemoData_HeldItems;
    mem_copy(LITP(u8, g_InventorySlots), (u8 *)&g_DemoData + 0x18, 0x18);
    g_DemoTimer = 1;
    g_PlayerEntity.position_vx = g_DemoData_PosX;
    g_PlayerEntity.position_vz = g_DemoData_PosZ;
    g_PlayerEntity.angle = g_DemoData_Angle;
    sound_load_bank1(g_PlayerEntity.behavior_flags, ADDR_g_OverlayArea);
}

void init_player_data(void) {
    s32 set_initial_items();
    set_initial_items();
    g_RoomId = 6;
    g_PlayerEntity.position_vx = 0x4268;
    g_PlayerEntity.position_vz = 0x1388;
    g_StageId = 0;
    g_PlayerEntity.angle = 0xC00;
    LIT(s32, g_StatusFlags) |= STATUS_NEW_GAME;
}

void attract_demo_watchdog(void) {
    s32 pad_get_raw_buttons(s32);
    s32 sound_fade_start(s32, s32);
    if (g_StatusFlags & STATUS_ATTRACT_DEMO) {
        if ((s32) g_DemoLength >= (g_DemoTimer + 0xA)) {
            if (((pad_get_raw_buttons(0) & 0xFFFF) && !(g_GameStateFlags & GAME_OVER)) || (g_GameStateFlags & GAME_OVER)) {
                goto block_6;
            }
        } else {
block_6:
            g_DemoTimer = 1;
            sound_fade_start(-3, 0x2B);
            LIT(s32, g_GameStateFlags) |= GAME_OVER;
            LIT(u16, g_UpdateFlags) &= 0xFE70;
            g_KeyConfigType = g_DemoSavedKeyConfigType;
        }
    }
}

void load_file(s32 a, s32 b) {
    s32 cd_read_file_a1(s32, s32) __asm__("cd_read_file");
    s32 cd_verify_file_checksum_a1() __asm__("cd_verify_file_checksum");
    void task_sleep(s32);
    extern u8 g_VSyncMode_u8 __asm__("g_VSyncMode");
    int VSync(int mode);
    int CdReadSync(int mode, u_char *result);
    s32 r;
    s32 n;

top:
    while (cd_read_file_a1(a, b) != 0) {
    }
    n = 0;
read:
    r = CdReadSync(1, NULL);
    if (g_VSyncMode_u8 == 0) {
        n += 1;
    } else {
        n += 2;
    }
    if (r == 0) {
        if (cd_verify_file_checksum_a1() != 0) {
            return;
        }
        goto top;
    }
    if (r == -1 || n >= 0x385) {
        goto top;
    }
    if (g_StatusFlags & STATUS_BLOCKING_LOAD) {
        VSync(0);
    } else {
        task_sleep(1);
    }
    goto read;
}

void load_file_blocking(void) {
    /* passes its own a0/a1 through unchanged */
    s32 load_file_passthru() __asm__("load_file");
    LIT(s32, g_StatusFlags) |= STATUS_BLOCKING_LOAD;
    load_file_passthru();
    LIT(s32, g_StatusFlags) &= ~STATUS_BLOCKING_LOAD;
}

void display_game_loading_message(void) {
    s32 task_exit();
    s32 msg_id;

    if (g_StatusFlags & STATUS_ATTRACT_DEMO) {
        msg_id = 0x5D;
    } else {
        msg_id = 0x5B;
        if (g_GameStateFlags & GAME_LOADED_SAVE) {
            msg_id = 0x5C;
        }
    }
    set_message_display(msg_id, 0);
    task_exit();
}

void display_die_screen(void) {
    void task_sleep();
    void render_entity(Entity *);
    void entity_compute_joint_world_matrices(s32);
    int GsSetRefView2(GsRVIEW2 *pv);
    void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
    void GsSortSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    void load_file_blocking_l(s32, s32) __asm__("load_file_blocking");
    void tim_load_to_vram(s32);
    void set_fading_l(s32, s32) __asm__("set_fading");
    void image_draw_wave_l(s16) __asm__("image_draw_wave");
    void sprite_load_template_l(u8 *, s32) __asm__("sprite_load_template");
    void entity_apply_look_at_rotation(void);
    u8 st[2];
    s16 fade;
    s16 cnt;
    u8 pri;
    u8 k;
    u8 *obj;

    load_file_blocking_l(8, ADDR_g_OverlayArea);
    g_VramTpageCursor = 0x1F06;
    tim_load_to_vram(ADDR_g_OverlayArea);
    g_PacketArea[0] = ADDR_g_OverlayArea;
    g_PacketArea[1] = ADDR_g_DiePacketBuffer1;
    LIT(s32, sp_box_attribute) = 0x60000000;
    LIT(s32, sp_box_x) = 0xFF88FF60;
    LIT(s32, sp_box_w) = 0xF00140;
    LIT(s32, sp_box_r) = 0;
    g_View.vpx = LITP(Entity, g_PlayerEntity)->pos_x + 0x1388;
    g_View_Vpy = LITP(Entity, g_PlayerEntity)->pos_y - 0x1B58;
    g_View_Vpz = LITP(Entity, g_PlayerEntity)->pos_z;
    g_View_Vrx = LITP(Entity, g_PlayerEntity)->pos_x;
    g_View_Vry = LITP(Entity, g_PlayerEntity)->pos_y - 0x3E8;
    g_View_Vrz = LITP(Entity, g_PlayerEntity)->pos_z;
    GsSetRefView2(&g_View);
    fade = 0;
    cnt = 0x3B;
    pri = 0xFF;
    st[1] = 0;
    st[0] = 0;
    while (1) {
        LIT(s16, sp_sprite_tpage) = 6;
        LIT(s32, sp_sprite_mx) = 0;
        LIT(s16, sp_sprite_scalex) = 0x1000;
        LIT(s32, sp_sprite_rotate) = 0;
        LIT(s32, sp_sprite_r) = 0x808080;
        switch (st[0]) {
        case 0:
            sound_fade_start(-3, 0x2B);
            g_FadeTypeId = 1;
            g_FadeStep = -0x180;
            fade_update();
            st[0] = 1;
        case 1:
            if (g_FadeLevel >= 0) {
                break;
            }
            st[0] = 2;
        case 2:
            if (fade < 0xFF) {
                LIT(u8, sp_box_r) = LIT(u8, sp_box_g) = LIT(u8, sp_box_b) = fade;
            }
            fade += 3;
            if (fade >= 0x40) {
                image_draw_wave_l(cnt);
                cnt--;
            }
            if (cnt < 4) {
                st[0] = 3;
            }
            break;
        case 3:
            image_draw_wave_l(cnt);
            st[1]++;
            if (st[1] >= 0x31) {
                st[0] = 4;
                st[1] = 0;
            }
            break;
        case 4:
            g_FadeTypeId = 2;
            g_FadeStep = 0x180;
            cnt = 4;
            fade_update();
            pri = 0xFF;
            st[0] = 5;
        case 5:
            image_draw_wave_l(cnt);
            cnt++;
        case 6:
            if (g_FadeLevel < 0) {
                goto end;
            }
            break;
        }
        if ((g_PadHeld & PAD_CONFIRM) && st[0] != 0) {
            g_FadeLevel = -1;
            set_fading_l(2, 0x800);
            goto end;
        }
        for (k = 0; k < 4; k++) {
            sprite_load_template_l(g_DieScreenImages + k * 16, 0x1000);
            GsSortSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OtBg) + g_ActiveBuffer, 1);
        }
        GsSortBoxFill(LITP(GsBOXF, sp_box), LITP(GsOT, g_OtBg) + LIT(u8, g_ActiveBuffer), 0);
        if ((s8)pri >= 0) {
            sprite_load_template_l(g_DieScreenTextImage, 0x1000);
            GsSortSprite(LITP(GsSPRITE, sp_sprite), LITP(GsOT, g_OrderingTables) + LIT(u8, g_ActiveBuffer), pri);
        }
        obj = LITP(u8, g_PlayerEntity);
        g_PlayerAngle += 8;
        task_sleep(1);
        if (*obj != 0) {
            g_CurrentEntity = (Entity *)((s32)obj);
            entity_compute_joint_world_matrices(g_PlayerEntity.scale);
            entity_apply_look_at_rotation();
            render_entity((Entity *)ADDR_g_PlayerEntity);
        }
    }
end:
    g_PacketArea[0] = ADDR_g_PacketBuffer0;
    g_PacketArea[1] = ADDR_g_PacketBuffer1;
}

void sprite_load_template(void *tpl, s16 scale_y) {
    u32 e;
    u16 t;

    LIT(s32, sp_sprite_attribute) = ((s32) SPR_TPL(tpl)->attribute);
    LIT(s32, sp_sprite_x) = FIELD_AT(tpl, SpriteTemplate, ->x, s32);
    LIT(s32, sp_sprite_w) = FIELD_AT(tpl, SpriteTemplate, ->w, s32);
    e = SPR_TPL(tpl)->clut_y;
    t = FIELD_AT(tpl, SpriteTemplate, ->u, u16);
    LIT(s16, sp_sprite_scaley) = scale_y;
    LIT(s32, sp_sprite_cx) = e << 0x10;
    LIT(u16, sp_sprite_u) = t;
}

void image_draw_wave(s32 t) {
    s32 sprite_load_template_p(s32 *, s32) __asm__("sprite_load_template");
    int rsin(int a);
    void GsSortSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    s32 scale;
    s32 height;
    s32 col;
    u32 wave;
    s32 y0;

    col = t * 8;
    g_WaveImage_H = 0x40;
    g_WaveImage_V = 0;
    g_WaveImage_W = 1;
    scale = ((s32) (col * col) / 16) + 0x1600;
    height = scale & 0xFFFF;
    if (height < 0) {
        height += 0x7F;
    }
    t *= t;
    y0 = -(height >> 7);
    col = 0;
    do {
        sprite_load_template_p(&g_WaveImage, scale & 0xFFFF);
        LIT(s8, sp_sprite_u) = col;
        LIT(s16, sp_sprite_x) = col - 0x80;
        wave = rsin(col << 8) * t;
        LIT(s16, sp_sprite_y) = y0 - (wave >> 0xD);
        GsSortSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)((g_ActiveBuffer * 0x14) + ADDR_g_OtMain), 0U);
        col += 1;
    } while (col < 0x100);
}
