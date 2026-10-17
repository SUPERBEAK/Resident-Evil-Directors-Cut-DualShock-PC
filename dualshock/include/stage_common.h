/* stage_common.h - declarations shared by the 7 stage overlays (STAGE1..7.EXE): the overlay API
   (overlay_api.h), the main-program globals the stage code reads, and the per-stage tables that
   stageN_entry installs. Each enemy's own data (sca info, health / anim tables, ...) is declared at
   the top of that enemy's source (src/shared/<enemy>.c or src/stageN/<enemy>.c). */
#ifndef STAGE_COMMON_H
#define STAGE_COMMON_H
#include "overlay_api.h"

/* The entity state word: Entity bytes 0x84..0x87 (state, sub_state, action_behavior, action_state)
   written or compared as one s32 (ENTITY_STATE_WORD). STATE4(1, 1, 3, 0) == 0x30101. */
#define STATE4(state, sub, behavior, action) ((action) << 24 | (behavior) << 16 | (sub) << 8 | (state))

/* Joint n of a joint array (Joint, JOINT_SIZE bytes each) written as the same pointer arithmetic
   Capcom's code did (one constant added): JOINT_N(joints, 2) == joints + 0xF8. The _ROT / _TRANSFORM /
   _WORLD / _POS forms give the address of that joint's rotation (0x04), transform matrix (0x24),
   world matrix (0x44) or world position (world.t, 0x58): JOINT_N_WORLD(joints, 2) == joints + 0x13C. */
#define JOINT_N(j, n)           ((j) + (n) * JOINT_SIZE)
#define JOINT_N_ROT(j, n)       ((j) + ((n) * JOINT_SIZE + 0x04))
#define JOINT_N_TRANSFORM(j, n) ((j) + ((n) * JOINT_SIZE + 0x24))
#define JOINT_N_WORLD(j, n)     ((j) + ((n) * JOINT_SIZE + 0x44))
#define JOINT_N_POS(j, n)       ((j) + ((n) * JOINT_SIZE + 0x58))

/* main-program globals (addresses: config/symbol_addrs.txt) */
extern s32 g_GameStateFlags;
extern s32 g_StatusFlags;
extern u8 g_ActiveBuffer;
extern u16 g_StageId;
/* g_StageId and the next byte g_RoomId read as one u16 = stage | room << 8 */
extern u16 g_StageRoom __asm__("g_StageId");
#define STAGE_ROOM(stage, room) ((room) << 8 | (stage))
extern u8 g_RoomId;
extern u8 g_CutId;
extern u8 g_PrevRoomId;
extern u16 g_RandSeed;
extern u16 g_UpdateFlags;
extern s16 g_FadeStep;
extern s8 g_FadeTypeId;
extern u16 g_PadTrigger;
extern void *g_CameraSwitchZones;
extern void *g_LoadPointer;
extern void *g_DeadMoveValue;
extern s32 *g_pLightMatrix;
extern s32 *g_pWorldScreenMatrix;
extern Entity *g_CurrentEntity;
extern Entity g_PlayerEntity __asm__("g_PlayerEntity");
/* enemy slot 0 (g_EnemiesList[0]) fields read directly by yawn / neptune / plant42 */
extern u8 g_EnemyCount;
extern s16 g_EnemySlot0_Angle;
extern s32 g_EnemySlot0_State;
extern s16 g_EnemySlot0_Health;
extern s32 g_EnemySlot0_Joints;

/* per-stage tables: defined in each stage overlay, installed by stageN_entry */
extern s32 *g_EnemyUpdateTable;
extern s32 *g_StageBgChecksumTable;
extern s32 *g_StageRoomBgmTable;
extern s32 *g_StageVoiceOffsetTable;
extern s32 enemies_update_functions_tbl;
extern s32 stage_bg_checksum_tbl;
extern s32 stage_bg_checksum_tbl_adv;
extern s32 stage_bgm_tbl;
extern s32 stage_voice_offset_tbl;

/* player animation slots a stage overrides with its enemies' grab / death animations
   (g_PlayerAnimFunctions is a main-program table: 3 windows of 19 entries, called by
   player_state_anim_window0/1/2 with the sub state) */
typedef void (*PlayerAnimFn)(void);
extern PlayerAnimFn g_PlayerAnimFunctions[3 * 19];

#endif
