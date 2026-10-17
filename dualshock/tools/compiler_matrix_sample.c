/* 15 functions used to identify RE1's compiler: all match with gcc 2.6.3-psx -O2 -G0. */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef unsigned short u16;
typedef int s32; typedef unsigned int u32;

/* --- first batch --- */
s32 func_80014A28(s32 a, s32 b, s32 c, s32 d) { return a * d - b * c; }
s32 func_80014A48(s32 a, s32 b, s32 c, s32 d) { return a * d + b * c; }

extern s32 CdInit(void);
extern u8 D_800C8459;
extern s32 D_800BC5A4;
void func_80015004(s32 arg0);
void func_80014A68(void) {
    while (CdInit() == 0) {
    }
    D_800C8459 = 0;
    func_80015004(0x80);
    D_800BC5A4 = 0;
}

extern u8 D_800BEA2C;
extern s32 D_800BE590;
void func_80015018(s32 arg0);
void func_800150A0(void) { D_800BE590 = 0; func_80015018(D_800BEA2C); }
void func_800150D0(void) { D_800BE590 = 1; func_80015018(D_800BEA2C); }

/* Fixed-address work area (see capcom.c). */
typedef struct { u32 flags; u8 pad[0x3E]; u8 unk42; u8 pad2[0x84 - 0x43]; } Work_800C3000;
#define WORK_800C3000 ((Work_800C3000 *)0x800C3000)
void func_8001515C(void) { WORK_800C3000->flags &= ~0x20000; }

typedef struct { s16 unk0; s16 unk2; s32 unk4; s32 unk8; u8 padC[0x1C]; s16 unk28; } Unk_BAC18;
extern Unk_BAC18 *D_800BAC18;
s32 func_8001A350(void) {
    Unk_BAC18 *p = D_800BAC18;
    p->unk0 = 0; p->unk2 = 0; p->unk4 = 0; p->unk8 = 0; p->unk28 = 0;
    return 0;
}
s32 func_8001A3D0(void) { D_800BAC18->unk4 += 2; return 0; }

extern s32 D_800BC4B0;
s32 func_8001B450(void) { D_800BC4B0 &= ~2; D_800BAC18->unk4 += 2; return 0; }

extern u8 D_800CF86E;
void func_8001BD1C(void) { WORK_800C3000[D_800CF86E].unk42 = 0; }

void func_80023EC4(u32 *p, s32 n) { while (n) { *p++ = 0; n--; } }

void func_80028EE4(u8 *arg0);
void func_80028CDC(u8 *arg0) { arg0[6] = 0; arg0[0x10] = 0; func_80028EE4(arg0); }

extern u8 D_800C3011, D_800C3012, D_800C3013;
void func_8002B3A8(void) { D_800C3013 = 0; D_800C3012 = 0; D_800C3011 = 0; }

typedef struct { u8 pad[0x74]; s16 unk74; s16 unk76; } Unk_744;
extern u8 *D_800C51BC;
/* The pointer is adjusted in a separate statement; GCC 2.6.3 then keeps the +0x744. */
void func_80031A58(void) {
    Unk_744 *p;
    p = (Unk_744 *)D_800C51BC;
    p = (Unk_744 *)((u8 *)p + 0x744);
    p->unk74 = 0;
    p->unk76 = 0;
}
void func_80031EDC(void) {
    u8 *base = D_800C51BC;
    Unk_744 *p;
    base += 0x744;
    p = (Unk_744 *)base;
    if (p->unk74 == 0) { p->unk74 = 1; p->unk76 = 0; }
}
