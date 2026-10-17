/* task.c - The cooperative task scheduler (task_execute / task_sleep / task_exit ...), overlay loading, FMV play. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void room_setup_camera(void);
void load_room_bg_image(void);
void load_file(s32 a, s32 b);
void load_file_blocking(void);


typedef struct {
    s16 state;
    u8 pad2[0xA];
    u8 *tcb;
    s32 stack;
    u8 pad14[0x30];
    s32 gp;
    u8 pad48[0x38];
} TaskSlotView;

void task_scheduler_init(void) {
    extern TaskSlotView * g_TaskCur_Rec37014p __asm__("g_TaskCur");
    u_long GetGp(void);
    s32 i;
    s16 *p0;
    s32 *p44;
    s32 off;
    u8 *q;
    u8 *k;
    s32 c;
    s32 *dst;
    s32 *src;

    k = (u8 *)0x100;
    i = 0;
    c = 0x404;
    p44 = &LITP(TaskSlot, g_Tasks)[0].gp;
    off = 0xC0;
    p0 = LITP(s16, g_Tasks);
    for (; i < 3; i++) {
        *p0 = 0;
        p0 += 0x40;
        q = (u8 *)(off + BIOS_TOT(k)->tcb);
        p44[-0xE] = (s32)q;
        BIOS_TCB(q)->sr = c;
        *p44 = GetGp();
        p44 += 0x20;
        off += 0xC0;
    }
    g_TaskCur_Rec37014p = LITP(TaskSlotView, g_Tasks);
    g_TaskCur_Rec37014p->stack = ADDR_g_Task0StackTop;
    g_TaskCur_Rec37014p = &LITP(TaskSlotView, g_Tasks)[1];
    g_TaskCur_Rec37014p->stack = ADDR_g_Task1StackTop;
    g_TaskCur_Rec37014p = &LITP(TaskSlotView, g_Tasks)[2];
    g_TaskCur_Rec37014p->stack = ADDR_g_Task2StackTop;
    dst = LITP(s32, g_OverlayApiTable);
    i = 0;
    src = g_OverlayApiTableSrc;
    for (; i < 0xB7; i++) {
        *dst++ = *src++;
    }
    g_MainGp = GetGp();
}

void task_scheduler_update(void) {
    long EnterCriticalSection(void);
    void ExitCriticalSection(void);
    u_long OpenTh(long (*func)(), u_long sp, u_long gp);
    int ChangeTh(u_long thread);
    u8 *p;
    u8 *q;

    g_CurrentTask = LITP(u8, g_Tasks);
    g_TaskCur = ADDR_g_Tasks;
    do {
        p = LIT(u8 *, g_CurrentTask);
        switch (*(u16 *)p) {
        case 2:
            EnterCriticalSection();
            q = LIT(u8 *, g_CurrentTask);
            *(u32 *)(LIT(u8 *, g_CurrentTask) + 8) = OpenTh(*(long (**)())(&TASK(q)->func), ((u32) TASK(q)->stack), ((u32) TASK(q)->gp));
            ExitCriticalSection();
            q = LIT(u8 *, g_CurrentTask);
            *(u16 *)q = 0x7F;
            ChangeTh(((u32) TASK(q)->thread));
            break;
        case 1:
            if ((--TASK(p)->sleep_count << 16) != 0) {
                break;
            }
            /* fallthrough */
        case 4:
            q = g_CurrentTask;
            *(u16 *)q = 0x7F;
            ChangeTh(((u32) TASK(q)->thread));
            break;
        }
        LIT(u8 *, g_CurrentTask) += 0x80;
    } while (LIT(u32, g_CurrentTask) <= ADDR_g_Tasks_LastByte);
}


void load_stage_overlay(s32 slot, s32 file) {
    s32 load_file_p() __asm__("load_file");
    void task_execute();
    s32 mem_copy();
    s32 size;
    CdFileEntry *p;
    u16 t;

    size = g_CdFileTable[file].size;
    g_CdFileTable[file].size = 0x800;
    LIT(u8, g_SkipCdChecksum) = 1;
    load_file_p(file, ADDR_g_ExeHeaderBuf);
    LIT(u8, g_SkipCdChecksum) = 0;
    mem_copy(&LITP(TaskSlot, g_Tasks)[slot].ovl_entry, ADDR_g_ExeHeaderBuf_Exec, 0x3C);
    p = &g_CdFileTable[file];
    t = g_CdFileTable[file].lba + 1;
    g_CdFileTable[file].size = size - 0x800;
    p->lba = t;
    load_file_p(file, LITP(TaskSlot, g_Tasks)[slot].ovl_load_addr);
    p->lba = g_CdFileTable[file].lba - 1;
    g_CdFileTable[file].size = size;
    LITP(TaskSlot, g_Tasks)[slot].gp = g_MainGp;
    task_execute(slot, LITP(TaskSlot, g_Tasks)[slot].ovl_entry);
}


void load_and_schedule_overlay(s32 file) {
    void load_file_blocking_l() __asm__("load_file_blocking");
    void task_chain_l() __asm__("task_chain");
    s32 size;
    s32 gp;
    CdFileEntry *entry;
    void *task;
    s16 t;

    size = g_CdFileTable[file].size;
    gp = TASK(LIT(void *, g_CurrentTask))->gp;
    g_CdFileTable[file].size = 0x800;
    LIT(s8, g_SkipCdChecksum) = 1;
    load_file_blocking_l(file, ADDR_g_ExeHeaderBuf);
    LIT(s8, g_SkipCdChecksum) = 0;
    mem_copy((u8 *)LIT(void *, g_CurrentTask) + 0x14, ADDR_g_ExeHeaderBuf_Exec, 0x3C);
    entry = &g_CdFileTable[file];
    t = g_CdFileTable[file].lba + 1;
    g_CdFileTable[file].size = size - 0x800;
    entry->lba = t;
    load_file_blocking_l(file, TASK(LIT(void *, g_CurrentTask))->ovl_load_addr);
    entry->lba = g_CdFileTable[file].lba - 1;
    g_CdFileTable[file].size = size;
    task = LIT(void *, g_CurrentTask);
    TASK(task)->gp = gp;
    task_chain_l(TASK(task)->ovl_entry);
}

void task_execute(s32 slot, s32 func) {
    LITP(TaskSlot, g_Tasks)[slot].func = func;
    LITP(TaskSlot, g_Tasks)[slot].state = 2;
}

void task_sleep(s16 frames) {
    int ChangeTh(u_long thread);
    void *task;

    task = LIT(void *, g_CurrentTask);
    TASK(task)->sleep_count = frames;
    FIELD_AT(task, TaskSlot, ->state, s16) = 1;
    __asm__ volatile("move $8, %0\n\tsw $31, 0($8)" : : "r"(&g_TaskSleepRa) : "$8");
    FIELD_AT(LIT(void *, g_CurrentTask), TaskSlot, ->func, s32) = g_TaskSleepRa;
    ChangeTh(0xFF000000U);
}

void task_exit(void) {
    int ChangeTh(u_long thread);
    s32 CloseTh(s32);
    long EnterCriticalSection(void);
    void ExitCriticalSection(void);
    TASK(LIT(void *, g_CurrentTask))->state = 0;
    EnterCriticalSection();
    CloseTh(TASK(LIT(void *, g_CurrentTask))->thread);
    ExitCriticalSection();
    ChangeTh(0xFF000000);
}

void task_kill(s32 slot) {
    long EnterCriticalSection(void);
    void ExitCriticalSection(void);
    s32 CloseTh(s32);
    u16 *p = &LITP(TaskSlot, g_Tasks)[slot].state;

    if (*p != 0) {
        *p = 0;
        EnterCriticalSection();
        CloseTh(LITP(TaskSlot, g_Tasks)[slot].thread);
        ExitCriticalSection();
    }
}

void task_chain(s32 func) {
    int ChangeTh();
    s32 CloseTh(s32);
    long EnterCriticalSection();
    void ExitCriticalSection();
    void *task;

    task = LIT(void *, g_CurrentTask);
    TASK(task)->state = 2;
    TASK(task)->func = func;
    EnterCriticalSection();
    CloseTh(TASK(LIT(void *, g_CurrentTask))->thread);
    ExitCriticalSection();
    ChangeTh(0xFF000000);
}

void task_suspend(s32 slot) {
    s32 base = ADDR_g_Tasks;

    slot = (slot << 7) + base;
    *(u16 *)slot |= 0x40;
}

typedef struct {
    u16 flags;
    u8 pad[126];
} TaskSlotFlagsView;

void task_resume(s32 slot) {
    TaskSlotFlagsView *p = LITP(TaskSlotFlagsView, g_Tasks);

    p[slot].flags &= 0xFFBF;
}

u16 task_get_state(s32 slot) {
    return *(s32 *)((u8 *)&LIT(s8, g_Tasks) + ((slot << 7)));
}

typedef struct {
    s32 cd_file;
    u16 end_frame;
    u16 ring_sectors;
    u16 unused_08;
    u16 flags;
    u32 vlc_buf_size;
    u16 skip_buttons;
    u16 pad12;
} MovieEntry;

void fmv_play(u32 movie_id, s32 work_buf) {
    void ClearImage(RECT *rect, u_char r, u_char g, u_char b);
    int DrawSync(int mode);
    int LoadImage(RECT *rect, u_long *p);
    int GsGetActiveBuff(void);
    void GsInit3D(void);
    void GsInitGraph2(u_short x, u_short y, u_short intmode, u_short dith, u_short varmmode);
    void GsSwapDispBuff(void);
    int VSync(int mode);
    void SsSetSerialAttr(char s_num, char attr, char mode);
    extern MovieEntry g_MovieTable_l[] __asm__("g_MovieTable");
    void cd_set_stereo(s32);
    void str_close(void);
    void str_wait_decode(void);
    void str_seek_frame(s32);
    void cd_mix_fade_in(void);
    void cd_mix_fade_out(void);
    void pad_update(void);
    RECT r;
    s32 unused[2];
    s32 done;
    s32 mode;
    u32 t;
    s32 buf;
    s32 small;

    cd_set_mix_volume(0x80);
    cd_set_stereo((g_KeyConfigType >> 7) ^ 1);
    small = ({ (u32)(movie_id - 2) < 13; });
    SsSetSerialAttr(0, 0, 1);
    mode = 0;
retry:
    if (str_open(g_MovieTable_l[movie_id].cd_file, 2, g_MovieTable_l[movie_id].end_frame, work_buf, g_MovieTable_l[movie_id].ring_sectors,
                        work_buf + (g_MovieTable_l[movie_id].ring_sectors << 11),
                        work_buf + (g_MovieTable_l[movie_id].ring_sectors << 11) + (g_MovieTable_l[movie_id].vlc_buf_size >> 2) * 4,
                        ({ s32 b_ = work_buf + (g_MovieTable_l[movie_id].ring_sectors << 11); (s32)(g_MovieTable_l[movie_id].vlc_buf_size >> 2) * 8 + b_; }),
                        g_MovieTable_l[movie_id].flags) == 1) {
        str_close();
        goto retry;
    }
    if (small) {
        GsInitGraph2(0x100, 0xF0, 0, 0, 1);
    }
    do {
        done = str_decode_next_frame(mode, -(GsGetActiveBuff() != 0) & 0xF0);
        str_wait_decode();
        VSync(0);
        t = str_get_frame();
        if (t - 0x97E < 10 && (g_SavePlayerId & 3) == 1 && movie_id == 1) {
            mode = 0x400;
            cd_mix_fade_out();
            g_FmvLastId = movie_id;
            str_seek_frame(0xA15);
        }
        if (t >= 0xA16 && (g_SavePlayerId & 3) == 1 && movie_id == 1 && g_FmvLastId == movie_id) {
            cd_mix_fade_in();
            g_FmvLastId = 0;
        }
        if (t >= 0xA1E && (g_SavePlayerId & 3) == 1 && movie_id == 1 && (mode & 0x400)) {
            mode = 0x800;
        }
        VSync(0);
        GsSwapDispBuff();
        pad_update();
        if (movie_id < 14) {
            if (g_PadTrigger & g_MovieTable_l[movie_id].skip_buttons) {
                done = 1;
            }
        } else if ((g_PadTrigger & g_MovieTable_l[movie_id].skip_buttons) && t >= 0x4C) {
            done = 1;
        }
    } while (done == 0);
    if (movie_id == 0) {
        str_close();
        VSync(0);
        buf = GsGetActiveBuff();
        GsInitGraph2(0x140, 0xF0, 0, 0, 0);
        GsInit3D();
        VSync(0);
        r.x = 0;
        if (buf != 0) {
            r.y = 0xF0;
        } else {
            r.y = 0;
        }
        r.w = 0x140;
        r.h = 0xF0;
        LoadImage(&r, LITP(u_long, g_DoorFile));
        DrawSync(0);
        VSync(0);
        GsSwapDispBuff();
        buf = GsGetActiveBuff();
        r.x = 0;
        if (buf != 0) {
            r.y = 0xF0;
        } else {
            r.y = 0;
        }
        r.w = 0x140;
        r.h = 0xF0;
        LoadImage(&r, LITP(u_long, g_DoorFile));
        DrawSync(0);
    } else if ((u32)(movie_id - 2) < 12) {
        str_close();
        VSync(0);
        load_room_bg_image();
        buf = GsGetActiveBuff();
        GsInitGraph2(0x140, 0xF0, 0, 0, 0);
        GsInit3D();
        VSync(0);
        r.x = 0;
        if (buf != 0) {
            r.y = 0xF0;
        } else {
            r.y = 0;
        }
        r.w = 0x140;
        r.h = 0xF0;
        if (movie_id != 2) {
            ClearImage(&r, 0, 0, 0);
        } else {
            ClearImage(&r, 0xFF, 0xFF, 0xFF);
        }
        DrawSync(0);
        VSync(0);
        GsSwapDispBuff();
        buf = GsGetActiveBuff();
        r.x = 0;
        if (buf != 0) {
            r.y = 0xF0;
        } else {
            r.y = 0;
        }
        r.w = 0x140;
        r.h = 0xF0;
        if (movie_id != 2) {
            ClearImage(&r, 0, 0, 0);
        } else {
            ClearImage(&r, 0xFF, 0xFF, 0xFF);
        }
        DrawSync(0);
        if (movie_id != 0) {
            room_setup_camera();
        }
    } else {
        buf = GsGetActiveBuff();
        VSync(0);
        r.x = 0;
        if (buf != 0) {
            r.y = 0xF0;
        } else {
            r.y = 0;
        }
        r.w = 0x1E0;
        r.h = 0xF0;
        ClearImage(&r, 0, 0, 0);
        DrawSync(0);
        VSync(0);
        GsSwapDispBuff();
        buf = GsGetActiveBuff();
        r.x = 0;
        if (buf != 0) {
            r.y = 0xF0;
        } else {
            r.y = 0;
        }
        r.w = 0x1E0;
        r.h = 0xF0;
        ClearImage(&r, 0, 0, 0);
        DrawSync(0);
        str_close();
    }
}
