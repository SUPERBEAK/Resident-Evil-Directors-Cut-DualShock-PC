/* overlay_api.h - the overlay API: function pointers the main program copies to a fixed
   table (g_OverlayApiTable 0x801FE900, task_scheduler_init) that overlays call through.
   Every call is written `(g_ApiCallPtr = api_X)(args)` (Capcom stored the pointer in
   g_ApiCallPtr before each call). The call's return type is g_ApiCallPtr's type (an
   assignment has the type of its left side), so one s32 type serves all of them; where Capcom's
   code read a result as another type (u32: unsigned % and compares), API_AS(T, api_X)(args).
   Addresses: config/symbol_addrs.txt. (2026-10-16: replaced ~2,300 per-function declarations.) */
#ifndef OVERLAY_API_H
#define OVERLAY_API_H

typedef s32 (*ApiFn)();

/* (g_ApiCallPtr = f) with the call result read as T */
#define API_AS(T, f) ((T (*)())(g_ApiCallPtr = (f)))

/* scenario flag ids (bits of g_ScenarioFlags, for api_Flg_on / api_Flg_ck) whose meaning the
   overlay code shows */
#define SCN_FLAG_PLANT42_DEAD 0x29  /* set when Plant 42 dies (plant42.c) */
#define SCN_FLAG_ADV_REWARD   0x7A  /* Advanced-mode clear reward (ending: item 5 in the new game) */
#define SCN_FLAG_CLEARED      0x7B  /* set by the ending when it restarts a cleared game */
#define SCN_FLAG_TIME_REWARD  0x7E  /* cleared under the time limit (ending: item 0xA in the new game) */

extern ApiFn g_ApiCallPtr;
extern ApiFn api_AddPrim;
extern ApiFn api_ApplyMatrixLV;
extern ApiFn api_ApplyMatrixSV;
extern ApiFn api_ClearImage;
extern ApiFn api_CompMatrix;
extern ApiFn api_DrawSync;
extern ApiFn api_Flg_ck;
extern ApiFn api_Flg_on;
extern ApiFn api_GetClut;
extern ApiFn api_GetTPage;
extern ApiFn api_GsGetActiveBuff;
extern ApiFn api_GsGetLs;
extern ApiFn api_GsInit3D;
extern ApiFn api_GsInitCoordinate2;
extern ApiFn api_GsInitGraph;
extern ApiFn api_GsInitGraph2;
extern ApiFn api_GsLinkObject5;
extern ApiFn api_GsMapModelingData;
extern ApiFn api_GsMulCoord0;
extern ApiFn api_GsMulCoord2;
extern ApiFn api_GsMulCoord3;
extern ApiFn api_GsPresetObject;
extern ApiFn api_GsPrstTG3L;
extern ApiFn api_GsPrstTG3NL;
extern ApiFn api_GsSetAmbient;
extern ApiFn api_GsSetFlatLight;
extern ApiFn api_GsSetLightMatrix;
extern ApiFn api_GsSetLightMode;
extern ApiFn api_GsSetLsMatrix;
extern ApiFn api_GsSetProjection;
extern ApiFn api_GsSetRefView2;
extern ApiFn api_GsSortBoxFill;
extern ApiFn api_GsSortFastSprite;
extern ApiFn api_GsSortFlipSprite;
extern ApiFn api_GsSortObject5;
extern ApiFn api_GsSortSprite;
extern ApiFn api_GsSwapDispBuff;
extern ApiFn api_LoadAverage12;
extern ApiFn api_LoadAverageShort12;
extern ApiFn api_LoadImage;
extern ApiFn api_MulMatrix;
extern ApiFn api_MulMatrix0;
extern ApiFn api_MulMatrix2;
extern ApiFn api_RotMatrix;
extern ApiFn api_RotMatrixY;
extern ApiFn api_RotTransPers4;
extern ApiFn api_ScaleMatrix;
extern ApiFn api_SetGeomScreen;
extern ApiFn api_SetLightMatrix;
extern ApiFn api_SetPolyFT4;
extern ApiFn api_SetRotMatrix;
extern ApiFn api_SetSemiTrans;
extern ApiFn api_SetTransMatrix;
extern ApiFn api_Square0;
extern ApiFn api_SquareRoot0;
extern ApiFn api_SsSetSerialAttr;
extern ApiFn api_StoreImage;
extern ApiFn api_TransMatrix;
extern ApiFn api_VSync;
extern ApiFn api_VectorNormal;
extern ApiFn api_billboard_adj_size;
extern ApiFn api_billboard_draw;
extern ApiFn api_billboard_init;
extern ApiFn api_billboard_reserve;
extern ApiFn api_billboard_set_color;
extern ApiFn api_billboard_set_size;
extern ApiFn api_catan;
extern ApiFn api_cd_set_mix_volume;
extern ApiFn api_cd_set_stereo;
extern ApiFn api_check_entity_collision;
extern ApiFn api_check_line_of_sight;
extern ApiFn api_check_room_actions;
extern ApiFn api_check_room_collision;
extern ApiFn api_check_room_collision_two_point;
extern ApiFn api_clut_fade_add_delta;
extern ApiFn api_clut_fade_set;
extern ApiFn api_effect_create_billboard;
extern ApiFn api_effect_free_matching;
extern ApiFn api_entity_add_speed_xz;
extern ApiFn api_entity_apply_anim_vertex;
extern ApiFn api_entity_ballistic_step;
extern ApiFn api_entity_check_alert_range;
extern ApiFn api_entity_check_view_and_distance;
extern ApiFn api_entity_check_visual_range;
extern ApiFn api_entity_compute_joint_world_matrices;
extern ApiFn api_entity_pathfind_update;
extern ApiFn api_entity_resolve_enemy_player_collisions;
extern ApiFn api_entity_rotate_collision_points;
extern ApiFn api_entity_rotate_toward_target;
extern ApiFn api_entity_swerve_around_obstacle;
extern ApiFn api_entity_update_look_at_angles;
extern ApiFn api_entity_update_wander_turn;
extern ApiFn api_entity_update_weapon_joint;
extern ApiFn api_fade_update;
extern ApiFn api_game_start;
extern ApiFn api_get_angle_towards_target;
extern ApiFn api_get_player_input_masked;
extern ApiFn api_is_facing_toward_entity;
extern ApiFn api_is_point_in_quad;
extern ApiFn api_joint_apply_color_tint;
extern ApiFn api_joint_check_player_in_range;
extern ApiFn api_joint_move;
extern ApiFn api_joint_setup_attack_effect;
extern ApiFn api_load_and_schedule_overlay;
extern ApiFn api_load_file;
extern ApiFn api_load_shoot_direction_data;
extern ApiFn api_matrix_relative;
extern ApiFn api_mem_copy;
extern ApiFn api_memcard_screen;
extern ApiFn api_memclr;
extern ApiFn api_options_menu;
extern ApiFn api_pad_get_raw_buttons;
extern ApiFn api_pad_vibrate_large;
extern ApiFn api_pad_vibrate_large_ramp;
extern ApiFn api_pad_vibrate_small;
extern ApiFn api_play_sfx;
extern ApiFn api_rand;
extern ApiFn api_rand_lfsr_next;
extern ApiFn api_reset_joint_transforms;
extern ApiFn api_room_collision_check_point;
extern ApiFn api_set_bg_image_buffer_params;
extern ApiFn api_set_fading;
extern ApiFn api_set_initial_items;
extern ApiFn api_snap_player_to_grab_position;
extern ApiFn api_sound_end;
extern ApiFn api_sound_load_bank1;
extern ApiFn api_sound_load_bank1_from_memory;
extern ApiFn api_sound_play_entity_se;
extern ApiFn api_sound_play_footstep;
extern ApiFn api_sound_play_se_3d;
extern ApiFn api_sound_reinit;
extern ApiFn api_sprintf;
extern ApiFn api_srand;
extern ApiFn api_str_close;
extern ApiFn api_str_decode_next_frame;
extern ApiFn api_str_get_frame;
extern ApiFn api_str_open;
extern ApiFn api_str_wait_decode;
extern ApiFn api_task_chain;
extern ApiFn api_task_exit;
extern ApiFn api_task_sleep;
extern ApiFn api_tim_load_to_vram;
extern ApiFn api_tmd_relocate_texture;
extern ApiFn api_turn_toward_target;
extern ApiFn api_tyrant_trail_alloc;
extern ApiFn api_tyrant_trail_push;
extern ApiFn api_update_2d_effects;
extern ApiFn api_update_entity_lighting;
extern ApiFn api_voice_command;
extern ApiFn api_entity_clone;
extern ApiFn api_web_spinner_update_webs;
extern ApiFn api_zone_path_find;

#endif
