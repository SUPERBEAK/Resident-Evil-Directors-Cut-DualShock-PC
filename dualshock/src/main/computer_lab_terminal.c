/* computer_lab_terminal.c - The computer-lab terminal's state machine: boot, login, menus and commands
   (its drawing and animation is in computer_lab.c). */

#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void check_camera_switch(s32 refresh);

void map_set_area_known(s32 map_id);

void Flg_on(s32 flags, u32 bit);

void Flg_off(s32 flags, u32 bit);

s32 Flg_ck(s32 *flags, u32 bit);

void display_computer_lab(void) {
    s32 computer_draw_windows_a1() __asm__("computer_draw_windows");
    s32 computer_draw_status();
    s32 computer_draw_leds();
    s32 computer_camera_step();
    extern s32 (*g_ComputerLabFuncs[])();
    extern u8 *g_InteractiveScreenStatePtr_u8p __asm__("g_InteractiveScreenStatePtr");
    g_ComputerLabFuncs[*g_InteractiveScreenStatePtr_u8p]();
    computer_draw_caret();
    computer_draw_text();
    computer_draw_windows_a1();
    computer_draw_keyboard();
    computer_draw_door_picture();
    computer_draw_status();
    computer_draw_leds();
    computer_camera_step();
}

typedef struct {
    u8 b;
    u8 pad[0x1B];
} RecB1C_h17;
typedef struct {
    s16 h;
    u8 pad[0x1A];
} RecH1C_h17;
typedef struct {
    u8 b;
    u8 pad[0x11];
} RecB12_h17;

void computer_lab_init(void) {
    void GsSetAmbient(long r, long g, long b);
    extern s32 *g_InteractiveScreenStatePtr_l __asm__("g_InteractiveScreenStatePtr");
    extern RoomLight g_ClSavedRoomLights[3];   /* the room's own lights while the terminal is up */
    extern RoomLight g_ClLabLights[3];         /* the terminal's lights */
    extern RecB1C_h17 g_ClWindows_l[] __asm__("g_ClWindows");
    extern RecB1C_h17 g_ClWindow0AnimCmd_l[] __asm__("g_ClWindow0AnimCmd");
    extern RecH1C_h17 g_ClWindow0Alpha_l[] __asm__("g_ClWindow0Alpha");
    extern RecH1C_h17 g_ClWindow0Big_l[] __asm__("g_ClWindow0Big");
    extern RecB12_h17 g_ClTextFields_l[] __asm__("g_ClTextFields");
    u8 *p;
    u8 i;

    *g_InteractiveScreenStatePtr_l = 1;
    g_InteractiveScreenSavedCut = LIT(u8, g_CutId);
    LIT(u8, g_CutId) = 4;
    check_camera_switch(1);
    LIT(u16, g_UpdateFlags) &= 0xFEF8;
    p = (u8 *)(g_InteractiveScreenStatePtr_l + 3);
    *(s32 *)p = g_PlayerEntity.pos_y;
    g_PlayerPosY = -9000;
    g_ClSavedRoomLights[0] = *(RoomLight *)(LIT(u8 *, g_RoomData) + 0xC);
    g_ClSavedRoomLights[1] = *(RoomLight *)(LIT(u8 *, g_RoomData) + 0x20);
    g_ClSavedRoomLights[2] = *(RoomLight *)(LIT(u8 *, g_RoomData) + 0x34);
    g_ClSavedAmbientR = *(u16 *)(LIT(u8 *, g_RoomData) + 6);
    g_ClSavedAmbientG = *(u16 *)(LIT(u8 *, g_RoomData) + 8);
    g_ClSavedAmbientB = *(u16 *)(LIT(u8 *, g_RoomData) + 0xA);
    *(RoomLight *)(LIT(u8 *, g_RoomData) + 0xC) = g_ClLabLights[0];
    *(RoomLight *)(LIT(u8 *, g_RoomData) + 0x20) = g_ClLabLights[1];
    *(RoomLight *)(LIT(u8 *, g_RoomData) + 0x34) = g_ClLabLights[2];
    p = LIT(u8 *, g_RoomData);
    ROOM_HDR(p)->ambient_r = 1800;
    ROOM_HDR(p)->ambient_g = 1800;
    ROOM_HDR(p)->ambient_b = 1800;
    GsSetAmbient(ROOM_HDR(p)->ambient_r, ROOM_HDR(p)->ambient_g, 1800);
    g_ClDoorSel = 0;
    g_ClInputEnabled = 0;
    for (i = 0; i < 4; i++) {
        g_ClWindows_l[i].b = 0;
        g_ClWindow0AnimCmd_l[i].b = 0;
        g_ClWindow0Alpha_l[i].h = 0;
        g_ClWindow0Big_l[i].h = 0;
    }
    g_ClKeyboardBig = 1;
    for (i = 0; i < 2; i++) {
        g_ClTextFields_l[i].b = 0;
    }
    g_ClColorR = g_ClColorG = g_ClColorB = 0x80;
    g_EnemySlot0_BehaviorFlags = 0x40;
    g_EnemySlot1_BehaviorFlags = 0x40;
}

void computer_lab_update(void) {
    void cl_windows_update(void);
    s32 computer_typer_update();
    extern s32 (*g_ClStages[])();
    g_ClStages[ISCREEN->substate]();
    cl_windows_update();
    computer_typer_update();
}

void cl_stage_boot(void) {
    void cl_boot_step_and_draw_monitor(void);
    void cl_boot_logo(void);
    if (ISCREEN->step == 0) {
        cl_boot_step_and_draw_monitor();
        return;
    }
    cl_boot_logo();
}

void cl_boot_step_and_draw_monitor(void) {
    extern s32 (*cl_boot_step_tbl[])();
    void GsSortSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    cl_boot_step_tbl[ISCREEN->substep]();
    LIT(s16, sp_sprite_tpage) = 7;
    LIT(s16, sp_sprite_cy) = 0x1EB;
    LIT(s16, sp_sprite_mx) = 0x3C;
    LIT(s16, sp_sprite_my) = 0x34;
    LIT(s16, sp_sprite_y) = -0x2B;
    LIT(s8, sp_sprite_v) = 0x68;
    LIT(s16, sp_sprite_w) = 0x78;
    LIT(s16, sp_sprite_cx) = 0;
    LIT(s32, sp_sprite_rotate) = 0;
    LIT(s16, sp_sprite_x) = 0;
    LIT(s8, sp_sprite_u) = 0;
    LIT(s16, sp_sprite_h) = 0x68;
    GsSortSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)((g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables), 3U);
}

void cl_boot_zoom_start(void) {
    extern u8 g_ClMsgFlags_u8 __asm__("g_ClMsgFlags");
    ISCREEN->substep = (u8) (ISCREEN->substep + 1);
    g_ClLedFlags = 1;
    LIT(s16, sp_sprite_scaley) = 0x64;
    LIT(s16, sp_sprite_scalex) = 0;
    g_ClMsgFlags_u8 = 0x83;
    ISCREEN->timer = 0;
}

void cl_boot_zoom_wide(void) {
    ISCREEN_FIELD(timer) = (u16) (ISCREEN_FIELD(timer) + 0x800);
    LIT(s32, sp_sprite_attribute) = 0x01000000;
    LIT(u8, sp_sprite_r) = 0x80;
    LIT(u8, sp_sprite_g) = 0x80;
    LIT(u8, sp_sprite_b) = 0x80;
    LIT(s16, sp_sprite_scaley) = 0x64;
    LIT(u16, sp_sprite_scalex) = ISCREEN->timer;
    if ((s16) ISCREEN_FIELD(timer) >= 0x1000) {
        ISCREEN->substep = (u8) (ISCREEN->substep + 1);
        ISCREEN_FIELD(timer) = 0x64U;
    }
}

void cl_boot_zoom_tall(void) {
    u16 scale_y;

    scale_y = ISCREEN_FIELD(timer) + 0x800;
    ISCREEN_FIELD(timer) = scale_y;
    if ((s16) scale_y >= 0x1001) {
        ISCREEN_FIELD(timer) = 0x1000U;
        ISCREEN_FIELD_AS(counter, s16) = 0x5A;
        ISCREEN->substep = (u8) (ISCREEN->substep + 1);
        g_ClMsgFlags = 0;
    }
    LIT(s32, sp_sprite_attribute) = 0x01000000;
    LIT(s16, sp_sprite_scalex) = 0x1000;
    LIT(u16, sp_sprite_scaley) = ISCREEN_FIELD(timer);
}

void cl_boot_flash(void) {
    void play_sfx_a8(s32, s32, s32) __asm__("play_sfx");
    extern u8 *g_InteractiveScreenStatePtr_u8p __asm__("g_InteractiveScreenStatePtr");
    s32 *s = LITP(s32, sp_sprite);
    u8 *p = g_InteractiveScreenStatePtr_u8p;
    u8 *q;
    s32 v;
    s16 t;

    (FIELD_AT(p, InteractiveScreen, ->counter, s16))--;
    v = 0x41000000;
    if (FIELD_AT(p, InteractiveScreen, ->counter, s16) & 1) {
        v = 0x01000000;
    }
    *s = v;
    LIT(u8, sp_sprite_r) = FIELD_AT(p, InteractiveScreen, ->counter, s16) / 2;
    LIT(u8, sp_sprite_g) = FIELD_AT(p, InteractiveScreen, ->counter, s16) / 2;
    t = FIELD_AT(p, InteractiveScreen, ->counter, s16);
    LIT(s16, sp_sprite_scalex) = 0x1000;
    LIT(s16, sp_sprite_scaley) = 0x1000;
    LIT(u8, sp_sprite_b) = t / 2;
    if (FIELD_AT(p, InteractiveScreen, ->counter, s16) == 0x3C) {
        play_sfx_a8(2, 0x17, 0);
    }
    q = g_InteractiveScreenStatePtr_u8p;
    if (FIELD_AT(q, InteractiveScreen, ->counter, s16) < 0x1E) {
        g_ClLedFlags = 3;
    }
    if (FIELD_AT(q, InteractiveScreen, ->counter, s16) == 0) {
        g_ClLedFlags = 1;
        g_ClIntroLogoState = 0;
        ISCR(q)->step++;
        ISCREEN_B->substep = 0;
        if ((g_PadHeld & PAD_CONFIRM) || (g_PadHeld & PAD_CANCEL)) {
            g_ClMsgFlags = 1;
            ISCREEN_B->substep = 1;
            ISCREEN_B->timer = 0x1E;
        }
    }
}

void cl_boot_logo(void) {
    s32 computer_fade_step(s32, s32);
    s32 computer_logo_update(void *);
    u16 ticks;
    u16 ticks2;
    u8 sub;

    sub = ISCREEN->substep;
    switch (sub) {
    case 0:
        computer_logo_update(g_InteractiveScreenStatePtr);
        if (g_ClIntroLogoState == 2) {
            g_ClMsgFlags = 1;
            ISCREEN->substep = (u8) (ISCREEN->substep + 1);
            ISCREEN->timer = 0x1EU;
            return;
        }
        return;
    case 1:
        ticks = ISCREEN->timer - 1;
        ISCREEN->timer = ticks;
        if ((ticks << 0x10) == 0) {
            ISCREEN->substep = (u8) (ISCREEN->substep + 1);
            ISCREEN->timer = 0x1EU;
            ISCREEN->counter = 0;
            return;
        }
        break;
    case 2:
        if (computer_fade_step(8, 2) & 0xFF) {
            ticks2 = ISCREEN->timer - 1;
            ISCREEN->timer = ticks2;
            if ((ticks2 << 0x10) == 0) {
                ISCREEN->substate = (u8) (ISCREEN->substate + 1);
                ISCREEN->step = 0;
            }
        }
        break;
    }
}

void cl_stage_login(void) {
    extern s32 (*cl_login_stage_tbl[])();
    cl_login_stage_tbl[ISCREEN->step]();
}

void cl_login_init(void) {
    ISCREEN_FIELD(step) = (u8) (ISCREEN_FIELD(step) + 1);
    ISCREEN->substep = 0;
    computer_window_setup(0, -0x90, -0x68, 0x10, 4, 3, &g_ClRowsLogin);
    g_ClWindows = 1;
    g_ClWindow0AnimCmd = 1;
    g_ClKeyboardAnimCmd = 1;
    g_ClKeyboardFlags = 1;
    g_ClText0X = 7;
    g_ClPadRepeatTimer = 0;
    g_ClRowsLogin[2].width = 0;
    g_ClTextFields = 0;
    g_ClText0WinIdx = 0;
    g_ClText0Y = 2;
    g_ClText0Masked = 0;
    g_ClText1Enabled = 0;
    computer_text_begin(0);
    g_ClPendingCamera = 0x85;
    g_ClStatusIdx = 0;
    g_ClPendingCamDelay = 0;
    ISCREEN_FIELD(cursor_col) = 1;
    ISCREEN_FIELD(cursor_row) = 0;
    g_EnemySlot0_BehaviorFlags = 0x47;
    g_EnemySlot1_BehaviorFlags = 0x47;
}

void cl_login_steps_input(void) {
    extern s32 (*cl_login_input_step_tbl[])();
    cl_login_input_step_tbl[ISCREEN->substep]();
}

void cl_login_wait_open(void) {
    if ((g_ClWindow0AnimCmd == 0) && (g_ClKeyboardAnimCmd == 0)) {
        ISCREEN->substep = (u8) (ISCREEN->substep + 1);
        ISCREEN_FIELD_AS(timer, s16) = 0;
        g_ClTextFields = 1;
        g_ClInputEnabled = 1;
        g_ClCaretMode = 0;
    }
}

void cl_login_typing(void) {
    s16 key;
    s8 *cancelled = &g_ClCancelled;

    (*cancelled) = 0;
    key = computer_keyboard_input();
    if (key == -1) {
        ISCREEN->substep = (u8) (ISCREEN->substep + 1);
        return;
    }
    if ((key == -2) || (g_PadPressed & PAD_CANCEL)) {
        ISCREEN->substep = (u8) (ISCREEN->substep + 1);
        (*cancelled) = 1;
        return;
    }
    if (key != 0) {
        computer_text_append(key, (((s16) ISCREEN->timer) * 0x12) + g_ClText0Buf);
        if (g_ClCaretPos != 7) {
            g_ClBlinkCounter = 0;
        }
    }
}

void cl_login_to_password(void) {
    if (g_ClCancelled != 0) {
        ISCREEN_FIELD_AS(state, s32) = 2;
        return;
    }
    ISCREEN->substep = (u8) (ISCREEN->substep + 1);
    g_ClText1Enabled = 1;
    g_ClRowsLogin[2].width = 0xC;
    g_ClText1Masked = 1;
    g_ClText1X = 0xC;
    g_ClText1Y = 3;
    g_ClText1WinIdx = 0;
    ISCREEN->timer = 1;
    computer_text_begin(1);
}

void cl_login_submit() {
    if (g_ClCancelled != 0) {
        ISCREEN_FIELD_AS(state, s32) = 2;
        return;
    }
    { s32 wt = (u8) (ISCREEN->substep + 1); ISCREEN->substep = wt; }
    g_ClWindow0AnimCmd = 0x81;
    g_ClKeyboardAnimCmd = 0x81;
    g_EnemySlot0_BehaviorFlags = 0x40;
    g_EnemySlot1_BehaviorFlags = 0x40;
    g_ClMsgFlags = 2;
    g_ClInputEnabled = 0;
    g_ClPendingCamDelay = 0;
    g_ClPendingCamera = 0x84;
}

void cl_login_wait_close(void) {
    if ((g_ClWindow0AnimCmd == 0) && (g_ClKeyboardAnimCmd == 0)) {
        ISCREEN->step = (u8) (ISCREEN->step + 1);
        ISCREEN->substep = 0;
        g_ClTextFields = 0;
        g_ClText1Enabled = 0;
        g_ClWindows = 0;
        g_ClKeyboardFlags = 0;
    }
}

void cl_login_steps_check(void) {
    extern s32 (*cl_login_check_step_tbl[])();
    cl_login_check_step_tbl[ISCREEN->substep]();
}

void cl_login_type_line(void) {
    ISCREEN->substep = (u8) (ISCREEN->substep + 1);
    g_ClTyper = 1;
    g_ClTyperLine = 0;
}

void cl_login_next_line(void) {
    u16 line;
    u8 *typer = &g_ClTyper;

    if ((*typer) == 0) {
        line = g_ClTyperLine + 1;
        g_ClTyperLine = line;
        if ((s16) line == 2) {
            ISCREEN->substep = (u8) (ISCREEN->substep + 1);
            return;
        }
        (*typer) = 1;
    }
}

void cl_login_validate(void) {
    int strcmp(const char *a, const char *b);
    if ((strcmp(g_ClText0Buf, &g_ClLoginUser) != 0) || (strcmp(g_ClText0Buf + 0x12, &g_ClLoginPass) != 0)) {
        ISCREEN->substep = 3;
        g_ClStatusIdx = 4;
        ISCREEN->timer = 0x5A;
        play_sfx(2, 0x19, 0);
        return;
    }
    ISCREEN->substep = 4;
    ISCREEN->timer = 0x5A;
}

void cl_login_rejected(void) {
    s16 ticks;
    s16 ticks_left;

    ticks = ((s16) ISCREEN->timer);
    ticks_left = ticks - (ticks > 0);
    ISCREEN->timer = ticks_left;
    if (((ticks_left << 0x10) == 0) && (g_EnemySlot0_BehaviorFlags & 0x20) && (g_EnemySlot1_BehaviorFlags & 0x20)) {
        ISCREEN->step = 0;
        ISCREEN->substep = 0;
    }
    if (((s16) ISCREEN->timer) == 0x4B) {
        g_EnemySlot0_BehaviorFlags = 0x44;
        g_EnemySlot1_BehaviorFlags = 0x44;
    }
}

void cl_login_accepted(void) {
    s16 ticks;
    s16 ticks_left;

    ticks = ((s16) ISCREEN->timer);
    ticks_left = ticks - (ticks > 0);
    ISCREEN->timer = ticks_left;
    if ((ticks_left << 0x10) == 0) {
        ISCREEN->substate = (u8) (ISCREEN->substate + 1);
        ISCREEN->step = 0;
    }
    if (((s16) ISCREEN->timer) >= 0x33) {
        computer_draw_quads(-0x3C, -0x5F, 3, 1, &g_ClStripA, 7);
        return;
    }
    g_ClMsgFlags |= 0x10;
}

void cl_stage_menu(void) {
    void cl_menu_setup(void);
    void cl_menu_input_and_step(void);
    if (ISCREEN->step == 0) {
        cl_menu_setup();
        return;
    }
    cl_menu_input_and_step();
}

void cl_menu_setup(void) {
    ISCREEN->step = (u8) (ISCREEN->step + 1);
    ISCREEN->substep = 0;
    computer_window_setup(1, -0x10, -0x38, 4, 3, 3, &g_ClRowsSide);
    g_ClWindow1Enabled = 0;
    g_ClWindow1AnimCmd = 0;
    computer_window_setup(2, -0x90, -0x68, 0x10, 3, 3, &g_ClRowsPanelA);
    g_ClWindow2Enabled = 1;
    g_ClWindow2AnimCmd = 1;
    g_ClCaretWinIdx = 1;
    g_ClCaretMode = 1;
    g_ClCaretBarWidth = 5;
    g_ClPendingCamera = 0x85;
    g_ClCaretBaseX = 0;
    g_ClCaretBaseY = 0;
    g_ClCaretPos = 0;
    g_ClBlinkCounter = 0;
    g_ClInputEnabled = 0;
    g_ClTextFields = 0;
    g_ClText1Enabled = 0;
    g_ClStatusIdx = 0;
    g_ClPendingCamDelay = 0;
    ISCREEN->timer = 0xC;
    ISCREEN->counter = 2;
}

void cl_menu_input_and_step(void) {
    extern u8 g_ClInputEnabled_u8 __asm__("g_ClInputEnabled");
    extern void (*cl_menu_step_tbl_v[])(void) __asm__("cl_menu_step_tbl");
    extern u8 *g_InteractiveScreenStatePtr_p __asm__("g_InteractiveScreenStatePtr");
    u8 *p;

    if (g_ClInputEnabled_u8 != 0) {
        if ((g_PadTrigger & PSX_UP) && (g_ClCaretPos != 0)) {
            g_ClCaretPos--;
            g_ClBlinkCounter = 0;
        }
        if (g_PadTrigger & PSX_DOWN) {
            p = &g_ClCaretPos;
            if (*p < ((s16) ISCR(g_InteractiveScreenStatePtr_p)->counter)) {
                g_ClBlinkCounter = 0;
                (*p)++;
            }
        }
    }
    cl_menu_step_tbl_v[ISCR(g_InteractiveScreenStatePtr_p)->substep]();
}

void cl_menu_open(void) {
    s16 ticks;
    s16 ticks_left;

    ticks = ((s16) ISCREEN->timer);
    if (ticks != 0) {
        ticks_left = ticks - 1;
        ISCREEN->timer = ticks_left;
        if ((ticks_left << 0x10) == 0) {
            g_ClWindow1Enabled = 1;
            g_ClWindow1AnimCmd = 1;
        }
    } else if ((g_ClWindow0AnimCmd == 0) && (g_ClWindow1AnimCmd == 0)) {
        ISCREEN->substep = (u8) (ISCREEN->substep + 1);
        g_ClInputEnabled = 1;
        g_EnemySlot0_BehaviorFlags = 0x47;
        g_EnemySlot1_BehaviorFlags = 0x47;
    }
}

void cl_menu_select(void) {
    if (g_PadPressed & PAD_CONFIRM) {
        switch (g_ClCaretPos) {
        case 0:
            g_ClDoorSel = 0;
            ISCREEN->substep = 4;
            break;
        case 1:
            g_ClDoorSel = 1;
            ISCREEN->substep = 4;
            break;
        case 2:
            ISCREEN->substep = 2;
            g_ClInputEnabled = 0;
            break;
        }
    }
    if (g_PadPressed & PAD_CANCEL) {
        ISCREEN->substep = 2;
        g_ClInputEnabled = 0;
    }
}

void cl_menu_to_command(void) {
    ISCREEN->substep = (u8) (ISCREEN->substep + 1);
    computer_window_setup(0, 0x10, -0x28, 3, 4, 4, &g_ClRowsMenu);
    g_ClWindows = 1;
    g_ClInputEnabled = 1;
    g_ClWindow0Alpha = 0x80;
    g_ClCaretMode = 1;
    g_ClCaretBaseY = 2;
    g_ClCaretBarWidth = 4;
    g_ClWindow0Visible = 0;
    g_ClCaretWinIdx = 0;
    g_ClCaretBaseX = 0;
    g_ClCaretPos = 0;
    g_ClBlinkCounter = 0;
    ISCREEN_FIELD_AS(counter, s16) = 1;
    g_EnemySlot0_BehaviorFlags = 0x40;
}

void cl_menu_confirm_exit(void) {
    extern u8 *g_InteractiveScreenStatePtr_u8p __asm__("g_InteractiveScreenStatePtr");
    u8 flag;
    u8 *q;

    flag = g_PadPressed >> 15;
    if (g_PadPressed & PAD_CONFIRM) {
        if (g_ClCaretPos == 0) {
            *(s32 *)g_InteractiveScreenStatePtr_u8p = 2;
            return;
        }
        flag = 1;
    }
    if (flag != 0) {
        do {
            flag = 1;
            ISCREEN_B->substep = flag;
            g_ClCaretWinIdx = flag;
            g_ClCaretMode = flag;
            g_ClInputEnabled = flag;
        } while (0);
        q = g_InteractiveScreenStatePtr_u8p;
        g_ClCaretBarWidth = 5;
        g_EnemySlot0_BehaviorFlags = 0x47;
        g_EnemySlot1_BehaviorFlags = 0x47;
        g_ClCaretBaseX = 0;
        g_ClCaretBaseY = 0;
        g_ClCaretPos = 0;
        g_ClBlinkCounter = 0;
        g_ClWindows = 0;
        ISCR(q)->counter = 2;
    }
}

void cl_menu_close(void) {
    ISCREEN->substep = (u8) (ISCREEN->substep + 1);
    g_ClMsgFlags = 0x12;
    g_ClWindow1AnimCmd = 0x81;
    g_ClPendingCamera = 0x84;
    g_ClInputEnabled = 0;
    ISCREEN->timer = 0xC;
}

void cl_menu_wait_close(void) {
    s16 ticks;
    s16 ticks_left;
    s16 settle_count;
    void *obj;
    u8 t;

    ticks = ((s16) ISCREEN->timer);
    if (ticks != 0) {
        ticks_left = ticks - 1;
        ISCREEN->timer = ticks_left;
        if ((ticks_left << 0x10) == 0) {
            g_ClWindow2AnimCmd = 0x81;
        }
    } else if ((g_ClWindow1AnimCmd == 0) && (g_ClWindow2AnimCmd == 0)) {
        ISCREEN->substep = (u8) (ISCREEN->substep + 1);
    obj = g_InteractiveScreenStatePtr;
    g_ClWindow1Enabled = 0;
    g_ClWindow2Enabled = 0;
    ISCR(obj)->timer = 0x5A;
    t = g_ClDoorSel;
        if (t == 0) {
            g_EnemySlot0_BehaviorFlags = 0x46;
            settle_count = 1;
        } else {
            g_EnemySlot0_BehaviorFlags = 0x48;
            settle_count = 2;
        }
        ISCR(obj)->counter = settle_count;
    }
}

void cl_menu_arm_settle(void) {
    u16 count;

    if (g_EnemySlot0_BehaviorFlags & 0x20) {
        count = ISCREEN->counter - 1;
        ISCREEN->counter = count;
        if ((count << 0x10) != 0) {
            g_EnemySlot0_BehaviorFlags = 0x46;
            return;
        }
        ISCREEN->substep = (u8) (ISCREEN->substep + 1);
        g_EnemySlot1_BehaviorFlags = 0x47;
        g_ClLedFlags |= 2;
    }
}

void cl_menu_blink(void) {
    u16 ticks;

    ticks = ISCREEN->timer - 1;
    ISCREEN->timer = ticks;
    if ((ticks << 0x10) == 0) {
        if (g_ClDoorSel == 1) {
            ISCREEN->substate = 4;
            ISCREEN->step = 0;
            return;
        }
        ISCREEN->substep = (u8) (ISCREEN->substep + 1);
        g_ClStatusIdx = 0;
        ISCREEN->timer = 0x1EU;
        ISCREEN->counter = 0;
        return;
    }
    g_ClStatusIdx = (((u16) ISCREEN->timer >> 4) ^ 1) & 1;
}

void cl_menu_countdown(void) {
    u16 ticks;
    u16 count;

    ticks = ISCREEN->timer - 1;
    ISCREEN->timer = ticks;
    if ((ticks << 0x10) == 0) {
        ISCREEN->timer = 0x1EU;
        count = ISCREEN->counter + 1;
        ISCREEN->counter = count;
        if ((s16) count == 3) {
            ISCREEN->substate = 3;
            ISCREEN->step = 0;
            g_ClMsgFlags = 0x32;
            ISCREEN->timer = 0x1EU;
            ISCREEN->counter = 0U;
        }
    }
    if ((s16) ISCREEN->counter != 0) {
        computer_draw_quads(-0x3C, -0x5F, 0x40, (u8) ISCREEN->counter, &g_ClStripB, 7);
    }
}

void cl_stage_command(void) {
    extern s32 (*cl_command_stage_tbl[])();
    cl_command_stage_tbl[ISCREEN->step]();
}

void cl_command_setup(void) {
    u16 ticks;

    ticks = ISCREEN->timer - 1;
    ISCREEN->timer = ticks;
    if ((ticks << 0x10) == 0) {
        ISCREEN->step = (u8) (ISCREEN->step + 1);
        ISCREEN->substep = 0;
        g_ClLedFlags = 1;
        computer_window_setup(2, -0x90, -0x68, 0x10, 4, 4, &g_ClRowsPanelB);
        g_ClText0WinIdx = 2;
        g_ClText0X = 0xC;
        g_ClWindow2Enabled = 1;
        g_ClWindow2AnimCmd = 1;
        g_ClKeyboardAnimCmd = 1;
        g_ClTextFields = 1;
        g_ClText0Y = 3;
        g_ClText0Masked = 1;
        computer_text_begin(0);
        g_ClPendingCamera = 0x85;
        g_ClCaretMode = 0;
        g_ClInputEnabled = 0;
        g_ClKeyboardFlags = 1;
        g_ClPadRepeatTimer = 0;
        ISCREEN->cursor_col = 1;
        ISCREEN->cursor_row = 0;
        ISCREEN->timer = 0U;
        play_sfx(2, 0x19, 0);
        g_EnemySlot0_BehaviorFlags = 0x47;
        g_EnemySlot1_BehaviorFlags = 0x47;
    }
}

void cl_command_wait_input(void) {
    void cl_login_typing();
    if ((g_ClWindow2AnimCmd == 0) && (g_ClKeyboardAnimCmd == 0)) {
        g_ClInputEnabled = 1;
        if (ISCREEN->substep != 0) {
            ISCREEN->step = (u8) (ISCREEN->step + 1);
            ISCREEN->substep = 0U;
            return;
        }
        cl_login_typing();
    }
}

void cl_command_steps(void) {
    extern s32 (*cl_command_step_tbl[])();
    cl_command_step_tbl[ISCREEN->substep]();
}

void cl_cmd_close_windows() {
    { s32 wt = (u8) (ISCREEN->substep + 1); ISCREEN->substep = wt; }
    g_ClWindow2AnimCmd = 0x81;
    g_ClKeyboardAnimCmd = 0x81;
    g_ClMsgFlags = 0x32;
    g_ClInputEnabled = 0;
    g_EnemySlot0_BehaviorFlags = 0x40;
    g_EnemySlot1_BehaviorFlags = 0x40;
}

void cl_cmd_after_close(void) {
    u8 *win2_anim_cmd = &g_ClWindow2AnimCmd;
    if (((*win2_anim_cmd) == 0) && (g_ClKeyboardAnimCmd == 0)) {
        ISCREEN->substep = (u8) (ISCREEN->substep + 1);
        g_ClWindow2Enabled = 0;
        (*win2_anim_cmd) = 0;
        g_ClKeyboardFlags = 0;
        if (g_ClCancelled != 0) {
            g_EnemySlot1_BehaviorFlags = 0x46;
        } else {
            g_ClTyper = 1;
            g_ClTyperLine = 0;
        }
        g_ClPendingCamDelay = 2;
        g_ClPendingCamera = 0x84;
    }
}

void cl_cmd_wait_typing(void) {
    if (g_ClCancelled != 0) {
        if (g_EnemySlot1_BehaviorFlags & 0x20) {
            ISCREEN_FIELD_AS(state, s32) = 0x201;
        }
    } else if (g_ClTyper == 0) {
        ISCREEN->substep = (u8) (ISCREEN->substep + 1);
    }
}

void cl_cmd_validate(void) {
    int strcmp(const char *a, const char *b);
    if (strcmp(g_ClText0Buf, &g_ClReleaseCommand) != 0) {
        ISCREEN->substep = 4;
        g_ClStatusIdx = 2;
        ISCREEN->timer = 0x5A;
        play_sfx(2, 0x19, 0);
        return;
    }
    ISCREEN->substep = 5;
    g_ClStatusIdx = 3;
    ISCREEN->timer = 0x5A;
    g_ClLedFlags |= 2;
}

void cl_cmd_rejected(void) {
    s16 ticks;
    s16 ticks_left;

    ticks = ((s16) ISCREEN->timer);
    ticks_left = ticks - (ticks > 0);
    ISCREEN->timer = ticks_left;
    if (((ticks_left << 0x10) == 0) && (g_EnemySlot0_BehaviorFlags & 0x20) && (g_EnemySlot1_BehaviorFlags & 0x20)) {
        ISCREEN->substate = 2;
        ISCREEN->step = 0;
    }
    if (((s16) ISCREEN->timer) == 0x4B) {
        g_EnemySlot0_BehaviorFlags = 0x44;
        g_EnemySlot1_BehaviorFlags = 0x44;
    }
}

void cl_cmd_accepted(void) {
    s16 ticks;
    s16 ticks_left;

    ticks = ((s16) ISCREEN->timer);
    ticks_left = ticks - (ticks > 0);
    ISCREEN->timer = ticks_left;
    if ((ticks_left << 0x10) == 0) {
        ISCREEN->substate = (u8) (ISCREEN->substate + 1);
        ISCREEN->step = 0;
        g_ClTextFields = 0;
        g_ClLedFlags = 1;
    }
}

void cl_stage_unlocked(void) {
    extern s32 (*cl_unlock_step_tbl[])();
    s32 *marker;
    s32 *origin;

    cl_unlock_step_tbl[ISCREEN->step]();
    if (!(g_ClDoorSel & 0x7F)) {
        origin = &g_ClDoorOrigin0;
        marker = &g_ClDoorMarker0;
    } else {
        origin = &g_ClDoorOrigin1;
        marker = &g_ClDoorMarker1;
    }
    if (ISCREEN->timer & 0x10) {
        computer_draw_quads((s16) (CL_PT(origin)->x + 0xFFC4), (s16) (CL_PT(origin)->y + 0xFFA1), 5, 1, marker, 0xA);
    }
}

void cl_cmd_unlock(void) {
    s32 Flg_ck_a12() __asm__("Flg_ck");
    s32 lock_flag;

    ISCREEN->step = (u8) (ISCREEN->step + 1);
    ISCREEN->substep = 0;
    g_ClLedFlags = 1;
    g_ClStatusIdx = 0;
    g_ClPendingCamera = 0x84;
    g_ClDoorSel |= 0x80;
    ISCREEN->timer = 0x5A;
    map_set_area_known(5);
    lock_flag = 0x26;
    if (!(g_ClDoorSel & 0x7F)) {
        lock_flag = 0x25;
    }
    if (Flg_ck_a12(ADDR_g_LocksFlags, lock_flag) != 0) {
        ISCREEN->timer = 0x20;
        return;
    }
    Flg_on(ADDR_g_LocksFlags, lock_flag);
    g_EnemySlot0_BehaviorFlags = 0x45;
    g_EnemySlot1_BehaviorFlags = 0x45;
}

void cl_cmd_unlock_hold(void) {
    u16 ticks;

    ticks = ISCREEN->timer - 1;
    ISCREEN->timer = ticks;
    if ((ticks << 0x10) == 0) {
        ISCREEN->step = (u8) (ISCREEN->step + 1);
        ISCREEN->timer = 0x10U;
    }
}

void cl_cmd_unlock_prompt(void) {
    u16 ticks;

    if (ISCREEN->substep == 0) {
        ISCREEN->substep = (u8) (ISCREEN->substep + 1);
        computer_window_setup(0, -0x38, -0x40, 5, 1, 1, &g_ClRowsUnlockPrompt);
        g_ClWindows = 1;
        ISCREEN->counter = 0x96U;
    }
    ticks = ISCREEN->counter - 1;
    ISCREEN->counter = ticks;
    if (((ticks << 0x10) == 0) || (g_PadPressed & PAD_CONFIRM) || (g_PadPressed & PAD_CANCEL)) {
        ISCREEN_FIELD_AS(state, s32) = 0x201;
        g_ClWindows = 0;
        g_ClDoorSel = 0;
        return;
    }
    computer_draw_quads(-0x38, -0x40, 3, 1, &g_ClUnlockPromptQuads, 0xA);
}

void computer_lab_finish(void) {
    void cl_finish_step_and_draw(void);
    s32 cl_finish_restore_room_a1() __asm__("cl_finish_restore_room");
    if (ISCREEN->substate == 0) {
        cl_finish_step_and_draw();
        return;
    }
    cl_finish_restore_room_a1();
}

void cl_finish_step_and_draw(void) {
    void cl_windows_update(void);
    s32 computer_typer_update();
    extern s32 (*cl_finish_step_tbl[])();
    void GsSortSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    cl_finish_step_tbl[ISCREEN->step]();
    computer_typer_update();
    cl_windows_update();
    if (ISCREEN->step != 0) {
        LIT(s32, sp_sprite_attribute) = 0x05000000;
        LIT(u8, sp_sprite_r) = 0x80;
        LIT(u8, sp_sprite_g) = 0x80;
        LIT(u8, sp_sprite_b) = 0x80;
        LIT(s16, sp_sprite_tpage) = 7;
        LIT(s16, sp_sprite_cy) = 0x1EB;
        LIT(s16, sp_sprite_mx) = 0x3C;
        LIT(s16, sp_sprite_my) = 0x34;
        LIT(s16, sp_sprite_y) = -0x2B;
        LIT(u8, sp_sprite_v) = 0x68;
        LIT(s16, sp_sprite_w) = 0x78;
        LIT(s16, sp_sprite_cx) = 0;
        LIT(s32, sp_sprite_rotate) = 0;
        LIT(s16, sp_sprite_x) = 0;
        LIT(u8, sp_sprite_u) = 0;
        LIT(s16, sp_sprite_h) = 0x68;
        GsSortSprite(LITP(GsSPRITE, sp_sprite), (GsOT *)((g_ActiveBuffer * 0x14) + ADDR_g_OrderingTables), 3U);
    }
}

void cl_finish_start(void) {
    typedef struct {
        u8 enabled;
        u8 pad1[0xF];
        u8 anim_cmd;
        u8 pad11[0xB];
    } ClWindowFlags;
    extern ClWindowFlags g_ClWindows_s[] __asm__("g_ClWindows");
    u8 i;

    switch (ISCREEN->substep) {
    case 0:
        ISCREEN->substep += 1;
        for (i = 0; i < 4; i++) {
            if (g_ClWindows_s[i].enabled != 0) {
                g_ClWindows_s[i].anim_cmd = 0x81;
            } else {
                g_ClWindows_s[i].anim_cmd = 0;
            }
        }
        g_ClText0Buf[0] = 0x13;
        g_ClText0Buf[1] = 8;
        g_ClText0Buf[2] = 0x15;
        g_ClText0Buf[3] = 0x14;
        g_ClTyper = 1;
        g_ClInputEnabled = 0;
        g_ClText0Buf[4] = 0;
        g_ClTyperLine = 0;
        g_ClPendingCamDelay = 0;
        g_ClPendingCamera = 0x84;
        break;
    case 1:
        if (g_ClTyper == 0) {
            ISCREEN->counter = 0;
            ISCREEN->substep = (u8) (ISCREEN->substep + 1);
            g_ClLedFlags = 3;
            break;
        }
        break;
    case 2:
        computer_fade_step(8, 0);
        if ((((s16) ISCREEN->counter) != 0) && (g_ClWindow0AnimCmd == 0) && (g_ClWindow1AnimCmd == 0) && (g_ClWindow2AnimCmd == 0) && (g_ClKeyboardAnimCmd == 0)) {
            g_ClWindows = 0;
            g_ClWindow1Enabled = 0;
            g_ClWindow2Enabled = 0;
            g_ClKeyboardFlags = 0;
            ISCREEN->step = (u8) (ISCREEN->step + 1);
            g_ClMsgFlags = 0;
            LIT(s16, sp_sprite_scalex) = 0x1000;
            LIT(s16, sp_sprite_scaley) = 0x1000;
            g_ClLedFlags = 0;
            ISCREEN->timer = 0x1000;
        }
        break;
    }
}

void cl_finish_shrink_v(void) {
    ISCREEN_FIELD(timer) = (u16) (ISCREEN_FIELD(timer) - 0x7D0);
    LIT(s16, sp_sprite_scalex) = 0x1000;
    LIT(u16, sp_sprite_scaley) = ISCREEN_FIELD(timer);
    if ((s16) ISCREEN_FIELD(timer) < 0x65) {
        ISCREEN_FIELD(step) = (u8) (ISCREEN_FIELD(step) + 1);
        ISCREEN_FIELD(timer) = 0x1000U;
    }
}

void cl_finish_shrink_h(void) {
    extern u8 *g_InteractiveScreenStatePtr_u8p __asm__("g_InteractiveScreenStatePtr");
    u8 *p = g_InteractiveScreenStatePtr_u8p;
    u16 t;

    ISCR(p)->timer -= 0x800;
    SCRATCH->spr_flags = 0x01000000;
    SCRATCH->spr_r = 0x80;
    SCRATCH->spr_g = 0x80;
    SCRATCH->spr_b = 0x80;
    t = ISCR(p)->timer;
    SCRATCH->spr_scale_y = 0x60;
    SCRATCH->spr_scale_x = t;
    if (((s16) ISCR(p)->timer) <= 0) {
        SCRATCH->spr_scale_x = 0;
        ISCR(p)->substate++;
        ISCREEN_B->timer = 0x1E;
    }
}

void cl_finish_restore_room(void) {
    void GsSetAmbient(long r, long g, long b);
    extern RoomLight g_ClSavedRoomLights[3];
    extern u8 *g_InteractiveScreenStatePtr_h10 __asm__("g_InteractiveScreenStatePtr");
    extern u8 g_EnemySlot0_BehaviorFlags_h10 __asm__("g_EnemySlot0_BehaviorFlags");
    extern u8 g_EnemySlot1_BehaviorFlags_h10 __asm__("g_EnemySlot1_BehaviorFlags");
    u8 *w;

    if (--FIELD_AT(g_InteractiveScreenStatePtr_h10, InteractiveScreen, ->timer, s16) == 0) {
        *(RoomLight *)(LIT(u8 *, g_RoomData) + 0xC) = g_ClSavedRoomLights[0];
        *(RoomLight *)(LIT(u8 *, g_RoomData) + 0x20) = g_ClSavedRoomLights[1];
        *(RoomLight *)(LIT(u8 *, g_RoomData) + 0x34) = g_ClSavedRoomLights[2];
        w = LIT(u8 *, g_RoomData);
        ROOM_HDR(w)->ambient_r = g_ClSavedAmbientR;
        ROOM_HDR(w)->ambient_g = g_ClSavedAmbientG;
        ROOM_HDR(w)->ambient_b = g_ClSavedAmbientB;
        GsSetAmbient(ROOM_HDR(w)->ambient_r, ROOM_HDR(w)->ambient_g, ROOM_HDR(w)->ambient_b);
        g_EnemySlot0_BehaviorFlags_h10 = 0x80;
        g_EnemySlot1_BehaviorFlags_h10 = 0x80;
        *g_InteractiveScreenStatePtr_h10 = 0;
        LIT(u16, g_UpdateFlags) |= 0x107;
        g_CutId = g_InteractiveScreenSavedCut;
        check_camera_switch(1);
        Flg_off(ADDR_g_ScenarioFlags, 0x20);
        g_PlayerEntity.pos_y = FIELD_AT(g_InteractiveScreenStatePtr_h10, InteractiveScreen, ->saved_pos_y, s32);
    }
}

void cl_windows_update(void) {
    s32 cl_window_anim_begin_a1() __asm__("cl_window_anim_begin");
    s32 cl_window_anim_step_a1() __asm__("cl_window_anim_step");
    s32 i;

    g_ClCurWindow = &g_ClWindows;
    i = 0;
    do {
        if ((CL_WIN(g_ClCurWindow)->enabled != 0) && (CL_WIN(g_ClCurWindow)->anim_cmd & 1)) {
            if (CL_WIN(g_ClCurWindow)->anim_state == 0) {
                cl_window_anim_begin_a1();
            } else {
                cl_window_anim_step_a1();
            }
        }
        i += 1;
        g_ClCurWindow += 0x1C;
    } while ((u32) (i & 0xFF) < 4U);
}

void cl_window_anim_begin(void) {
    s16 v;

    CL_WIN(g_ClCurWindow)->anim_state++;
    if (CL_WIN(g_ClCurWindow)->anim_cmd & 0x80) {
        v = 0xFF;
        if (CL_WIN(g_ClCurWindow)->keyboard_ramp != 0) {
            v = 0x80;
        }
        CL_WIN(g_ClCurWindow)->alpha = v;
        CL_WIN(g_ClCurWindow)->visible = 1;
    } else {
        CL_WIN(g_ClCurWindow)->alpha = 0;
        CL_WIN(g_ClCurWindow)->visible = 1;
    }
}

void cl_window_anim_step(void) {
    typedef struct {
        u8 pad[0x10];
        u8 anim_cmd;
        u8 anim_state;
        s16 alpha;
        s16 visible;
        s16 keyboard_ramp;
    } ClWindowFade;
    extern ClWindowFade *g_ClCurWindow_h6 __asm__("g_ClCurWindow");
    u8 mode;
    ClWindowFade *p;
    ClWindowFade *q;
    s32 c;
    s32 v;
    s32 w;

    q = g_ClCurWindow_h6; c = q->anim_cmd; do { mode = q->keyboard_ramp; } while (0); p = q;
    if (c & 0x80) {
        v = p->alpha;
        if (mode != 0) {
            v -= 8;
        } else {
            v -= 0x10;
        }
        p->alpha = v;
        if (q->alpha <= 0) {
            q->anim_state++;
            g_ClCurWindow_h6->alpha = 0;
            g_ClCurWindow_h6->anim_cmd = 0;
            g_ClCurWindow_h6->anim_state = 0;
        }
    } else {
        v = p->alpha;
        if (mode != 0) {
            w = v + 8;
        } else {
            w = v + 0x10;
        }
        p->alpha = w;
        v = g_ClCurWindow_h6->alpha;
        if (g_ClCurWindow_h6->keyboard_ramp != 0) {
            if (v < 0x80) {
                return;
            }
            goto reset;
        }
        if (v >= 0xFF) {
        reset:
            g_ClCurWindow_h6->anim_state++;
            g_ClCurWindow_h6->alpha = 0x80;
            g_ClCurWindow_h6->visible = 0;
            g_ClCurWindow_h6->anim_cmd = 0;
            g_ClCurWindow_h6->anim_state = 0;
        }
    }
}
