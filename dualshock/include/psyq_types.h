/*
 * psyq_types.h - Sony PsyQ library TYPES used by RE1's Capcom code (prototypes of the 188
 * functions are in psyq_protos.h;
 * dualshock/tools/port_inventory.py --list), written for this project from the documented
 * PlayStation library interface (types, struct layouts, prototypes).
 *
 * Uses:
 *   - context for m2c (--context) and the matching build: the original code was compiled
 *     with Sony's prototypes, which affects argument/return conversions at -O2.
 *
 * Struct layouts follow the PS1 ABI (little-endian, 4-byte longs/pointers).
 */
#ifndef PSYQ_TYPES_H
#define PSYQ_TYPES_H

typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef unsigned long  u_long;

/* ---------------------------------------------------------------- libgte */
typedef struct { short m[3][3]; long t[3]; } MATRIX;          /* 0x20 */
typedef struct { long vx, vy, vz, pad; } VECTOR;              /* 0x10 */
typedef struct { short vx, vy, vz, pad; } SVECTOR;            /* 0x08 */
typedef struct { u_char r, g, b, cd; } CVECTOR;               /* 0x04 */
typedef struct { short vx, vy; } DVECTOR;                     /* 0x04 */


/* ---------------------------------------------------------------- libgpu */
typedef struct { short x, y, w, h; } RECT;                    /* 0x08 */

typedef struct {                                              /* 0x5C */
    RECT clip;
    short ofs[2];
    RECT tw;
    u_short tpage;
    u_char dtd, dfe, isbg, r0, g0, b0;
    u_long dr_env[16];                                        /* DR_ENV: tag + 15 words */
} DRAWENV;

typedef struct {                                              /* 0x14 */
    RECT disp, screen;
    u_char isinter, isrgb24, pad0, pad1;
} DISPENV;

typedef struct { u_long tag; u_long code[2]; } DR_MODE;
typedef struct { u_long tag; u_char r0, g0, b0, code; short x0, y0, w, h; } TILE;
typedef struct { u_long tag; u_char r0, g0, b0, code; short x0, y0, x1, y1, x2, y2, x3, y3; } POLY_F4;
typedef struct {
    u_long tag; u_char r0, g0, b0, code;
    short x0, y0; u_char u0, v0; u_short clut;
    short x1, y1; u_char u1, v1; u_short tpage;
    short x2, y2; u_char u2, v2; u_short pad1;
    short x3, y3; u_char u3, v3; u_short pad2;
} POLY_FT4;
typedef struct {
    u_long tag;
    u_char r0, g0, b0, code; short x0, y0; u_char u0, v0; u_short clut;
    u_char r1, g1, b1, p1;   short x1, y1; u_char u1, v1; u_short tpage;
    u_char r2, g2, b2, p2;   short x2, y2; u_char u2, v2; u_short pad2;
} POLY_GT3;


/* ---------------------------------------------------------------- libgs */
typedef struct { u_long p : 24; u_char num : 8; } GsOT_TAG;
typedef struct {
    u_long length;
    GsOT_TAG *org;
    u_long offset;
    u_long point;
    GsOT_TAG *tag;
} GsOT;                                                       /* 0x14 */

typedef struct GsCOORDINATE2 {
    u_long flg;
    MATRIX coord;
    MATRIX workm;
    struct GsCOORD_PARAM *param;
    struct GsCOORDINATE2 *super;
    struct GsCOORDINATE2 *sub;
} GsCOORDINATE2;                                              /* 0x50 */

typedef struct {
    u_long attribute;
    GsCOORDINATE2 *coord2;
    u_long *tmd;
    u_long *packet;
    u_long id;
} GsDOBJ5;                                                    /* 0x14 */

typedef struct { long vpx, vpy, vpz, vrx, vry, vrz, rz; GsCOORDINATE2 *super; } GsRVIEW2;
typedef struct { long vx, vy, vz; u_char r, g, b; } GsF_LIGHT;
typedef struct {
    u_long attribute;
    short x, y;
    u_short w, h;
    u_short tpage;
    u_char u, v;
    short cx, cy;
    u_char r, g, b;
    short mx, my;
    short scalex, scaley;
    long rotate;
} GsSPRITE;                                                   /* 0x24 */
typedef struct { u_long attribute; short x, y; u_short w, h; u_char r, g, b; } GsBOXF;
typedef struct { u_long attribute; short x0, y0, x1, y1; u_char r, g, b; } GsLINE;
typedef struct { u_long attribute; short x0, y0, x1, y1; u_char r0, g0, b0, r1, g1, b1; } GsGLINE;
typedef struct {
    u_long pmode;
    short px, py;
    u_short pw, ph;
    u_long *pixel;
    short cx, cy;
    u_short cw, ch;
    u_long *clut;
} GsIMAGE;


/* ---------------------------------------------------------------- libetc / libapi / libc */


struct DIRENTRY {
    char name[20];
    long attr;
    long size;
    struct DIRENTRY *next;
    long head;
    char system[4];
};
typedef struct { u_char minute, second, sector, track; } CdlLOC;
typedef struct { CdlLOC pos; u_long size; char name[16]; } CdlFILE;


/* ---------------------------------------------------------------- libpress (MDEC) */

/* ---------------------------------------------------------------- libsnd / libspu */

/* ---------------------------------------------------------------- libpad (Dual Shock) */

/* ---------------------------------------------------------------- libsn (dev-kit host files) */

#endif /* PSYQ_TYPES_H */
