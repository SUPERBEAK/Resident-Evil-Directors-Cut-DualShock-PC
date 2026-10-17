/* memcard.c - Memory card: the save / load screens, the save block (player state store / restore) and the card
   I/O (events, directory, read / write / format). */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void load_room_bg_image(void);


void memcard_draw_cursor(s32 x, s32 y, s32 hidden) {
    void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
    GsSPRITE *sp;

    if (hidden == 0) {
        sp = LITP(GsSPRITE, sp_sprite);
        sp->x = x - 0xA0;
        sp->y = y - 0x78;
        *(s32 *)&sp->w = 0xE0008;   /* w = 8, h = 14 */
        sp->tpage = 0x1E;
        *(u16 *)&sp->u = 0x1C10;    /* u = 0x10, v = 0x1C */
        sp->attribute &= 0xBFFFFFFF;
        *(s32 *)&sp->cx = 0x01E00100;
        GsSortFastSprite(sp, LITP(GsOT, g_OrderingTables) + g_ActiveBuffer, 2);
    }
}

const char s_SumError[] = "SUM Error!!%x:%x\n"; /* printed by cd.c */

const char s_Empty[] = "";
const char s_SaveFilePattern[] = "BASLUS-00551*"; /* memory card file search: every RE1 save file */
const char s_SaveFileName[] = "BASLUS-005510";   /* the last character becomes the file number */
const char s_UnusedEmpty[] = "";                 /* not referenced anywhere */
/* 4-aligned jump table (tools/jtalign.awk) */
__asm__("#JTALIGN2");
/* 14-byte string copied by value from rodata (s_SaveFileName; unaligned byte-block copy, as compiled) */
typedef struct { char s[14]; } Str14;

s32 memcard_screen(s32 is_load, s32 save_buf, s32 use_ribbon, u8 sfx_bank) {
    void memcard_screen_open(s32);
    void memcard_screen_close(s32);
    s32 memcard_check_changed(s32);
    void memcard_draw_menu(s32);
    s32 memcard_draw_file_list(s32, u8 *, s32, u8, u8);
    void memcard_draw_card_prompt(void);
    void save_store_player_state(void);
    void load_restore_player_state(void);
    void memcard_save_done_anim_l(u8, s32, s32, u8 *, u8) __asm__("memcard_save_done_anim");
    void memcard_build_file_title(u8 *, s32, s32);
    s32 memcard_detect_cards(void);
    s32 memcard_read_saves_l(u8 *, s32) __asm__("memcard_read_saves");
    s32 memcard_write_file_l(s32, s32, u8 *, u8 *, s32) __asm__("memcard_write_file");
    s32 memcard_list_files(s32, u8 *, char *);
    s32 memcard_load_dir(s32);
    s32 memcard_get_status(s32);
    s32 memcard_format(s32);
    s32 memcard_wait_sw_event(void);
    void use_room_action_item(void);
    void print_formatted_text(s32, s32, s32, u8 *);
    void task_sleep(s16);
    void play_sfx_l(s32, s32, s32) __asm__("play_sfx");
    void mem_copy(u8 *, u8 *, s32);
    int VSync(int mode);
    extern u8 g_MemcardInfoPending_l __asm__("g_MemcardInfoPending");
    extern u8 g_MemcardFileCount_l __asm__("g_MemcardFileCount");
    extern u8 s_McMemoryCard1_l[] __asm__("s_McMemoryCard1");
    extern u8 s_McMemoryCard2_l[] __asm__("s_McMemoryCard2");
    extern u8 s_McCheckingMemoryCards_l[] __asm__("s_McCheckingMemoryCards");
    extern u8 s_McDoNotPullOut_l[] __asm__("s_McDoNotPullOut");
    extern Str14 s_SaveFileName_l __asm__("s_SaveFileName");
    u8 info[400];
    u8 hdr[72];
    u8 status[8];
    u8 saved;
    u8 prev;
    s32 size;
    u8 timer;
    u8 blinkhi;
    u8 slot;
    u8 state;
    u8 sel;
    u8 ret;
    u8 max;
    u8 mode;
    s32 blink;
    u16 pad;
    s32 r;
    u8 idx;
    s32 p;
    u8 *e;

    sel = 0;
    state = 0;
    mode = 0;
    blink = 0;
    memcard_screen_open(sfx_bank);
    g_MemcardBusy = 1;
    while (1) {
        g_MemcardInfoPending_l = 0;
        if (g_MemcardBusy == 0) {
            if (g_PadTrigger & (PSX_SELECT | PSX_UP | PSX_DOWN)) {
                timer = 6;
                goto input;
            }
            if (timer == 0) {
                timer = 2;
            input:
                if (state == 6 || state == 2) {
                    pad = g_PadHeldRaw;
                    if ((pad & 0x4000) || (pad & 0x100)) {
                        blink = 0;
                        sel++;
                        play_sfx_l(sfx_bank, 0x1E, 0);
                    } else if (pad & 0x1000) {
                        blink = 0;
                        sel--;
                        play_sfx_l(sfx_bank, 0x1E, 0);
                    }
                    if ((s8)sel < 0) {
                        sel = max;
                    }
                    if (sel > max) {
                        sel = 0;
                    }
                } else if ((g_PadHeldRaw & PSX_DOWN) && sel < max) {
                    blink = 0;
                    sel++;
                    play_sfx_l(sfx_bank, 0x1B, 0);
                } else if ((g_PadHeldRaw & PSX_SELECT) && max != 0) {
                    blink = 0;
                    sel++;
                    play_sfx_l(sfx_bank, 0x1B, 0);
                    if (sel == max + 1) {
                        sel = 0;
                    }
                } else if ((g_PadHeldRaw & PSX_UP) && sel != 0) {
                    blink = 0;
                    sel--;
                    play_sfx_l(sfx_bank, 0x1B, 0);
                }
                if (max != 0 && max < sel) {
                    sel = max;
                }
            } else {
                timer--;
            }
        }
        if (sel != prev) {
            blink = 0;
        }
        blink = (blink + 1) & 0x1F;
        blinkhi = blink >> 4;
        prev = sel;
        switch (state) {
        case 0:
            r = memcard_detect_cards();
            if (r == 1 || r == 2) {
                g_MemcardSlot = r & 1;
                state = 4;
            } else if (r == 3) {
                g_MemcardSlot = 0;
                state = 3;
                ret = 4;
            } else {
                state = 2;
                g_MemcardBusy = 0;
            }
            break;
        case 1:
            mode = 0;
            max = 1;
            if ((g_PadPressed & PAD_CONFIRM) || (g_PadRawPressed32 & PSX_START)) {
                play_sfx_l(sfx_bank, 0x1D, 0);
                if (sel == 1) {
                    goto exit1;
                }
                sel = 0;
                g_MemcardBusy = 1;
                state = 0;
            } else if (g_PadPressed & PAD_CANCEL) {
                play_sfx_l(sfx_bank, 0x1C, 0);
                goto exit1;
            } else {
                print_formatted_text(0x4C, 0x60, 0, s_McUseMemoryCard);
                print_formatted_text(0x4C, 0x78, 0, s_McDontUseMemoryCard);
                memcard_draw_cursor(0x42, sel * 24 + 0x60, blinkhi);
            }
            break;
        case 2:
            mode = 1;
            max = 2;
            if ((g_PadPressed & PAD_CONFIRM) || (g_PadRawPressed32 & PSX_START)) {
                play_sfx_l(sfx_bank, 0x1D, 0);
                if (sel == 2) {
                    goto exit1;
                }
                state = 4;
                g_MemcardBusy = 1;
                g_MemcardSlot = sel;
            } else if (g_PadPressed & PAD_CANCEL) {
                play_sfx_l(sfx_bank, 0x1C, 0);
                goto exit1;
            }
            break;
        case 3:
            max = 0;
            sel = 0;
            mode = 0;
            if (ret != 4) {
                if (g_MemcardSlot == 0) {
                    print_formatted_text(0x6C, 0x18, 0, s_McMemoryCard1_l);
                } else {
                    print_formatted_text(0x6C, 0x18, 0, s_McMemoryCard2_l);
                }
            }
            print_formatted_text(g_McErrorMsgX[ret], 0x60, 1, g_McErrorMsgs[ret]);
            if ((g_PadPressed & (PAD_CONFIRM | PAD_CANCEL)) || (g_PadRawPressed32 & PSX_START)) {
                play_sfx_l(sfx_bank, 0x1D, 0);
                g_MemcardBusy = 0;
                state = 1;
                sel = 0;
            }
            break;
        case 4:
            mode = 0;
            print_formatted_text(0x20, 0xB2, 0, s_McCheckingMemoryCards_l);
            print_formatted_text(0x20, 0xC0, 0, s_McDoNotPullOut_l);
            task_sleep(1);
            state = 5;
            break;
        case 5:
            mode = 0;
            g_MemcardBusy = 0;
            max = 5;
            ret = memcard_get_status(g_MemcardSlot);
            if (ret & 1) {
                if (ret == 1) {
                    state = 3;
                    ret = 4;
                } else if (ret == 3) {
                    state = 3;
                    ret = 1;
                }
                break;
            }
            ret = memcard_load_dir(g_MemcardSlot);
            if (ret == 0) {
                ret = memcard_read_saves_l(status, save_buf);
                memcard_list_files(g_MemcardSlot, info, (char *)s_Empty);
                size = *(s32 *)(&MC_DIR(info)->total_size);
            }
            switch (ret) {
            case 0:
                if (is_load == 1 && g_MemcardFileCount_l == 0) {
                    state = 3;
                    ret = 3;
                } else if (is_load == 0 && g_MemcardFileCount_l == 0 && size == 0x1E000) {
                    state = 3;
                    ret = 2;
                } else {
                    sel = 0;
                    state = 6;
                }
                break;
            case 1:
                state = 3;
                ret = 4;
                break;
            case 2:
                if (is_load == 1) {
                    state = 3;
                    ret = 6;
                } else {
                    sel = 1;
                    state = 10;
                }
                break;
            case 3:
                state = 3;
                ret = 1;
                break;
            }
            break;
        case 6:
            max = 5;
            mode = 2;
            if ((g_PadPressed & PAD_CONFIRM) || (g_PadRawPressed32 & PSX_START)) {
                if (sel == 5) {
                    play_sfx_l(sfx_bank, 0x1C, 0);
                    goto exit1;
                }
                play_sfx_l(sfx_bank, 0x1F, 0);
                slot = sel;
                if (is_load == 0 && memcard_list_files(g_MemcardSlot, info, (char *)s_SaveFilePattern) != 0) {
                    VSync(0);
                    ret = memcard_get_status(g_MemcardSlot);
                    if (ret != 0) {
                        state = 3;
                        ret = 4;
                        break;
                    }
                }
                g_MemcardBusy = 1;
                state = 7;
            } else if (g_PadPressed & PAD_CANCEL) {
                play_sfx_l(sfx_bank, 0x1C, 0);
                sel = 0;
                mode = 0;
                state = 14;
            } else if (memcard_check_changed(g_MemcardSlot) != 0) {
                g_MemcardSlot = 0;
                state = 3;
                ret = 4;
            } else {
                memcard_draw_cursor(0x26, sel * 20 + 0x38, blinkhi);
            }
            break;
        case 7:
            sel = slot;
            if (is_load == 0) {
                e = info + sel * 22;
                *(Str14 *)e = s_SaveFileName_l;
                e[12] += sel;
                state = 13;
                if (status[sel] < 5) {
                    saved = sel;
                    g_MemcardBusy = 0;
                    sel = 1;
                } else {
                    state = 15;
                    if (size == 0x1E000) {
                        ret = 2;
                        state = 12;
                        saved = sel;
                    }
                }
            } else {
                state = 9;
                if (status[sel] == 5) {
                    ret = 3;
                    state = 12;
                    g_MemcardBusy = 0;
                    saved = sel;
                }
            }
            break;
        case 8:
            sel = slot;
            idx = sel;
            save_store_player_state();
            p = save_buf + ({ idx * 0x240 + 0xA00; });
            mem_copy((u8 *)p, LITP(u8, g_StageId), 0x13C);
            mem_copy(LITP(u8, sp_tmp04), g_ItemSlotIconIndex, 9);
            LIT(s32, sp_tmp00) = LIT(u8, g_TotalHeldItems);
            if (use_ribbon != 0) {
                g_SelectedItemId = 0x2F;
                use_room_action_item();
            }
            memcard_build_file_title(hdr, p, idx);
            if ((u8)memcard_write_file_l(ADDR_g_SaveData, g_MemcardSlot, info + idx * 22, hdr, 0x800) == 0) {
                status[idx] = 0;
                memcard_save_done_anim_l(idx, is_load, save_buf, status, sfx_bank);
                ret = 0;
                LIT(u8, g_SaveCount)++;
                goto out;
            }
            mem_copy(LITP(u8, g_InventorySlots), (u8 *)(p + 0x124), 0x18);
            mem_copy(g_ItemSlotIconIndex, LITP(u8, sp_tmp04), 9);
            state = 3;
            LIT(u8, g_TotalHeldItems) = LIT(u8, sp_tmp00);
            ret = 5;
            g_MemcardBusy = 0;
            break;
        case 9:
            mem_copy(LITP(u8, g_StageId), (u8 *)(save_buf + ({ slot * 0x240 + 0xA00; })), 0x240);
            ret = 0;
            load_restore_player_state();
            goto out;
        case 10:
            mode = 3;
            max = 1;
            if ((g_PadPressed & PAD_CONFIRM) || (g_PadRawPressed32 & PSX_START)) {
                play_sfx_l(sfx_bank, 0x1D, 0);
                if (sel != 0) {
                    sel = 0;
                    state = 1;
                } else {
                    state = 11;
                }
                break;
            }
            if (g_PadPressed & PAD_CANCEL) {
                play_sfx_l(sfx_bank, 0x1C, 0);
                g_MemcardBusy = 1;
                mode = 0;
                state = 0;
                sel = 0;
            }
            if (memcard_check_changed(g_MemcardSlot) != 0) {
                g_MemcardSlot = 0;
                state = 3;
                ret = 4;
            }
            break;
        case 11:
            mode = 0;
            idx = memcard_get_status(g_MemcardSlot);
            if (idx == 0) {
                ret = memcard_load_dir(g_MemcardSlot);
                if (ret == 2 && memcard_format(g_MemcardSlot) == 0) {
                    g_MemcardBusy = 1;
                    state = 4;
                } else {
                    ret = 0;
                    state = 3;
                }
            } else {
                ret = (idx == 1) * 4;
                sel = 0;
                state = 3;
            }
            break;
        case 12:
            max = 0;
            sel = 0;
            if ((g_PadPressed & (PAD_CONFIRM | PAD_CANCEL)) || (g_PadRawPressed32 & PSX_START)) {
                play_sfx_l(sfx_bank, 0x1D, 0);
                g_MemcardBusy = 0;
                if (ret == 2) {
                    state = 1;
                } else {
                    state = 6;
                    sel = saved;
                    max = sel;
                }
            }
            print_formatted_text(g_McErrorMsgX[ret], 0xC4, 1, g_McErrorMsgs[ret]);
            if (memcard_check_changed(g_MemcardSlot) != 0) {
                g_MemcardSlot = 0;
                state = 3;
                ret = 4;
            }
            break;
        case 13:
            max = 1;
            if ((g_PadPressed & PAD_CONFIRM) || (g_PadRawPressed32 & PSX_START)) {
                play_sfx_l(sfx_bank, 0x1D, 0);
                if (sel == 1) {
                    sel = saved;
                    state = 6;
                    max = sel;
                } else {
                    sel = saved;
                    max = sel;
                    g_MemcardBusy = 1;
                    state = 15;
                }
                break;
            }
            if (g_PadPressed & PAD_CANCEL) {
                play_sfx_l(sfx_bank, 0x1C, 0);
                sel = saved;
                state = 6;
                max = sel;
            }
            if (memcard_check_changed(g_MemcardSlot) != 0) {
                g_MemcardSlot = 0;
                state = 3;
                ret = 4;
            }
            break;
        case 14:
            if (memcard_get_status(g_MemcardSlot ^ 1) != 0) {
                goto exit1;
            }
            sel = 0;
            state = 2;
            break;
        case 15:
        case 16:
            print_formatted_text(0x20, 0xB2, 0, s_McNowSaving);
            print_formatted_text(0x20, 0xC0, 0, s_McDoNotPullOut2);
            state++;
            if (state == 17) {
                state = 8;
            }
            break;
        }
        g_MemcardInfoTimeout = 0;
        if (g_MemcardInfoPending_l != 0 && memcard_wait_sw_event() == 1) {
            g_MemcardInfoTimeout = 1;
        }
        switch (mode) {
        case 0:
            break;
        case 1:
            memcard_draw_menu(is_load);
            memcard_draw_cursor(0x62, sel * 24 + 0x60, blinkhi);
            break;
        case 2:
            memcard_draw_file_list(is_load, status, save_buf, 0, 0x1A);
            if (state == 13) {
                print_formatted_text(0x78, 0xB2, 1, s_McOverwrite);
                print_formatted_text(0x94, 0xC4, 0, s_McYes);
                print_formatted_text(0x94, 0xD6, 0, s_McNo);
                memcard_draw_cursor(0x8A, sel * 18 + 0xC4, blinkhi);
            }
            break;
        case 3:
            memcard_draw_card_prompt();
            memcard_draw_cursor(0x3A, sel * 24 + 0x60, blinkhi);
            break;
        }
        task_sleep(1);
    }
exit1:
    ret = 1;
out:
    memcard_screen_close(sfx_bank);
    return ret;
}

void memcard_screen_open(s32 sfx_bank) {
    void task_sleep(s16);
    s32 fade_update();
    s32 status_flags;
    s32 status_flags2;

    g_VSyncMode = 2;
    LIT(s32, g_GameStateFlags) = (LIT(s32, g_GameStateFlags) & ~GAME_SCREEN_MODE) | GAME_SCREEN_CLEAR;
    status_flags = LIT(s32, g_StatusFlags);
    g_MemcardSavedStatusFlags = status_flags & 6;
    status_flags2 = status_flags & ~6;
    LIT(s32, g_StatusFlags) = status_flags2;
    if ((sfx_bank & 0xFF) == 2) {
        LIT(s32, g_StatusFlags) = status_flags2 | STATUS_SFX_BANK1_SMALL;
    }
    task_sleep(2);
    load_file(0x1E, ADDR_g_TypewriterTimBuf);
    g_FadeTypeId = 2;
    g_FadeStep = -0x400;
    LIT(s32, g_GameStateFlags) = (LIT(s32, g_GameStateFlags) & ~GAME_SCREEN_MODE) | GAME_SCREEN_BG;
    fade_update();
    do {
        task_sleep(1);
    } while (g_FadeLevel >= 0);
    task_sleep(1);
}

s32 memcard_check_changed(s32 slot) {
    long _card_info(long chan);
    if (_card_info(slot * 0x10) != 0) {
        g_MemcardInfoPending = 1;
        return g_MemcardInfoTimeout != 0;
    }
    return 0;
}

void memcard_screen_close(s32 sfx_bank) {
    void task_sleep(s16);
    s32 load_room_bg_image_a2() __asm__("load_room_bg_image");
    s32 fade_update();
    s32 vram_clear_rect();
    g_FadeTypeId = 2;
    g_FadeStep = 0x400;
    fade_update();
    if (g_FadeLevel >= 0) {
        do {
            task_sleep(1);
        } while (g_FadeLevel >= 0);
    }
    g_VSyncMode = 2;
    task_sleep(1);
    if ((sfx_bank & 0xFF) == 2) {
        load_room_bg_image_a2();
        LIT(s32, g_GameStateFlags) = (LIT(s32, g_GameStateFlags) & ~GAME_SCREEN_MODE) | GAME_SCREEN_BG;
    } else {
        vram_clear_rect(0, 0, 0x140, 0x1E0);
    }
    LIT(s32, g_StatusFlags) = (LIT(s32, g_StatusFlags) | g_MemcardSavedStatusFlags) & ~STATUS_SFX_BANK1_SMALL;
}

void memcard_build_slot_map(u8 *map) {
    extern char g_MemcardFileNames[][0x16] __asm__("g_MemcardFileList");  /* "BASLUS-00551" + save digit */
    u32 i;

    i = 0;
    do {
        map[i] = 5;
        i++;
    } while (i < 5);
    for (i = 0; i < 5; i++) {
        if (i >= g_MemcardFileCount[0]) {
            break;
        }
        *(map + ({ g_MemcardFileNames[i][12] - '0'; })) = i;
    }
}

void memcard_draw_menu(s32 is_load) {
    void print_formatted_text_a2(s32, s32, s32, void *) __asm__("print_formatted_text");
    extern u8 s_McMemoryCard1_u8[] __asm__("s_McMemoryCard1");
    extern u8 s_McMemoryCard2_u8[] __asm__("s_McMemoryCard2");
    if (is_load == 0) {
        print_formatted_text_a2(0x50, 0x40, 0, s_McSaveOnCard1Or2);
        print_formatted_text_a2(0x6C, 0x90, 0, s_McSave);
    } else {
        print_formatted_text_a2(0x48, 0x40, 0, s_McLoadFromCard1Or2);
        print_formatted_text_a2(0x6C, 0x90, 0, s_McLoad);
    }
    print_formatted_text_a2(0x6C, 0x60, 0, s_McMemoryCard1_u8);
    print_formatted_text_a2(0x6C, 0x78, 0, s_McMemoryCard2_u8);
    print_formatted_text_a2(0x6C, 0x90, 0, s_McDoNot);
}

s32 memcard_draw_file_list(s32 is_load, u8 *file_map, s32 save_buf, u8 hl_file, u8 hl_char) {
    s32 print_formatted_text_l() __asm__("print_formatted_text");
    s32 mem_copy_l() __asm__("mem_copy");
    s32 save_get_area_index_l() __asm__("save_get_area_index");
    extern u8 s_McMemoryCard1_l[] __asm__("s_McMemoryCard1");
    extern u8 s_McMemoryCard2_l[] __asm__("s_McMemoryCard2");
    s32 i;
    s32 ret;
    s32 col;

    ret = 0;
    for (i = 0; i < 5; i++) {
        LIT(u8, sp_text) = i + 0xD;
        LIT(u8, sp_text_1) = 0x79;
        LIT(u8, sp_text_2) = 1;
        print_formatted_text_l(0x30, i * 0x14 + 0x38, 0, ADDR_sp_text);
        LIT(u8, sp_text) = 0;
        LIT(u8, sp_text_1) = 0;
        col = 0;
        if (file_map[i] < 5) {
            if (((u8 *)save_buf + i * 0x240)[0xA2B] & 3) {
                mem_copy_l(ADDR_sp_text_2, s_McJillLabel, 9);
            } else {
                mem_copy_l(ADDR_sp_text_2, s_McChrisLabel, 9);
            }
            mem_copy_l(ADDR_sp_text_B, g_McAreaNames[save_get_area_index_l(save_buf + 0xA00 + i * 0x240) & 0xFF], 0x22);
            {
                u8 t = ((u8 *)save_buf + i * 0x240)[0xA28];
                LIT(u8, sp_text_8) += t / 10;
                LIT(u8, sp_text_9) += ((u8 *)save_buf + i * 0x240)[0xA28] % 10;
            }
            col = ((u8 *)save_buf + i * 0x240)[0xA33];
        } else {
            mem_copy_l(ADDR_sp_text_2, s_McNoData, 0x12);
        }
        if (i == hl_file) {
            if (hl_char >= 9) {
                hl_char += hl_char - 9;
            }
            if (LITP(u8, sp_text_2)[hl_char] != 0) {
                ret = 1;
            }
            LITP(u8, sp_text_2)[hl_char] = 1;
        }
        if (file_map[i] < 5) {
            if (((u8 *)save_buf + i * 0x240)[0xA33] == 3) {
                col = 1;
                if (LIT(u8, sp_text_7) != 1) {
                    col = 2;
                    LIT(u8, sp_text_7) = 1;
                    print_formatted_text_l(0x30, i * 0x14 + 0x38, 0x11, ADDR_sp_text);
                    LIT(u8, sp_text_7) = 0x38;
                }
            }
        }
        print_formatted_text_l(0x30, i * 0x14 + 0x38, col, ADDR_sp_text);
    }
    print_formatted_text_l(0x30, 0x9C, 0, s_McDoNot);
    if (is_load == 0) {
        print_formatted_text_l(0x30, 0x9C, 0, s_McSave);
    } else {
        print_formatted_text_l(0x30, 0x9C, 0, s_McLoad);
    }
    if (g_MemcardSlot == 0) {
        print_formatted_text_l(0x6C, 0x18, 0, s_McMemoryCard1_l);
    } else {
        print_formatted_text_l(0x6C, 0x18, 0, s_McMemoryCard2_l);
    }
    return ret;
}

void memcard_draw_card_prompt(void) {
    void print_formatted_text();
    if (g_MemcardSlot == 0) {
        print_formatted_text(0x6C, 0x18, 0, &s_McMemoryCard1);
    } else {
        print_formatted_text(0x6C, 0x18, 0, &s_McMemoryCard2);
    }
    print_formatted_text(0x48, 0x40, 1, &s_McFormattingIsRequired);
    print_formatted_text(0x44, 0x60, 0, &s_McFormatThisCard);
    print_formatted_text(0x44, 0x78, 0, &s_McDoNotFormatThisCard);
}

void save_store_player_state(void) {
    g_SavePlayerId = g_PlayerEntity.id;
    g_SavePlayerX = (*(u16 *)&g_PlayerEntity.pos_x);
    g_SavePlayerZ = (*(u16 *)&g_PlayerEntity.pos_z);
    g_SavePlayerAngle = g_PlayerEntity.angle;
    g_SavePlayerCollisionFlags = g_PlayerEntity.collision_flags;
}

void load_restore_player_state(void) {
  int flags;
  LIT(s32, g_GameStateFlags) |= GAME_LOADED_SAVE;
  if ((LIT(u8, g_SaveDcMode)) == 1)
  {
    LIT(s32, g_StatusFlags) |= STATUS_TRAINING;
  }
  if ((LIT(u8, g_SaveDcMode)) == 2)
  {
    LIT(s32, g_StatusFlags) = (s32) ((LIT(s32, g_StatusFlags)) | STATUS_ADVANCED);
  }
  if ((LIT(u8, g_SaveDcMode)) == 3)
  {
    flags = (LIT(s32, g_StatusFlags)) | STATUS_ADVANCED;
    LIT(s32, g_StatusFlags) = (s32) (flags | 0x10000);
  }
  g_PlayerEntity.id = g_SavePlayerId;
  if (g_SavePlayerId & 3)
  {
    LIT(s32, g_GameStateFlags) |= GAME_ALT_RDT;
  }
}

void memcard_save_done_anim(u8 file, s32 is_load, s32 save_buf, s32 file_map, u8 sfx_bank) {
    s32 memcard_draw_file_list_a1(s32, s32, s32, s32, s32) __asm__("memcard_draw_file_list");
    void task_sleep(s16);
    void mem_copy(u8 *, u8 *, s32);
    s32 i;
    s32 found;
    u32 n;

    mem_copy((u8 *)(save_buf + 0xA00 + file * 0x240), (u8 *)ADDR_g_StageId, 0x34);
    found = 0;
    i = 0;
    do {
        if (found != 0) {
            play_sfx(sfx_bank & 0xFF, 0x1F, 0);
        }
        n = (u8)(&g_McSaveDoneAnimDelays)[i] >> 1;
        if ((s32)n > 0) {
            do {
                found = memcard_draw_file_list_a1(is_load, file_map, save_buf, file, i & 0xFF);
                task_sleep(1);
                n--;
            } while ((s32)n > 0);
        }
        i++;
    } while (i < 0x1B);
}

/* The memory card file title (Shift-JIS, full-width characters): "R.E-DC" (Director's Cut), the file
   number at byte 0x15, the player name, the save count at 0x27/0x29; the mode tag goes at 0xE. */
/* "R.E-DC    1. Jill /00/" */
#define MC_TITLE_JILL \
    "\202\161\201\104\202\144\201\174\202\143\202\142\201\100\201\100\201\100\201\100\202\120" \
    "\201\104\201\100\202\151\202\211\202\214\202\214\201\100\201\136\202\117\202\117\201\136"
/* "R.E-DC    1. Chris/00/" */
#define MC_TITLE_CHRIS \
    "\202\161\201\104\202\144\201\174\202\143\202\142\201\100\201\100\201\100\201\100\202\120" \
    "\201\104\201\100\202\142\202\210\202\222\202\211\202\223\201\136\202\117\202\117\201\136"
#define MC_TAG_NONE "\201\100\201\100" /* two spaces */
#define MC_TAG_TR "\202\163\202\222" /* "Tr" */
#define MC_TAG_AD "\202\140\202\204" /* "Ad" */
#define MC_TAG_AT "\202\140\202\163" /* "AT" */

extern u8 g_McAreaTitlesSjis[][0x15];
extern u8 g_SavePlayerId;

void memcard_build_file_title(u8 *title, s32 save_rec, s32 file) {
    void mem_copy(u8 *, u8 *, s32);
    s32 save_get_area_index_l(s32) __asm__("save_get_area_index");
    u8 *p;
    s32 *flags;
    u8 *num;

    if (g_SavePlayerId & 1) {
        mem_copy(title, MC_TITLE_JILL, 0x2C);
    } else {
        mem_copy(title, MC_TITLE_CHRIS, 0x2C);
    }
    p = title + 0xE;
    mem_copy(p, MC_TAG_NONE, 4);
    flags = LITP(s32, g_StatusFlags);
    if (*flags & STATUS_TRAINING) {
        mem_copy(p, MC_TAG_TR, 4);
    }
    if (*flags & STATUS_ADVANCED) {
        mem_copy(p, MC_TAG_AD, 4);
    }
    if (*flags & STATUS_ADVANCED_HOLD) {
        mem_copy(p, MC_TAG_AT, 4);
    }
    num = LITP(u8, g_SaveCount);
    title[0x15] += file;
    title[0x27] += *num / 10;
    title[0x29] += *num % 10;
    mem_copy(title + 0x2C, g_McAreaTitlesSjis[save_get_area_index_l(save_rec) & 0xFF], 0x15);
}

s32 save_get_area_index(void *save_rec) {
    s32 area;
    u8 room;

    area = (SAVE_LOC(save_rec)->stage % 5) & 0xFF;
    if (area == 0) {
        room = SAVE_LOC(save_rec)->room;
        if (room == 6) {
            area = 1;
        }
        if ((area == 0) && (room == 0x18)) {
            area = 5;
        }
    }
    if ((area == 2) && (SAVE_LOC(save_rec)->room == 7)) {
        area = 6;
    }
    return area;
}

s32 memcard_detect_cards(void) {
    void task_sleep(s16);
    void print_formatted_text();
    s32 memcard_get_status(s32);
  unsigned char missing1;
  int missing2;
  print_formatted_text(0x20, 0xB2, 0, &s_McCheckingMemoryCards);
  print_formatted_text(0x20, 0xC0, 0, &s_McDoNotPullOut);
  task_sleep(2);
  missing1 = memcard_get_status(0) & 1;
  return missing1 | ((missing2 = memcard_get_status(1) & 1) * 2);
}

s32 memcard_read_saves(u8 *map, u8 *buf) {
    void mem_copy(u8 *, u8 *, s32);
    s32 memcard_read_file_a1() __asm__("memcard_read_file");
    s32 memcard_get_status();
    s32 memcard_load_dir();
    extern u8 g_MemcardFileCount_u8 __asm__("g_MemcardFileCount");
    u32 i;
    s32 off;
    u32 c;
    s32 r;

    memcard_list_files(g_MemcardSlot, g_MemcardFileList, s_SaveFilePattern);
    memcard_build_slot_map(map);
    if (g_MemcardFileCount_u8 != 0) {
        for (i = 0, off = 0xA00; i < 5; i++, off += 0x240) {
            c = *map++;
            if (c == 5) {
                continue;
            }
            if (memcard_read_file_a1(buf, g_MemcardSlot, c * 0x16 + g_MemcardFileList, 0xA00) != 0) {
                goto err;
            }
            mem_copy(buf + off, buf + 0x200, 0x240);
        }
        return 0;
    }
err:
    r = memcard_get_status(g_MemcardSlot);
    if (r != 0) {
        if (r == 3) {
            return 3;
        }
        return memcard_load_dir(g_MemcardSlot);
    }
    return 0;
}

void memcard_init_events(void) {
    s32 EnableEvent(s32);
    s32 EnterCriticalSection();
    s32 ExitCriticalSection();
    s32 OpenEvent(s32, s32, s32, s32);
    EnterCriticalSection();
    g_CardSwEvents = OpenEvent(0xF4000001, 4, 0x2000, 0);
    g_CardSwEvTimeout = OpenEvent(0xF4000001, 0x100, 0x2000, 0);
    g_CardSwEvNew = OpenEvent(0xF4000001, 0x2000, 0x2000, 0);
    g_CardSwEvError = OpenEvent(0xF4000001, 0x8000, 0x2000, 0);
    g_CardHwEvents = OpenEvent(0xF0000011, 4, 0x2000, 0);
    g_CardHwEvTimeout = OpenEvent(0xF0000011, 0x100, 0x2000, 0);
    g_CardHwEvError = OpenEvent(0xF0000011, 0x8000, 0x2000, 0);
    ExitCriticalSection();
    EnableEvent(g_CardSwEvents);
    EnableEvent(g_CardSwEvTimeout);
    EnableEvent(g_CardSwEvNew);
    EnableEvent(g_CardSwEvError);
    EnableEvent(g_CardHwEvents);
    EnableEvent(g_CardHwEvTimeout);
    EnableEvent(g_CardHwEvError);
}

void memcard_clear_sw_events(void) {
    long TestEvent();
    TestEvent(g_CardSwEvents);
    TestEvent(g_CardSwEvTimeout);
    TestEvent(g_CardSwEvNew);
    TestEvent(g_CardSwEvError);
}

void memcard_clear_hw_events(void) {
    long TestEvent();
    TestEvent(g_CardHwEvents);
    TestEvent(g_CardHwEvTimeout);
    TestEvent(g_CardHwEvError);
}

s32 memcard_write_file(u8 *buf, s32 slot, char *name, char *title, u32 size) {
    char *strcpy(char *dst, const char *src);
    void *memset_C43(void *s, int c, int n);
    long read(long fd, void *buf, long n);
    long close(long fd);
    long open(char *devname, int flag);
    extern Str6 s_bu00;
    u8 verify[0x200];
    struct {
        Str6 s;
        char rest[14];
    } path;
    u32 tries;
    s32 n;
    s32 fd;
    s32 r;
    u32 len;

    path.s = s_bu00;
    memset_C43(path.rest, 0, 0xE);
    len = size;
    buf[3] = len / 0x2000 + (len % 0x2000 != 0);
    path.s.s[2] = slot + path.s.s[2];
    strcpy(&path.s.s[5], name);
    strcpy(buf + 4, title);
    close(open(path.s.s, (buf[3] << 16) | 0x200));
    tries = 0;
    do {
        n = 0x1E;
        do {
            fd = open(path.s.s, 2);
            if (fd != -1) {
                goto opened;
            }
            VSync(2);
        } while (--n != 0);
        return 1;
opened:
        n = 0x1E;
        do {
            if (write(fd, buf, len) == len) {
                goto written;
            }
            VSync(2);
        } while (--n != 0);
        return 2;
written:
        close(fd);
        fd = open(path.s.s, 1);
        if (fd == -1) {
            break;
        }
        r = read(fd, verify, 0x200);
        close(fd);
        tries++;
        if (r == 0x200) {
            return 0;
        }
    } while (tries < 3);
    return 2;
}

s32 memcard_read_file(u8 *buf, s32 slot, char *name) {
    int VSync(int mode);
    long open(char *devname, int flag);
    long close(long fd);
    long read(long fd, void *buf, long n);
    void *memset_C43(void *s, int c, int n);
    char *strcpy(char *dst, const char *src);
    struct {
        Str6 s;
        char rest[14];
    } path;
    extern Str6 s_bu00;
    s32 n;
    s32 fd;

    /* the do/while (0) block was found by decomp-permuter: it keeps the original scheduling */
    do {
        path.s = s_bu00; /* "bu00:" */
        memset_C43(path.rest, 0, 0xE);
        path.s.s[2] += slot;
        strcpy(&path.s.s[5], name);
    } while (0);
    n = 30;
    do {
        fd = open(path.s.s, 1);
        if (fd != -1) {
            goto opened;
        }
        VSync(1);
    } while (--n != 0);
    return 1;
opened:
    n = 30;
    do {
        if (read(fd, buf, 0x800) == 0x800) {
            close(fd);
            return 0;
        }
        VSync(1);
    } while (--n != 0);
    return 2;
}

s32 memcard_list_files(s32 slot, u8 *info, char *name) {
    struct DIRENTRY *firstfile_FIRST(char *name, struct DIRENTRY *dir);
    struct DIRENTRY *nextfile(struct DIRENTRY *dir);
    void *memset_C43(void *s, int c, int n);
    char *strcpy(char *dst, const char *src);
    extern Str6 s_bu00;
    struct DIRENTRY d;
    struct {
        Str6 s;
        char rest[26];
    } path;

    path.s = s_bu00;
    memset_C43(path.rest, 0, 0x1A);
    strcpy(&path.s.s[5], name);
    path.s.s[2] += slot;
    MC_DIR(info)->count = 0;
    MC_DIR(info)->total_size = 0;
    if (firstfile_FIRST(path.s.s, &d) != &d) {
        return 1;
    }
    {
        do {
            strcpy(info + MC_DIR(info)->count * 0x16, d.name);
            *(s32 *)(info + MC_DIR(info)->count * 4 + 0x150) = d.size;
            MC_DIR(info)->total_size += d.size;
            MC_DIR(info)->count++;
        } while (nextfile(&d) == &d);
    }
    return 0;
}

s32 memcard_load_dir(s32 slot) {
    void memcard_clear_sw_events();
    s32 memcard_wait_sw_event_a1() __asm__("memcard_wait_sw_event");
    long _card_load(long chan);
    s32 i;
    s32 ret;

    i = 3;
    memcard_clear_sw_events();
    do {
        if (_card_load(slot * 0x10) != 0) {
            ret = memcard_wait_sw_event_a1();
            if (ret == 0) {
                return 0;
            }
        }
    } while (--i);
    return ret;
}

s32 memcard_get_status(s32 slot) {
    s32 memcard_wait_sw_event_a2() __asm__("memcard_wait_sw_event");
    s32 memcard_clear_card();
    long _card_info(long chan);
    s32 status;
    s32 tries;
    s32 prev_status;

    status = status;
    tries = 0x10;
    prev_status = 0xFF;
    memcard_clear_sw_events();
loop_1:
    if (_card_info(slot * 0x10) != 0) {
        status = memcard_wait_sw_event_a2();
        if ((status & 1) || (status != prev_status)) {
            prev_status = status;
            goto block_5;
        }
    } else {
block_5:
        tries -= 1;
        if (tries != 0) {
            goto loop_1;
        }
    }
    if (status == 2) {
        status = 0;
    }
    if (status == 0) {
        memcard_clear_card(slot);
    }
    return status;
}


s32 memcard_format(s32 slot) {
    extern Str6 s_bu00;
    long format(char *fs);
    Str6 path;

    path = s_bu00;
    path.s[2] += slot;
    return format(path.s) ^ 1;
}

s32 memcard_wait_sw_event(void) {
    long TestEvent();
    s32 n;

    for (n = 0x3D08F; n != 0; n--) {
        if (TestEvent(g_CardSwEvents)) {
            return 0;
        }
        if (TestEvent(g_CardSwEvTimeout)) {
            return 1;
        }
        if (TestEvent(g_CardSwEvNew)) {
            return 2;
        }
        if (TestEvent(g_CardSwEvError)) {
            return 3;
        }
    }
    return 3;
}

s32 memcard_wait_hw_event(void) {
    long TestEvent(long event);
    s32 n;

    for (n = 0x3D08F; n != 0; n--) {
        if (TestEvent(g_CardHwEvents)) {
            return 0;
        }
        if (TestEvent(g_CardHwEvTimeout)) {
            return 1;
        }
        if (TestEvent(g_CardHwEvError)) {
            return 2;
        }
    }
    return 3;
}

s32 memcard_clear_card(s32 slot) {
    void memcard_clear_hw_events();
    s32 memcard_wait_hw_event();
    long _card_clear(long chan);
    s32 i;
    s32 ret;

    i = 10;
    ret = 2;
    memcard_clear_hw_events();
    do {
        if (_card_clear(slot * 0x10) != 0) {
            ret = memcard_wait_hw_event();
            if (ret == 0) {
                break;
            }
        }
    } while (--i);
    return ret;
}

void memcard_rename_file(s32 unused, char *src_name, char *dst_name) {
    long rename(char *src, char *dest);
    char *strcpy(char *dst, const char *src);
    g_McRenameSrcPort[0] = g_McRenameSrcPort[0] + '0' + g_MemcardSlot;
    g_McRenameDstPort[0] = g_McRenameDstPort[0] + '0' + g_MemcardSlot;
    strcpy(&g_McRenameSrcPort[3], src_name);
    strcpy(&g_McRenameDstPort[3], dst_name);
    rename(&g_McRenameSrcPort[-2], &g_McRenameDstPort[-2]);
}
