/* npc.c - NPCs (partners and corpses): walk behaviours, path finding, look-at, footsteps and the per-character
   init functions. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void voice_command(s32 cmd, s32 clip);
s32 is_point_in_quad(s32 *pos, u16 *quad);
s8 effect_create_billboard(u8 type, u8 group, s32 yaw, s32 parent_mat, s32 offset, u8 scale);
void load_behavior_model(void);
void entity_apply_walk_speed(u16 speed);
s8 zone_path_find(s16 x, s16 z, s16 *out_x, s16 *out_z);
s16 zone_edge_heading(s16 x, s16 z, s8 dst, s8 src);
s32 walk_zone_find(s32 x, s32 z);
s32 walk_zone_shared_edge(u16 zone_a, u16 zone_b);
s32 point_collides(s16 x, s16 z);
s16 get_angle_towards_target(s16 x, s16 z);
s32 joint_move(u8 reverse, s16 *skel, u8 *anim_tbl, s16 blend_step);
void reset_joint_transforms(void);
s32 check_entity_collision(u8 *a, u8 *b);
void entity_rotate_collision_points(u8 *obj);
void rotate_vector_y(s16 angle, SVECTOR *src, void *dst);
void billboard_init(SVECTOR *pos, u8 *bb, s16 half_w, s16 half_d);
void billboard_set_color(u8 *bb, s32 semi, s32 unused, s32 color);
void billboard_adj_size(void *bb, s32 dx, s32 dz);
void billboard_draw(VECTOR *pos, u8 *obj, s16 y, s16 ang);
void entity_update_weapon_joint(u8 limb);
void entity_update_look_at_angles(void);
s32 Flg_ck(s32 *flags, u32 bit);
s32 check_room_collision(VECTOR *pos, u16 radius);
void sound_play_se_3d(u8 type, u8 idx, s32 unused, s32 pos);
void sound_play_footstep(u8 se);


void npc_state9_pathfind(void) {
    s8 walk_zone_find_a1() __asm__("walk_zone_find");
    s8 joint_move_a37() __asm__("joint_move");
    s32 check_entity_collision_a1() __asm__("check_entity_collision");
    s32 entity_rotate_collision_points_a3() __asm__("entity_rotate_collision_points");
    s32 check_room_collision_a2() __asm__("check_room_collision");
    void npc_walk_state9_enter();
    void npc_walk_behavior_swap();
    s32 npc_walk_footstep_sound_a1() __asm__("npc_walk_footstep_sound");
    extern s32 (*npc_walk_behavior_tbl[])();
    void *e3;
    void *e;
    void *e2;

    if (((u8) CURRENT_ENTITY_LIT->sub_state) == 0) {
        npc_walk_state9_enter();
    }
    entity_pathfind_update();
    npc_walk_behavior_swap();
    e = LIT(void *, g_CurrentEntity);
    NPC_CUR->zone = walk_zone_find_a1(FIELD_AT(e, Entity, ->pos_x, s16), FIELD_AT(e, Entity, ->pos_z, s16));
    npc_walk_behavior_tbl[CURRENT_ENTITY_LIT->action_behavior]();
    e2 = LIT(void *, g_CurrentEntity);
    NPC_CUR->anim_done = joint_move_a37(NPC_W(e2)->anim_flags & 1, ENT(e2)->anim_header, ENT(e2)->anim_base, 0x1000 / (s32) (((u8) ENT(e2)->blend_counter) + 1));
    npc_walk_footstep_sound_a1();
    entity_rotate_collision_points_a3(LIT(void *, g_CurrentEntity));
    check_entity_collision_a1(ADDR_g_PlayerEntity, LIT(void *, g_CurrentEntity));
    entity_resolve_enemy_player_collisions();
    e3 = LIT(void *, g_CurrentEntity);
    check_room_collision_a2(&ENT(e3)->pos_x, SCA(((void *) ENT(e3)->sca_info))->radius);
}

void npc_walk_behavior_00(void) {
    void npc_walk_reset_lookat();
    s32 npc_walk_lookat_wander_a1() __asm__("npc_walk_lookat_wander");
    int rand(void);
  s32 one_side;
  u8 state;
  void *e;
  void *e2;
  void *e3;
  e = LIT(void *, g_CurrentEntity);
  state = *((u8 *) (((s8 *) e) + 0x87));
  switch (state)
  {
    case 0:
      *((u8 *) (((s8 *) e) + 0x87)) = 1U;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBD)) = 0;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBE)) = 0;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBF)) = 0;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0x8C)) = 7;
      *((u8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0x16E)) = (u8) ((rand() & 0x38) + 0x40);
      npc_walk_reset_lookat();
      return;

    case 1:
      *((u8 *) (((s8 *) e) + 0x16E)) = (u8) ((*((s8 *) (((s8 *) e) + 0x16E))) - 1);
      e2 = LIT(void *, g_CurrentEntity);
      one_side = 0;
      if ((*((u8 *) (((s8 *) e2) + 0x16E))) == 0)
    {
      *((u8 *) (((s8 *) e2) + 0x87)) = 2U;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBD)) = 5;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBE)) = 0;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBF)) = 0;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0x8C)) = 7;
      npc_walk_reset_lookat(0);
      case 2:
        e3 = LIT(void *, g_CurrentEntity);

      if ((*((u8 *) (((s8 *) e3) + 0x16C))) & 1)
      {
        *((u8 *) (((s8 *) e3) + 0x87)) = 3U;
        *((s8 *) (((s8 *) (LIT(s32, g_CurrentEntity))) + 0xBD)) = 6;
        *((s8 *) (((s8 *) (LIT(s32, g_CurrentEntity))) + 0xBE)) = 0;
        *((s8 *) (((s8 *) (LIT(s32, g_CurrentEntity))) + 0xBF)) = 0;
        *((s8 *) (((s8 *) (LIT(s32, g_CurrentEntity))) + 0x8C)) = 7;
        case 3:
          one_side = *((u8 *) (((s8 *) g_CurrentEntity) + 1));
          one_side = one_side & 1;
          one_side = one_side ^ 1;

        goto block_13;
      }
    }
    else
    {
      block_13:
      npc_walk_lookat_wander_a1(one_side);

    }
      return;

  }

}

void npc_walk_behavior_01(void) {
    s32 npc_walk_choose_heading_a1() __asm__("npc_walk_choose_heading");
    s32 npc_walk_turn_toward_heading_a1() __asm__("npc_walk_turn_toward_heading");
    s32 npc_walk_advance_xz_a1() __asm__("npc_walk_advance_xz");
    s32 npc_walk_lookat_wander_a2() __asm__("npc_walk_lookat_wander");
  unsigned int arc_lo;
  void *e2;
  void *e3;
  void *e;
  npc_walk_choose_heading_a1();
  e = LIT(void *, g_CurrentEntity);
  if ((*((u8 *) (((s8 *) e) + 0x87))) == 0)
  {
    *((u8 *) (((s8 *) e) + 0x87)) = 1U;
    e2 = LIT(void *, g_CurrentEntity);
    if ((*((u8 *) (((s8 *) e2) + 0xBD))) != 7)
    {
      *((u8 *) (((s8 *) e2) + 0xBD)) = 7U;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBE)) = 0;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBF)) = 0;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0x8C)) = 7;
    }
  }
  e3 = LIT(void *, g_CurrentEntity);
  arc_lo = (*((s16 *) (((s8 *) e3) + 0x74))) - 0x180;
  if (((u32) ((*((s16 *) (((s8 *) e3) + 0x176))) - arc_lo)) < 0x301U)
  {
    npc_walk_turn_toward_heading_a1(0x30);
    entity_apply_walk_speed(0x5D);
    npc_walk_advance_xz_a1(*((s16 *) (((s8 *) (LIT(s32, g_CurrentEntity))) + 0xC2)));
  }
  else
  {
    npc_walk_turn_toward_heading_a1(0x28);
  }
  npc_walk_lookat_wander_a2(0);
}

void npc_walk_behavior_02(void) {
    s32 entity_apply_walk_speed_p(s32) __asm__("entity_apply_walk_speed");
    s32 npc_walk_choose_heading_a2() __asm__("npc_walk_choose_heading");
    s32 npc_walk_turn_toward_heading_a2(s32) __asm__("npc_walk_turn_toward_heading");
    s32 npc_walk_advance_xz_a2(s16) __asm__("npc_walk_advance_xz");
    void npc_walk_set_lookat_target(s16 x, s16 z);
    void *e;
    void *e2;

    npc_walk_choose_heading_a2();
    e = LIT(void *, g_CurrentEntity);
    if ((u32) (NPC_W(e)->heading + 0x180 - ((s16) ENT(e)->angle)) < 0x301U) {
        if (((u8) ENT(e)->animation_id) != 8) {
            ENT(e)->animation_id = 8U;
            CURRENT_ENTITY_LIT->animation_frame_id = 0;
            CURRENT_ENTITY_LIT->timing_control = 0;
            CURRENT_ENTITY_LIT->blend_counter = 7;
        }
        npc_walk_turn_toward_heading_a2(0x60);
        npc_walk_advance_xz_a2(0xD2);
    } else {
    if (((u8) ENT(e)->animation_id) != 7) {
        ENT(e)->animation_id = 7U;
        CURRENT_ENTITY_LIT->animation_frame_id = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->blend_counter = 7;
    }
    npc_walk_turn_toward_heading_a2(0x30);
    e2 = LIT(void *, g_CurrentEntity);
    if ((u32) (NPC_W(e2)->heading + 0x200 - ((s16) ENT(e2)->angle)) < 0x401U) {
        entity_apply_walk_speed_p(0x5D);
        npc_walk_advance_xz_a2(CURRENT_ENTITY_LIT->move_speed_current);
    }
    }
    npc_walk_set_lookat_target(((s16) g_CurrentEntity->waypoint_x), ((s16) g_CurrentEntity->waypoint_z));
}

void npc_walk_behavior_03(void) {
    s32 entity_apply_walk_speed_p(s32) __asm__("entity_apply_walk_speed");
    s16 get_angle_towards_target_p(s32, s32) __asm__("get_angle_towards_target");
    s32 npc_walk_turn_toward_heading_a3(s32) __asm__("npc_walk_turn_toward_heading");
    s32 npc_walk_advance_xz_a3(s16) __asm__("npc_walk_advance_xz");
    void npc_walk_reset_lookat();
    void npc_walk_set_lookat_target(s16 x, s16 z);
  s16 to_player;
  void *e;
  void *e2;
  int arc_lo;
  to_player = get_angle_towards_target_p(g_PlayerPosX, g_PlayerPosZ);
  e = LIT(void *, g_CurrentEntity);
  *((s16 *) (((s8 *) e) + 0x176)) = to_player;
  arc_lo = (*((s16 *) (((s8 *) e) + 0x74))) - 0x200;
  if (((u32) (to_player - arc_lo)) < 0x401U)
  {
    if ((*((u8 *) (((s8 *) e) + 0xBD))) != 3)
    {
      *((u8 *) (((s8 *) e) + 0xBD)) = 3U;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBE)) = 0;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBF)) = 0;
      *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0x8C)) = 7;
    }
    npc_walk_turn_toward_heading_a3(0x30);
    npc_walk_advance_xz_a3(-0x3C);
    npc_walk_set_lookat_target((s16) g_PlayerPosX, (s16) g_PlayerPosZ);
    return;
  }
  if ((*((u8 *) (((s8 *) e) + 0xBD))) != 7)
  {
    *((u8 *) (((s8 *) e) + 0xBD)) = 7U;
    *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBE)) = 0;
    *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xBF)) = 0;
    *((s8 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0x8C)) = 7;
  }
  e2 = LIT(void *, g_CurrentEntity);
  *((s16 *) (((s8 *) e2) + 0x176)) = (s16) ((((u16) (*((s16 *) (((s8 *) e2) + 0x176)))) + 0x800) & 0xFFF);
  npc_walk_turn_toward_heading_a3(0x30);
  entity_apply_walk_speed_p(0x5D);
  npc_walk_advance_xz_a3(*((s16 *) (((s8 *) (LIT(void *, g_CurrentEntity))) + 0xC2)));
  npc_walk_reset_lookat();
}

void npc_walk_state9_enter(void) {
    void npc_walk_reset_lookat();
    g_CurrentEntity->sub_state = 1;
    npc_walk_reset_lookat();
}

void npc_walk_behavior_swap(void) {
    long SquareRoot0(long a);
    s32 dist;
    s32 dx;
    s32 dz;
    u16 far_dist;
    u16 near_dist;
    u8 beh;
    u8 beh2;
    void *e;
    void *e2;

    e = LIT(void *, g_CurrentEntity);
    dx = ENT(e)->pos_x - g_PlayerEntity.pos_x;
    dz = ENT(e)->pos_z - g_PlayerEntity.pos_z;
    dist = SquareRoot0((dx * dx) + (dz * dz));
    e2 = LIT(void *, g_CurrentEntity);
    beh = ENT(e2)->action_behavior;
    near_dist = ((s32 *)&g_NpcWalkSwapNearDist)[beh];
    if ((near_dist != 0) && ((u32) dist < near_dist)) {
        FIELD_AT(e2, Entity, ->action_behavior, s16) = *(u8 *)((u8 *)&g_NpcWalkSwapNearNext + (beh * 2));
        return;
    }
    beh2 = g_CurrentEntity->action_behavior;
    far_dist = ((s32 *)&g_NpcWalkSwapFarDist)[beh2];
    if ((far_dist != 0) && ((u32) dist >= far_dist)) {
        { s32 wt = (s16) *(u8 *)((u8 *)&g_NpcWalkSwapFarNext + ((beh2 * 2))); ENTITY_FIELD_AS(g_CurrentEntity, action_behavior, s16) = wt; }
    }
}

void npc_walk_choose_heading(void) {
    s16 zone_edge_heading_l(s32, s32, u8, u8) __asm__("zone_edge_heading");
    u8 walk_zone_shared_edge_l(u8, u8) __asm__("walk_zone_shared_edge");
    u16 point_collides_l(s16, s16) __asm__("point_collides");
    s16 get_angle_towards_target_l(s32, s32) __asm__("get_angle_towards_target");
    u8 npc_walk_corridor_open_l(u8, s16, s16) __asm__("npc_walk_corridor_open");
    u8 *e;
    u8 r;
    s32 x;
    s32 z;
    s16 t;

    LIT(u8 *, g_CurrentEntity)[0x175] = zone_path_find(g_PlayerEntity.pos_x, g_PlayerEntity.pos_z, (s16 *)(LIT(u8 *, g_CurrentEntity) + 0x166),
                                                  (s16 *)(LIT(u8 *, g_CurrentEntity) + 0x168));
    if (LIT(u8 *, g_CurrentEntity)[0x175] & 0x10) {
        *(s16 *)(LIT(u8 *, g_CurrentEntity) + 0x176) = get_angle_towards_target_l(g_PlayerPosX, g_PlayerEntity.pos_z);
        return;
    }
    r = walk_zone_shared_edge_l(LIT(u8 *, g_CurrentEntity)[0x174], LIT(u8 *, g_CurrentEntity)[0x175]);
    if (r) {
        z = LIT(s32, sp_tmp04);
        x = (g_PlayerPosX - *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x34)) * (z - *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x3C));
        x = x / (g_PlayerEntity.pos_z - *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x3C));
        x += *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x34);
    } else {
        x = LIT(s32, sp_tmp00);
        z = (g_PlayerEntity.pos_z - *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x3C)) * (x - *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x34));
        z = z / (g_PlayerPosX - *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x34));
        z += *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x3C);
    }
    if (npc_walk_corridor_open_l(r, x, z) && !point_collides_l(x, z)) {
        t = get_angle_towards_target_l(x, z);
        e = LIT(u8 *, g_CurrentEntity);
        NPC_W(e)->heading = t;
        *(s16 *)(&ENT(e)->waypoint_x) = x;
        *(s16 *)(&ENT(e)->waypoint_z) = z;
        return;
    }
    *(s16 *)(LIT(u8 *, g_CurrentEntity) + 0x176) = zone_edge_heading_l(*(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x34), *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x3C),
                                                           LIT(u8 *, g_CurrentEntity)[0x175], LIT(u8 *, g_CurrentEntity)[0x174]);
    e = LIT(u8 *, g_CurrentEntity);
    ENT(e)->waypoint_x = LIT(s32, sp_tmp00);
    ENT(e)->waypoint_z = LIT(s32, sp_tmp04);
}

s32 npc_walk_corridor_open(u8 mode, s16 x, s16 z) {
    extern u8 * g_RoomData_u8p __asm__("g_RoomData");
    extern u8 * g_CurrentEntity_u8p __asm__("g_CurrentEntity");
    typedef struct {
        u16 x0;
        u16 z0;
        u16 x1;
        u16 z1;
        u16 a;
        u16 b;
    } WalkZone;
    u16 point_collides_l(s16, s16) __asm__("point_collides");
    u8 *tbl;
    WalkZone *ra;
    WalkZone *rb;
    u16 pos;
    u16 lo;
    u16 hi;
    s32 d;
    s16 o;

    tbl = ((u8 *) ROOM_P->blocks);
    ra = (WalkZone *)(g_CurrentEntity_u8p[0x174] * 12 + (s32)tbl + 2);
    rb = (WalkZone *)(g_CurrentEntity_u8p[0x175] * 12 + (s32)tbl + 2);
    if (mode) {
        pos = x;
        if (ra->x0 > rb->x0) {
            lo = ra->x0;
        } else {
            lo = rb->x0;
        }
        if (ra->x1 < rb->x1) {
            hi = ra->x1;
        } else {
            hi = rb->x1;
        }
    } else {
        pos = z;
        if (ra->z0 > rb->z0) {
            lo = ra->z0;
        } else {
            lo = rb->z0;
        }
        if (ra->z1 < rb->z1) {
            hi = ra->z1;
        } else {
            hi = rb->z1;
        }
    }
    d = ((hi - lo) >> 3) + 0x280;
    if (mode) {
        o = z;
        if (point_collides_l(lo, o)) {
            lo += d;
        }
        if (point_collides_l(hi, o)) {
            hi -= d;
        }
    } else {
        o = x;
        if (point_collides_l(o, lo)) {
            lo += d;
        }
        if (point_collides_l(o, hi)) {
            hi -= d;
        }
    }
    if (hi < lo) {
        return 0;
    }
    return (pos >= lo) & (pos < hi);
}

void npc_walk_turn_toward_heading(s32 base_step) {
    s16 target;
    s16 cur;
    s16 step;
    void **pp;

    target = NPC_W(g_CurrentEntity)->heading;
    cur = ((s16) g_CurrentEntity->angle);
    step = base_step + ((g_CurrentEntity->id & 1) * 8);
    if ((u32) ((target + step) - cur) <= (u32) (step * 2)) {
        g_CurrentEntity->angle = target;
        return;
    }
    if ((target - cur) & 0x800) {
        g_CurrentEntity->angle = cur - step;
    } else {
        g_CurrentEntity->angle = step + cur;
    }
    pp = (void **)&g_CurrentEntity;
    ENT(*pp)->angle &= 0xFFF;
}

void npc_walk_advance_xz(s16 dist) {
    s32 rotate_vector_y_p() __asm__("rotate_vector_y");
    void *e;

    LIT(s16, sp_svec_vx) = dist;
    LIT(s16, sp_svec_vy) = 0;
    SCRATCH->sv_z = 0;
    rotate_vector_y_p(CURRENT_ENTITY_LIT_FIELD_AS(angle, s16), ADDR_sp_svec, ADDR_sp_svec);
    e = LIT(void *, g_CurrentEntity);
    ENT(e)->pos_x = (s32) (LIT(s16, sp_svec_vx) + ENT(e)->pos_x);
    ENT(e)->pos_z = (s32) (SCRATCH->sv_z + ENT(e)->pos_z);
}

void npc_walk_reset_lookat(void) {
    int rand(void);
    void *e2;
    void *e;

    NPC_CUR->look_timer = (s8) ((s32) ((rand() & 0x18) + 0x30) >> 1);
    e = LIT(void *, g_CurrentEntity);
    FIELD_AT(e, NpcWork, ->look_rand0, s32) = 0;
    FIELD_AT(e, Entity, ->look_at_flags, s8) = 0x10;
    e2 = LIT(void *, g_CurrentEntity);
    FIELD_AT(e2, Entity, ->scd_pos_x, s32) = 0;
    FIELD_AT(e2, Entity, ->scd_pos_y, s32) = 0;
    FIELD_AT(e2, Entity, ->scd_pos_z, s32) = 0;
    FIELD_AT(e2, Entity, ->look_at_yaw_step, u8) = 0xC0;
    CURRENT_ENTITY_LIT->look_at_pitch_step = 0x40;
}

#define OBJ (LIT(u8 *, g_CurrentEntity))

void npc_walk_lookat_wander(s32 one_side) {
    int rand(void);
    s32 mask;
    u32 r;
    s32 t;
    s32 v;
    u8 a;
    u8 b;
    u8 c;
    u8 d;

    CURRENT_ENTITY_LIT->look_at_flags = 0x33;
    NPC_OBJ->look_timer--;
    mask = (((one_side & 1) ^ 1) + 1) * 4 - 1;
    if (NPC_OBJ->look_timer == 0) {
        NPC_OBJ->look_timer = (rand() & 0x18) + 0x30;
        r = rand();
        t = r & mask;
        a = t;
        r &= 0xFFFF;
        b = (r >> 4) & 7;
        c = (r >> 8) & 0x18;
        d = (r >> 12) & 8;
        if (NPC_OBJ->look_rand0 == a || ((a | NPC_OBJ->look_rand0) & 3) == 0) {
            a = mask & (t + 1);
        }
        NPC_OBJ->look_rand0 = a;
        v = (a & 3) * 0x60;
        CURRENT_ENTITY_LIT->scd_pos_y = v;
        if (!(a & 4)) {
            CURRENT_ENTITY_LIT->scd_pos_y = -v;
        }
        CURRENT_ENTITY_LIT->scd_pos_y &= 0xFFF;
        if (NPC_OBJ->look_rand1 == b || ((b | NPC_OBJ->look_rand1) & 3) == 0) {
            b = (b + 1) & 7;
        }
        NPC_OBJ->look_rand1 = b;
        v = (b & 3) * 0xA0;
        CURRENT_ENTITY_LIT->scd_pos_x = v;
        if (b & 4) {
            CURRENT_ENTITY_LIT->scd_pos_x = -v;
        }
        CURRENT_ENTITY_LIT->scd_pos_x &= 0xFFF;
        if (NPC_OBJ->look_rand2 == c) {
            c = (c + 8) & 0x18;
        }
        NPC_OBJ->look_rand2 = c;
        CURRENT_ENTITY_LIT->look_at_pitch_step = c + 0x14;
        if (NPC_OBJ->look_rand3 == d) {
            d = (d + 8) & 0x18;
        }
        NPC_OBJ->look_rand3 = d;
        CURRENT_ENTITY_LIT->look_at_yaw_step = d + 0x28;
    }
}
#undef OBJ

void npc_walk_set_lookat_target(s16 x, s16 z) {
    int rand(void);
    void *e;
    void *e2;

    NPC_CUR->look_timer = (s8) ((s32) ((rand() & 0x18) + 0x30) >> 1);
    e = LIT(void *, g_CurrentEntity);
    FIELD_AT(e, NpcWork, ->look_rand0, s32) = 0;
    FIELD_AT(e, Entity, ->look_at_flags, s8) = 0x11;
    e2 = LIT(void *, g_CurrentEntity);
    FIELD_AT(e2, Entity, ->scd_pos_x, s32) = (s32) x;
    FIELD_AT(e2, Entity, ->scd_pos_y, s32) = 0;
    FIELD_AT(e2, Entity, ->scd_pos_z, s32) = (s32) z;
    FIELD_AT(e2, Entity, ->look_at_yaw_step, u8) = 0xC0;
    CURRENT_ENTITY_LIT->look_at_pitch_step = 0x40;
}

void npc_walk_footstep_sound(void) {
    void sound_play_footstep_l() __asm__("sound_play_footstep");
    s8 v;

    switch (((u8) g_CurrentEntity->animation_id)) {
    case 3:
    case 7:
        if (g_CurrentEntity->animation_frame_id == 8) {
            v = (g_CurrentEntity->id & 1) * 2;
            sound_play_footstep_l(0, v);
        } else if (g_CurrentEntity->animation_frame_id == 0x16) {
            v = (g_CurrentEntity->id & 1) * 2 - 4;
            sound_play_footstep_l(0, v);
        }
        break;
    case 8:
        if (g_CurrentEntity->animation_frame_id == 0) {
            v = (g_CurrentEntity->id & 1) * 2;
            sound_play_footstep_l(1, v);
        } else if (g_CurrentEntity->animation_frame_id == 0xA) {
            v = (g_CurrentEntity->id & 1) * 2 - 4;
            sound_play_footstep_l(1, v);
        }
        break;
    }
}

void character_npc_update(void) {
    u8 is_point_in_quad_a4() __asm__("is_point_in_quad");
    s32 billboard_draw_a3() __asm__("billboard_draw");
    s32 entity_update_look_at_angles_a2() __asm__("entity_update_look_at_angles");
    extern s32 (*g_NpcStateTable[])();
    void *e2;
    void *e;

    if (g_UpdateFlags & 2) {
        g_NpcStateTable[g_CurrentEntity->state]();
        entity_update_look_at_angles_a2();
    }
    e = LIT(void *, g_CurrentEntity);
    ENT(e)->sca_matrix_head = 0;
    CURRENT_ENTITY_LIT->has_enter_switch_zone = is_point_in_quad_a4(&ENT(e)->pos_x, g_CameraSwitchZones);
    e2 = LIT(void *, g_CurrentEntity);
    if ((ENT(e2)->has_enter_switch_zone != 0) && !(ENT(e2)->scd_entity_flags & 0x40)) {
        billboard_draw_a3(&ENT(e2)->pos_x, &ENT(e2)->shadow, ENT(e2)->pos_y, ((s16) ENT(e2)->angle));
    }
}

void npc_state0_init(void) {
    s32 reset_joint_transforms_a3() __asm__("reset_joint_transforms");
    s32 entity_rotate_collision_points_a4() __asm__("entity_rotate_collision_points");
    extern s32 (*g_NpcDispatchTable[])();
    void *e;

    e = LIT(void *, g_CurrentEntity);
    FIELD_AT(e, Entity, ->state, s32) = 1;
    ENT(e)->rot_x = 0;
    ENT(e)->angle_z = 0;
    ENT(e)->health = -1;
    ENT(e)->scale = 0;
    reset_joint_transforms_a3();
    g_NpcDispatchTable[CURRENT_ENTITY_LIT->id]();
    entity_rotate_collision_points_a4(LIT(void *, g_CurrentEntity));
}

void npc_char_init_chris(void) {
    s32 load_behavior_model_a1() __asm__("load_behavior_model");
    s32 joint_move_a38(s32, s32, s32, s32) __asm__("joint_move");
    s32 billboard_init_a3(s32, s32, s32, s32) __asm__("billboard_init");
    void *e;

    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = -0x50;
    LIT(s32, sp_tmp1C) = 0x808080;
    billboard_init_a3(ADDR_sp_svec, (s32)(LIT(void *, g_CurrentEntity) + 0xE4), 0x200, 0x280);
    CURRENT_ENTITY_LIT->blend_counter = 0;
    e = LIT(void *, g_CurrentEntity);
    joint_move_a38(0, ENT(e)->anim_header, ENT(e)->anim_base, 0x400);
    CURRENT_ENTITY_LIT->sca_info = (s32)&g_NpcScaInfoChris;
    load_behavior_model_a1();
}

void npc_char_init_jill(void) {
    s32 load_behavior_model_a2() __asm__("load_behavior_model");
    s32 joint_move_a39(s32, s32, s32, s32) __asm__("joint_move");
    s32 billboard_init_a4(s32, s32, s32, s32) __asm__("billboard_init");
    void *e;

    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = -0x50;
    LIT(s32, sp_tmp1C) = 0x808080;
    billboard_init_a4(ADDR_sp_svec, (s32)(LIT(void *, g_CurrentEntity) + 0xE4), 0x200, 0x280);
    CURRENT_ENTITY_LIT->blend_counter = 0;
    e = LIT(void *, g_CurrentEntity);
    joint_move_a39(0, ENT(e)->anim_header, ENT(e)->anim_base, 0x400);
    CURRENT_ENTITY_LIT->sca_info = (s32)&g_NpcScaInfoJill;
    load_behavior_model_a2();
}

void npc_char_init_barry(void) {
    s32 load_behavior_model_a3() __asm__("load_behavior_model");
    s32 joint_move_a40(s32, s32, s32, s32) __asm__("joint_move");
    s32 billboard_init_a5(s32, s32, s32, s32) __asm__("billboard_init");
    void *e;

    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = -0x50;
    LIT(s32, sp_tmp1C) = 0x808080;
    billboard_init_a5(ADDR_sp_svec, (s32)(LIT(void *, g_CurrentEntity) + 0xE4), 0x200, 0x280);
    CURRENT_ENTITY_LIT->blend_counter = 0;
    e = LIT(void *, g_CurrentEntity);
    joint_move_a40(0, ENT(e)->anim_header, ENT(e)->anim_base, 0x400);
    CURRENT_ENTITY_LIT->sca_info = (s32)&g_NpcScaInfoBarry;
    load_behavior_model_a3();
}

void npc_char_init_rebecca(void) {
    s32 load_behavior_model_a4() __asm__("load_behavior_model");
    s32 joint_move_a41(s32, s32, s32, s32) __asm__("joint_move");
    s32 billboard_init_a6(s32, s32, s32, s32) __asm__("billboard_init");
    s32 billboard_set_color_a1(void *, s32, s32, s32) __asm__("billboard_set_color");
    s32 Flg_ck_a17(s32, s32) __asm__("Flg_ck");
    void *joints;
    void *e;
    void *e2;

    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = -0x50;
    LIT(s32, sp_tmp1C) = 0x808080;
    billboard_init_a6(ADDR_sp_svec, (s32)(LIT(void *, g_CurrentEntity) + 0xE4), 0x200, 0x280);
    CURRENT_ENTITY_LIT->blend_counter = 0;
    if (Flg_ck_a17(ADDR_g_ScenarioFlags2, 0xC0) != 0) {
        e = LIT(void *, g_CurrentEntity);
        joints = ((void *) ENT(e)->joints);
        ENT(e)->timing_control = 0;
        CURRENT_ENTITY_LIT->animation_id = 0x33;
        CURRENT_ENTITY_LIT->animation_frame_id = 0x3D;
        billboard_set_color_a1(LIT(void *, g_CurrentEntity) + 0xE4, 1, 2, 0xFFFF70);
        billboard_adj_size(LIT(void *, g_CurrentEntity) + 0xE4, 0x1E0, 0x1E0);
        FIELD_AT(joints, Joint, [1].flags, u8) = (u8) (FIELD_AT(joints, Joint, [1].flags, u8) & 0xFE);
        joint_apply_color_tint(joints, 0x30, 0x80820, 0x606060);
        joint_apply_color_tint(joints + 0x45C, 0x30, 0x80820, 0x606060);
        joint_apply_color_tint(joints + 0x5D0, 0x30, 0x80820, 0x606060);
    }
    e2 = LIT(void *, g_CurrentEntity);
    joint_move_a41(0, ENT(e2)->anim_header, ENT(e2)->anim_base, 0x400);
    CURRENT_ENTITY_LIT->sca_info = (s32)&g_NpcScaInfoRebecca;
    load_behavior_model_a4();
}

void npc_char_init_wesker(void) {
    s32 load_behavior_model_a5(void *) __asm__("load_behavior_model");
    s32 joint_move_a42(s32, s32, s32, s32) __asm__("joint_move");
    s32 billboard_init_a7(s32, s32, s32, s32) __asm__("billboard_init");
    s32 billboard_set_color_a2(void *, s32, s32, s32) __asm__("billboard_set_color");
    s32 billboard_adj_size_p(void *, s32, s32) __asm__("billboard_adj_size");
    s32 Flg_ck_a18(s32, s32) __asm__("Flg_ck");
    extern u16 g_StageId_u16 __asm__("g_StageId");
    void *e2;
    void *e;
    void *joints;

    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = -0x50;
    LIT(s32, sp_tmp1C) = 0x808080;
    billboard_init_a7(ADDR_sp_svec, (s32)(LIT(void *, g_CurrentEntity) + 0xE4), 0x200, 0x280);
    CURRENT_ENTITY_LIT->blend_counter = 0;
    if (Flg_ck_a18(ADDR_g_ScenarioFlags, 0x37) != 0) {
        CURRENT_ENTITY_LIT->animation_id = 0x30;
        CURRENT_ENTITY_LIT->animation_frame_id = 0x6D;
        billboard_set_color_a2(LIT(void *, g_CurrentEntity) + 0xE4, 1, 2, 0xFFFF70);
        billboard_adj_size_p(LIT(void *, g_CurrentEntity) + 0xE4, 0x5DC, 0x5DC);
    }
    e = LIT(void *, g_CurrentEntity);
    joint_move_a42(0, ENT(e)->anim_header, ENT(e)->anim_base, 0x400);
    e2 = LIT(void *, g_CurrentEntity);
    joints = ((void *) ENT(e2)->joints);
    MEMBER_AT(joints, Joint, [1].rot_delta_z, s16) = 0x10;
    MEMBER_AT(joints, Joint, [1].rot_delta_x, s16) = 0;
    MEMBER_AT(joints, Joint, [1].rot_delta_y, s16) = 0;
    ENT(e2)->sca_info = (s32)&g_NpcScaInfoWesker;
    if (g_StageId_u16 == 0x1104) {
        ENT(e2)->status_flags = (u8) (ENT(e2)->status_flags | 2);
    }
    load_behavior_model_a5(e2);
}

void npc_char_init_kenneth_corpse(void) {
    s32 joint_move_a43(s32, s32, s32, s32) __asm__("joint_move");
    s32 billboard_init_a8(s32, s32, s32, s32) __asm__("billboard_init");
    void *e;

    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s32, sp_tmp1C) = 0xFFFF50;
    billboard_init_a8(ADDR_sp_svec, (s32)(LIT(void *, g_CurrentEntity) + 0xE4), 0x1F4, 0x2BC);
    CURRENT_ENTITY_LIT->animation_id = 0;
    CURRENT_ENTITY_LIT->animation_frame_id = 0;
    CURRENT_ENTITY_LIT->timing_control = 0;
    CURRENT_ENTITY_LIT->blend_counter = 0;
    e = LIT(void *, g_CurrentEntity);
    joint_move_a43(0, ENT(e)->anim_header, ENT(e)->anim_base, 0x400);
    CURRENT_ENTITY_LIT->sca_info = (s32)&g_NpcScaInfoKennethCorpse;
}

void npc_char_init_forest_corpse(void) {
    s32 joint_move_a44(s32, s32, s32, s32) __asm__("joint_move");
    s32 billboard_init_a9(s32, s32, s32, s32) __asm__("billboard_init");
    void *e;

    LIT(s16, sp_svec_vx) = -0x258;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0xC8;
    LIT(s32, sp_tmp1C) = 0xFFFF50;
    billboard_init_a9(ADDR_sp_svec, (s32)(LIT(void *, g_CurrentEntity) + 0xE4), 0x2BC, 0x2BC);
    CURRENT_ENTITY_LIT->animation_id = 0;
    CURRENT_ENTITY_LIT->animation_frame_id = 0;
    CURRENT_ENTITY_LIT->timing_control = 0;
    CURRENT_ENTITY_LIT->blend_counter = 0;
    e = LIT(void *, g_CurrentEntity);
    joint_move_a44(0, ENT(e)->anim_header, ENT(e)->anim_base, 0x400);
    CURRENT_ENTITY_LIT->sca_info = (s32)&g_NpcScaInfoForestCorpse;
}

void npc_char_init_richard(void) {
    s32 joint_move_a45(s32, s32, s32, s32) __asm__("joint_move");
    s32 billboard_init_a10(s32, s32, s32, s32) __asm__("billboard_init");
    void *e;

    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s32, sp_tmp1C) = 0x606060;
    billboard_init_a10(ADDR_sp_svec, (s32)(LIT(void *, g_CurrentEntity) + 0xE4), 0x1F4, 0x2BC);
    CURRENT_ENTITY_LIT->blend_counter = 0;
    e = LIT(void *, g_CurrentEntity);
    joint_move_a45(0, ENT(e)->anim_header, ENT(e)->anim_base, 0x400);
    CURRENT_ENTITY_LIT->sca_info = (s32)&g_NpcScaInfoRichard;
}

void npc_char_init_enrico(void) {
    s32 joint_move_a46(s32, s32, s32, s32) __asm__("joint_move");
    s32 billboard_init_a11(s32, s32, s32, s32) __asm__("billboard_init");
    void *e;

    LIT(s16, sp_svec_vx) = -0x258;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0xC8;
    LIT(s32, sp_tmp1C) = 0x404040;
    billboard_init_a11(ADDR_sp_svec, (s32)(LIT(void *, g_CurrentEntity) + 0xE4), 0x2BC, 0x2BC);
    CURRENT_ENTITY_LIT->blend_counter = 0;
    e = LIT(void *, g_CurrentEntity);
    joint_move_a46(0, ENT(e)->anim_header, ENT(e)->anim_base, 0x400);
    CURRENT_ENTITY_LIT->sca_info = (s32)&g_NpcScaInfoEnrico;
}

void npc_char_init_41(void) {
    s32 joint_move_a47(s32, s32, s32, s32) __asm__("joint_move");
    s32 billboard_init_a12(s32, s32, s32, s32) __asm__("billboard_init");
    extern s32 g_NpcScaInfo41;
    void *e;

    LIT(s16, sp_svec_vx) = 0;
    LIT(s16, sp_svec_vy) = 0;
    LIT(s16, sp_svec_vz) = 0;
    LIT(s32, sp_tmp1C) = 0x606060;
    billboard_init_a12(ADDR_sp_svec, (s32)(LIT(void *, g_CurrentEntity) + 0xE4), 0x1F4, 0x2BC);
    CURRENT_ENTITY_LIT->animation_id = 0;
    CURRENT_ENTITY_LIT->animation_frame_id = 0;
    CURRENT_ENTITY_LIT->timing_control = 0;
    CURRENT_ENTITY_LIT->blend_counter = 0;
    e = LIT(void *, g_CurrentEntity);
    joint_move_a47(0, ENT(e)->anim_header, ENT(e)->anim_base, 0x400);
    CURRENT_ENTITY_LIT->sca_info = (s32)&g_NpcScaInfo41;
}

void npc_state1_idle(void) {
    extern s32 (*g_NpcIdleBehaviourTable[])();
    g_NpcIdleBehaviourTable[g_CurrentEntity->action_behavior]();
}

void npc_idle_walk_00(void) {
    extern s32 (*g_NpcScaInfo41_fn[])() __asm__("g_NpcScaInfo41");
    g_NpcScaInfo41_fn[g_CurrentEntity->id]();
}

void npc_idle_nop(void) {
}

void npc_idle_play_anim(void) {
    s32 joint_move_a48(s32, s32, s32, s32) __asm__("joint_move");
    u8 state;
    void *e;

    e = LIT(void *, g_CurrentEntity);
    state = ENT(e)->action_state;
    switch (state) {
    case 0:
        ENT(e)->action_state = 1U;
        CURRENT_ENTITY_LIT->animation_id = 0;
        CURRENT_ENTITY_LIT->animation_frame_id = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->blend_counter = 0;
        /* fallthrough */
    case 1:
        joint_move_a48(0, g_CurrentEntity->anim_header, g_CurrentEntity->anim_base, 0x400);
        return;
    }
}

void npc_idle_walk_01(void) {
    void voice_command_l() __asm__("voice_command");
    s32 joint_move_l() __asm__("joint_move");
    void entity_update_weapon_joint_l() __asm__("entity_update_weapon_joint");
    s32 check_room_collision_l() __asm__("check_room_collision");
    void sound_play_se_3d_l() __asm__("sound_play_se_3d");
    u8 *e;
    u8 *e1;
    u8 *e2;
    u8 *e3;
    VECTOR save;   /* the position (pos_x/y/z) and the word after it */

    e = LIT(u8 *, g_CurrentEntity);
    switch (ENT(e)->action_state) {
    case 0:
        ENT(e)->action_state = 1;
        LIT(u8 *, g_CurrentEntity)[0xBE] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBF] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBD] = 0x35;
        LIT(u8 *, g_CurrentEntity)[0x8C] = 0;
        *(u16 *)(LIT(u8 *, g_CurrentEntity) + 0xC2) = 1000;
    case 1:
        e1 = LIT(u8 *, g_CurrentEntity);
        ENT(e1)->move_speed_current -= ENT(e1)->animation_frame_id * 15;
        joint_move_l(0, ENT(e1)->anim_header, ENT(e1)->anim_base, 0x400);
        entity_add_speed_xz(0x800);
        save = *(VECTOR *)(LIT(u8 *, g_CurrentEntity) + 0x34);
        e2 = LIT(u8 *, g_CurrentEntity);
        LIT(s32, sp_tmp00) = check_room_collision_l(&ENT(e2)->pos_x, *(u16 *)(((u8 *) ENT(e2)->sca_info) + 0xA));
        *(VECTOR *)(LIT(u8 *, g_CurrentEntity) + 0x34) = save;
        if (LIT(s32, sp_tmp00) != 0) {
            LIT(u8 *, g_CurrentEntity)[0x87]++;
            voice_command_l(1, 0xA9);
            LIT(s32, g_GameStateFlags) |= GAME_VOICE_PLAYING;
        }
        break;
    case 2:
        ENT(e)->action_state++;
        LIT(u8 *, g_CurrentEntity)[0xBE] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBF] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBD] = 0x36;
        LIT(u8 *, g_CurrentEntity)[0x8C] = 3;
        sound_play_se_3d_l(2, 0x1C, 0, LIT(u8 *, g_CurrentEntity) + 0x34);
    case 3:
        e3 = LIT(u8 *, g_CurrentEntity);
        LIT(u8 *, g_CurrentEntity)[0x87] += joint_move_l(0, ENT(e3)->anim_header, ENT(e3)->anim_base, 0x400);
        entity_update_weapon_joint_l(0);
        break;
    }
}

void npc_idle_walk_02(void) {
    extern MATRIX *g_DeadMoveValue_MATRIXp __asm__("g_DeadMoveValue");
    void effect_create_billboard_l() __asm__("effect_create_billboard");
    s32 joint_move_l(s32, s32, s32, s32) __asm__("joint_move");
    void sound_play_se_3d_l() __asm__("sound_play_se_3d");
    u8 *e;
    u8 *obj;
    u8 *ent;

    switch (ENT(g_CurrentEntity)->action_state) {
    case 0:
        LIT(u8 *, g_CurrentEntity)[0x87]++;
        LIT(u8 *, g_CurrentEntity)[0xBE] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBF] = 0;
        LIT(u8 *, g_CurrentEntity)[0xBD] = 0x33;
        LIT(u8 *, g_CurrentEntity)[0xBC] = 0xB4;
        LIT(u8 *, g_CurrentEntity)[0x8C] = 3;
        obj = *(u8 **)(LIT(u8 *, g_CurrentEntity) + 0x98);
        obj[0x7C] |= 8;
        LIT(VECTOR, sp_vec) = *(VECTOR *)LIT(MATRIX *, g_DeadMoveValue)->t;
        effect_create_billboard_l(0, 3, 0, obj + 0xC0, LITP(void, sp_vec), 0);
        effect_create_billboard_l(0, 3, 0, LIT(MATRIX *, g_DeadMoveValue), obj + 0xD4, 0);
    case 1:
        if (LIT(u8 *, g_CurrentEntity)[0xBE] < 10) {
            LIT(VECTOR, sp_vec) = *(VECTOR *)g_DeadMoveValue_MATRIXp->t;
            LIT(s32, sp_vec_vy) = -0x898;
            effect_create_billboard_l(0, 0, 0, LIT(u8 *, g_CurrentEntity) + 0x20, LITP(void, sp_vec), 0);
        }
        if (LIT(u8 *, g_CurrentEntity)[0xBE] == 3) {
            obj = *(u8 **)(LIT(u8 *, g_CurrentEntity) + 0x98);
            joint_apply_color_tint(obj, 0x30, 0x80820, 0x606060);
            joint_apply_color_tint(obj + 0x45C, 0x30, 0x80820, 0x606060);
            joint_apply_color_tint(obj + 0x5D0, 0x30, 0x80820, 0x606060);
        }
        if (LIT(u8 *, g_CurrentEntity)[0xBE] == 0x2A) {
            sound_play_se_3d_l(2, 0x2F, 0, LIT(u8 *, g_CurrentEntity) + 0x34);
        }
        LIT(u8 *, g_CurrentEntity)[0x87] += joint_move_l(0, *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x90), *(s32 *)(LIT(u8 *, g_CurrentEntity) + 0x94), 0x400);
        entity_update_weapon_joint(0);
        break;
    case 2:
        LIT(s16, sp_svec_vx) = -0x384;
        SCRATCH->sv_y = 0;
        SCRATCH->sv_z = 0;
        LIT(MATRIX, sp_matrix) = *g_DeadMoveValue_MATRIXp;
        RotMatrixY(*(s16 *)(LIT(u8 *, g_CurrentEntity) + 0x74), LITP(MATRIX, sp_matrix));
        ApplyMatrixSV(LITP(MATRIX, sp_matrix), LITP(SVECTOR, sp_svec), LITP(SVECTOR, sp_svec));
        e = LIT(u8 *, g_CurrentEntity);
        FIELD_AT(e, Entity, ->shadow.offset_x, u16) += LIT(u16, sp_svec_vx);
        FIELD_AT(e, Entity, ->shadow.offset_y, u16) += ((u16) SCRATCH->sv_y);
        FIELD_AT(e, Entity, ->shadow.offset_z, u16) += ((u16) SCRATCH->sv_z);
        billboard_set_color((u8 *)&ENT(e)->shadow, 1, 2, 0xFFFF50);
        billboard_adj_size(LIT(u8 *, g_CurrentEntity) + 0xE4, -0xC8, -0xC8);
        LIT(u8 *, g_CurrentEntity)[0x87]++;
        *(u16 *)(LIT(u8 *, g_CurrentEntity) + 0xC4) = 0x1E;
        break;
    case 3:
        billboard_adj_size(LIT(u8 *, g_CurrentEntity) + 0xE4, 0x10, 0x10);
        ent = LIT(u8 *, g_CurrentEntity);
        if (--FIELD_AT(ent, Entity, ->action_ticks_counter, s16) == 0) {
            ENT(ent)->action_state++;
        }
        break;
    case 4:
        break;
    }
}

void npc_idle_walk_03(void) {
    void effect_create_billboard_l() __asm__("effect_create_billboard");
    s32 joint_move_l() __asm__("joint_move");
    void billboard_set_color_l() __asm__("billboard_set_color");
    void billboard_adj_size_l() __asm__("billboard_adj_size");
    void *p;
    void *q;
    void *r;
    void *s;
    void *t;
    s32 m;

    p = LIT(void *, g_CurrentEntity);
    switch (FIELD_AT(p, Entity, ->action_state, u8)) {
    case 0:
        FIELD_AT(p, Entity, ->action_state, u8) = 1;
        CURRENT_ENTITY_LIT->animation_frame_id = 0;
        CURRENT_ENTITY_LIT->timing_control = 0;
        CURRENT_ENTITY_LIT->animation_id = 0x30;
        CURRENT_ENTITY_LIT->blend_counter = 0;
        q = LIT(void *, g_CurrentEntity);
        FIELD_AT(q, Entity, ->action_ticks_counter, u16) = 0xFA;
        FIELD_AT(q, Entity, ->hit_state, u8) = 0x80;
        CURRENT_ENTITY_LIT->status_flags |= 6;
        CURRENT_ENTITY_LIT->angle = g_EnemySlot1_Angle;
    case 1:
        if (CURRENT_ENTITY_LIT->animation_frame_id == 8) {
            m = CURRENT_ENTITY_LIT->joints;
            joint_apply_color_tint(m, 0x30, 0x80820, 0x606060);
            joint_apply_color_tint(m + 0x7C, 0x30, 0x80820, 0x606060);
            joint_apply_color_tint(m + 0xF8, 0x30, 0x80820, 0x606060);
            joint_apply_color_tint(m + 0x45C, 0x30, 0x80820, 0x606060);
            joint_apply_color_tint(m + 0x5D0, 0x30, 0x80820, 0x606060);
        }
        if (CURRENT_ENTITY_LIT->animation_frame_id < 9) {
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            LIT(s32, sp_vec_vy) = -0x5DC;
            effect_create_billboard_l(0, 0, 0, (u8 *)LIT(void *, g_CurrentEntity) + 0x20, ADDR_sp_vec, 0);
        }
        if (CURRENT_ENTITY_LIT->animation_frame_id >= 0x60) {
            LIT(VECTOR, sp_vec) = FIELD_AT(g_DeadMoveValue, MATRIX, ->t, VECTOR);
            effect_create_billboard_l(0, 0, 0, (u8 *)LIT(void *, g_CurrentEntity) + 0x20, ADDR_sp_vec, 0);
        }
        s = LIT(void *, g_CurrentEntity);
        entity_apply_anim_vertex(s, FIELD_AT(s, Entity, ->anim_header, s32), FIELD_AT(s, Entity, ->anim_base, s32));
        t = LIT(void *, g_CurrentEntity);
        if (joint_move_l(0, FIELD_AT(t, Entity, ->anim_header, s32), FIELD_AT(t, Entity, ->anim_base, s32), 0x400) != 0) {
            CURRENT_ENTITY_LIT->action_state++;
            billboard_set_color_l(&CURRENT_ENTITY_LIT->shadow, 1, 2, 0xFFFF50);
            billboard_adj_size_l(&CURRENT_ENTITY_LIT->shadow, -0x64, -0x64);
        }
        break;
    case 2:
        FIELD_AT(p, Entity, ->status_flags, u8) &= 0xFD;
        q = LIT(void *, g_CurrentEntity);
        FIELD_AT(q, Entity, ->health, s16) = -1;
        billboard_adj_size_l((u8 *)q + 0xE4, 6, 6);
        r = LIT(void *, g_CurrentEntity);
        if (--FIELD_AT(r, Entity, ->action_ticks_counter, s16) == 0) {
            FIELD_AT(r, Entity, ->action_state, u8)++;
        }
        break;
    }
}
