/* interactive_screen.c - Screens opened by rooms: the dispatcher and the passcode panel (the computer-lab
   terminal is in computer_lab_terminal.c + computer_lab.c). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void check_camera_switch(s32 refresh);
void display_room_camera_bg(void);
void map_set_area_known(s32 map_id);
void print_text_8x14(s32 x, s32 y, s32 attr, u8 semi_trans);
s32 set_message_display(u8 msg, s16 msg_id);
void room_spr_set_inactive(u8 sprite_id);
void room_spr_set_active(u8 sprite_id);
void Flg_on(s32 flags, u32 bit);
void Flg_off(s32 flags, u32 bit);
s32 Flg_ck(s32 *flags, u32 bit);


void interactive_screen_reset(void) {
    g_InteractiveScreenState = 0;
    g_LabSlidesState = 0;
}

void check_and_display_interactive_screen(void) {
    s32 Flg_ck_a10() __asm__("Flg_ck");
    void display_passcode_panel(void);
    s32 display_computer_lab();
    s32 lab_slides_display();
    extern s32 *g_InteractiveScreenStatePtr_s32p __asm__("g_InteractiveScreenStatePtr");
    g_InteractiveScreenStatePtr_s32p = &g_InteractiveScreenState;
    if (Flg_ck_a10(ADDR_g_ScenarioFlags, 0x20) != 0) {
        if (g_LabSlidesState == 0) {
            *g_InteractiveScreenStatePtr_s32p = 0;
        }
        if (Flg_ck_a10(ADDR_g_SysFlags, 0x1D) != 0) {
            display_passcode_panel();
        } else if (Flg_ck_a10(ADDR_g_SysFlags, 0x1E) != 0) {
            display_computer_lab();
        } else if (Flg_ck_a10(ADDR_g_SysFlags, 0x1F) != 0) {
            lab_slides_display();
        }
    }
    g_LabSlidesState = Flg_ck_a10(ADDR_g_ScenarioFlags, 0x20);
}

void display_passcode_panel(void) {
    s32 passcode_panel_draw_a1() __asm__("passcode_panel_draw");
    void passcode_panel_update_room_sprites(void);
    extern s32 (*passcode_panel_state_tbl[])();
    extern u8 *g_InteractiveScreenStatePtr_u8p __asm__("g_InteractiveScreenStatePtr");
    passcode_panel_state_tbl[*g_InteractiveScreenStatePtr_u8p]();
    passcode_panel_draw_a1();
    passcode_panel_update_room_sprites();
}

void passcode_panel_init(void) {
    s32 display_room_camera_bg_p() __asm__("display_room_camera_bg");
    s32 set_message_display_a9() __asm__("set_message_display");
    s32 Flg_ck_a11() __asm__("Flg_ck");
    extern u8 g_PasscodePanelTiles_u8[] __asm__("g_PasscodePanelTiles");
    extern s16 g_InteractiveScreenSavedCut_s16 __asm__("g_InteractiveScreenSavedCut");
    /* g_PasscodePanelInitStates[1..3] as symbols of their own: the -O2 code loads each address separately
       (with g_PasscodePanelInitStates[n] GCC derives them from the [0] pointer) */
    extern u8 g_PasscodePanelInitStates_1[] __asm__("g_PasscodePanelInitStates+12");
    extern u8 g_PasscodePanelInitStates_2[] __asm__("g_PasscodePanelInitStates+24");
    extern u8 g_PasscodePanelInitStates_3[] __asm__("g_PasscodePanelInitStates+36");
    u8 i;
    u8 *s;

    ISCREEN->state = (u8) (ISCREEN->state + 1);
    ISCREEN->substate = 0;
    g_InteractiveScreenSavedCut_s16 = LIT(u8, g_CutId);
    LIT(u8, g_CutId) = 5;
    s = g_PasscodePanelInitStates[0];
    display_room_camera_bg_p();
    LIT(u16, g_UpdateFlags) &= 0xFEB0;
    ISCREEN->cursor_col = 0;
    ISCREEN->cursor_row = 0;
    if (Flg_ck_a11(ADDR_g_ScenarioFlags, 0x1E) != 0) {
        if (g_PlayerEntity.id == 1) {
            s = g_PasscodePanelInitStates_2;
        }
    } else if (Flg_ck_a11(ADDR_g_ScenarioFlags, 0x1F) != 0) {
        s = g_PasscodePanelInitStates_1;
        if (g_PlayerEntity.id == 1) {
            s = g_PasscodePanelInitStates_3;
        }
    }
    for (i = 0; i < 9; i++) {
        g_PasscodePanelTiles_u8[i] = *s++;
    }
    g_PasscodePanelActive = 0;
    set_message_display_a9(0xE, 0xFF);
}

void passcode_panel_update(void) {
    extern s32 (*passcode_panel_update_tbl[])();
    passcode_panel_update_tbl[ISCREEN->substate]();
}

void passcode_panel_wait_message(void) {
    if (!(LIT(u8, g_MessageStatus) & 0x80)) {
        if (LIT(u8, g_MessageStatus) != 0) {
            ISCREEN->state = 2;
            ISCREEN->substate = 0;
            return;
        }
        ISCREEN->substate = 1;
        g_PasscodePanelActive = 1;
    }
}

void passcode_panel_input(void) {
    s32 row_up;
    s32 col_left;
    s32 row_down;
    s32 col_right;

    if (g_PadTrigger & PSX_UP) {
        row_up = ISCREEN->cursor_row;
        if (row_up > 0) {
            ISCREEN->cursor_row = (s32) (row_up - 1);
        }
    }
    if (g_PadTrigger & PSX_DOWN) {
        row_down = ISCREEN->cursor_row;
        if (row_down < 2) {
            ISCREEN->cursor_row = (s32) (row_down + 1);
        }
    }
    if (g_PadTrigger & PSX_LEFT) {
        col_left = ISCREEN->cursor_col;
        if (col_left > 0) {
            ISCREEN->cursor_col = (s32) (col_left - 1);
        }
    }
    if (g_PadTrigger & PSX_RIGHT) {
        col_right = ISCREEN->cursor_col;
        if (col_right < 2) {
            ISCREEN->cursor_col = (s32) (col_right + 1);
        }
    }
    if (LIT(u16, g_PadPressed) & PAD_CONFIRM) {
        ISCREEN->substate = (u8) (ISCREEN->substate + 1);
    }
    if (LIT(u16, g_PadPressed) & PAD_CANCEL) {
        ISCREEN->state = (u8) (ISCREEN->state + 1);
        ISCREEN->substate = 0U;
    }
}

void passcode_panel_toggle(void) {
    extern u8 g_PasscodePanelTiles_u8[9] __asm__("g_PasscodePanelTiles");
    extern u8 *g_InteractiveScreenStatePtr_u8p __asm__("g_InteractiveScreenStatePtr");
    u16 x;
    u16 y;
    u16 i;
    u16 sum;

    sum = 0;
    play_sfx(2, 0x17, 0);
    y = ISCREEN_B_FIELD_AS(cursor_row, u16);
    x = ISCREEN_B_FIELD_AS(cursor_col, u16);
    g_PasscodePanelTiles_u8[y * 3 + x] ^= 1;
    if (y != 0) {
        g_PasscodePanelTiles_u8[(y - 1) * 3 + x] ^= 1;
    }
    if (y < 2) {
        g_PasscodePanelTiles_u8[(y + 1) * 3 + x] ^= 1;
    }
    if (x != 0) {
        g_PasscodePanelTiles_u8[y * 3 + (x - 1)] ^= 1;
    }
    if (x < 2) {
        g_PasscodePanelTiles_u8[y * 3 + (x + 1)] ^= 1;
    }
    for (i = 0; i < 9; i++) {
        sum += g_PasscodePanelTiles_u8[i];
    }
    if (sum == 9) {
        ISCREEN_B->substate = 3;
        ISCREEN_B->step = 0;
        g_PasscodePanelActive = 0;
        ISCREEN_B->timer = 30;
    } else {
        ISCREEN_B->substate = 1;
    }
}

void passcode_panel_animation_update(void) {
    extern s32 (*passcode_panel_anim_tbl[])();
    passcode_panel_anim_tbl[ISCREEN->step]();
}

void passcode_panel_animation_start(void) {
    u16 ticks;

    ticks = ISCREEN->timer - 1;
    ISCREEN->timer = ticks;
    if ((ticks << 0x10) == 0) {
        ISCREEN->step = (u8) (ISCREEN->step + 1);
        ISCREEN->timer = 1U;
        ISCREEN->counter = 0;
    }
}

void passcode_panel_animation_step(void) {
    u16 ticks;
    u16 tile_pos;
    u16 delay_pos;
    u8 delay;

    ticks = ISCREEN->timer - 1;
    ISCREEN->timer = ticks;
    if ((ticks << 0x10) == 0) {
        tile_pos = ISCREEN->counter;
        ISCREEN->counter = (u16) (tile_pos + 1);
        *(&g_PasscodePanelTiles + *(&g_PasscodePanelAnimTiles + (s16) tile_pos)) = 0;
        delay_pos = ISCREEN->counter;
        ISCREEN->counter = (u16) (delay_pos + 1);
        delay = *(&g_PasscodePanelAnimTiles + (s16) delay_pos);
        ISCREEN->timer = (u16) delay;
        if (delay == 0) {
            ISCREEN->step = (u8) (ISCREEN->step + 1);
            ISCREEN->timer = 0x1EU;
        }
    }
}

void passcode_panel_animation_finish(void) {
    u16 ticks;

    ticks = ISCREEN->timer - 1;
    ISCREEN->timer = ticks;
    if ((ticks << 0x10) == 0) {
        ISCREEN->state = (u8) (ISCREEN->state + 1);
        play_sfx(2, 0x18, 0);
        Flg_on(ADDR_g_ScenarioFlags, 0x21);
    }
}

void passcode_panel_finish(void) {
    LIT(u16, g_UpdateFlags) |= 0x14F;
    g_CutId = g_InteractiveScreenSavedCut;
    display_room_camera_bg();
    g_PasscodePanelActive = 0;
    Flg_off(ADDR_g_ScenarioFlags, 0x20);
}

void passcode_panel_draw(void) {
    extern u8 g_PasscodePanelActive_u8 __asm__("g_PasscodePanelActive");
    int sprintf();
    s32 x;
    s32 y;
    s32 c;
    s32 k;
    s32 k2;
    s32 y2;
    s32 k3;
    u16 i;
    u16 j;

    if (g_PasscodePanelActive_u8 != 0) {
        sprintf(ADDR_sp_text, "$$$$$$$$$");
        x = j * 8 + 0x7B;
        print_text_8x14(x, 0x8C, 0, 1);
        print_text_8x14(x, 0xC4, 0, 1);
        for (i = 0; i < 3; i++) {
            sprintf(ADDR_sp_text, "$$");
            k = i;
            y = (k + 1) * 0xE;
            print_text_8x14(0x7B, y + 0x8C, 0, 1);
            sprintf(ADDR_sp_text, "$");
            print_text_8x14(0xBB, y + 0x8C, 0, 1);
            j = 0;
            k2 = k;
            k3 = k2 * 3;
            y2 = y;
            for (; j < 3; j++) {
                c = 3;
                if (ISCREEN->cursor_col == j) {
                    if (ISCREEN->cursor_row == k2) {
                        c = 1;
                    }
                }
                sprintf(ADDR_sp_text, "%c$", k3 + j + 0x31);
                print_text_8x14(j * 0x10 + 0x8B, y2 + 0x8C, c, 1);
            }
        }
    }
}

void passcode_panel_update_room_sprites(void) {
    s32 room_spr_set_inactive_a2(s32) __asm__("room_spr_set_inactive");
    s32 room_spr_set_active_a2(s32) __asm__("room_spr_set_active");
    s32 spr_id;
    u8 *tile;

    tile = &g_PasscodePanelTiles;
    spr_id = 1;
    do {
        if (*tile++ != 0) {
            room_spr_set_active_a2(spr_id & 0xFF);
            spr_id += 1;
        } else {
            room_spr_set_inactive_a2(spr_id & 0xFF);
            spr_id += 1;
        }
    } while ((u32) (spr_id & 0xFF) < 0xAU);
}
