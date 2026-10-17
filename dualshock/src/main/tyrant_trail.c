/* tyrant_trail.c - the Tyrant's trail effect: allocating and pushing trail entries */

#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

typedef struct {
    DR_MODE dm;
    POLY_F4 pf;
} DmF4;
typedef struct {
    DmF4 q[4];
    u8 pad[0x70];
} DmF4x4;

void tyrant_trail_alloc(u8 n, DmF4x4 *prims, u32 col) {
    int GetGraphType(void);
    void MargePrim(void *p0, void *p1);
    void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
    void SetPolyF4(POLY_F4 *p);
    void SetSemiTrans(void *p, int abe);
    DmF4x4 *p;

    p = prims;
    LIT(s32, sp_tmp00) = n;
    if (--LIT(s32, sp_tmp00) != -1) {
        do {
            SetPolyF4(&p[LIT(s32, sp_tmp00)].q[0].pf);
            SetSemiTrans(&p[LIT(s32, sp_tmp00)].q[0].pf, 1);
            SetDrawMode(&p[LIT(s32, sp_tmp00)].q[0].dm, 0, 0,
                        (GetGraphType() == 1 || GetGraphType() == 2) ? 0x200 : 0x80, NULL);
            p[LIT(s32, sp_tmp00)].q[0].pf.r0 = col + LIT(s32, sp_tmp00) * 8;
            p[LIT(s32, sp_tmp00)].q[0].pf.g0 = ((col & 0xFF00) >> 8) + LIT(s32, sp_tmp00) * 16;
            p[LIT(s32, sp_tmp00)].q[0].pf.b0 = ({ (col >> 16) & 0xFF; }) + LIT(s32, sp_tmp00) * 16;
            MargePrim(&p[LIT(s32, sp_tmp00)].q[0].dm, &p[LIT(s32, sp_tmp00)].q[0].pf);

            SetPolyF4(&p[LIT(s32, sp_tmp00)].q[1].pf);
            SetSemiTrans(&p[LIT(s32, sp_tmp00)].q[1].pf, 1);
            SetDrawMode(&p[LIT(s32, sp_tmp00)].q[1].dm, 0, 0,
                        (GetGraphType() == 1 || GetGraphType() == 2) ? 0x200 : 0x80, NULL);
            p[LIT(s32, sp_tmp00)].q[1].pf.r0 = col + LIT(s32, sp_tmp00) * 8;
            p[LIT(s32, sp_tmp00)].q[1].pf.g0 = ((col & 0xFF00) >> 8) + LIT(s32, sp_tmp00) * 16;
            p[LIT(s32, sp_tmp00)].q[1].pf.b0 = ({ (col >> 16) & 0xFF; }) + LIT(s32, sp_tmp00) * 16;
            MargePrim(&p[LIT(s32, sp_tmp00)].q[1].dm, &p[LIT(s32, sp_tmp00)].q[1].pf);

            SetPolyF4(&p[LIT(s32, sp_tmp00)].q[2].pf);
            SetSemiTrans(&p[LIT(s32, sp_tmp00)].q[2].pf, 1);
            SetDrawMode(&p[LIT(s32, sp_tmp00)].q[2].dm, 0, 0,
                        (GetGraphType() == 1 || GetGraphType() == 2) ? 0x200 : 0x80, NULL);
            p[LIT(s32, sp_tmp00)].q[2].pf.r0 = col + LIT(s32, sp_tmp00) * 8;
            p[LIT(s32, sp_tmp00)].q[2].pf.g0 = ((col & 0xFF00) >> 8) + LIT(s32, sp_tmp00) * 16;
            p[LIT(s32, sp_tmp00)].q[2].pf.b0 = ({ (col >> 16) & 0xFF; }) + LIT(s32, sp_tmp00) * 16;
            MargePrim(&p[LIT(s32, sp_tmp00)].q[2].dm, &p[LIT(s32, sp_tmp00)].q[2].pf);

            SetPolyF4(&p[LIT(s32, sp_tmp00)].q[3].pf);
            SetSemiTrans(&p[LIT(s32, sp_tmp00)].q[3].pf, 1);
            SetDrawMode(&p[LIT(s32, sp_tmp00)].q[3].dm, 0, 0,
                        (GetGraphType() == 1 || GetGraphType() == 2) ? 0x200 : 0x80, NULL);
            p[LIT(s32, sp_tmp00)].q[3].pf.r0 = col + LIT(s32, sp_tmp00) * 8;
            p[LIT(s32, sp_tmp00)].q[3].pf.g0 = ((col & 0xFF00) >> 8) + LIT(s32, sp_tmp00) * 16;
            p[LIT(s32, sp_tmp00)].q[3].pf.b0 = ({ (col >> 16) & 0xFF; }) + LIT(s32, sp_tmp00) * 16;
            MargePrim(&p[LIT(s32, sp_tmp00)].q[3].dm, &p[LIT(s32, sp_tmp00)].q[3].pf);
        } while (--LIT(s32, sp_tmp00) != -1);
    }
    LIT(s32, g_LoadPointer) += n << 8;
}

typedef struct {
    u8 hdr[0xC];
    POLY_F4 p;
} TrlPrim;
typedef struct {
    /* 0x00 */ TrlPrim a[2];
    /* 0x48 */ TrlPrim b[2];
    /* 0x90 */ MATRIX A;
    /* 0xB0 */ MATRIX B;
    /* 0xD0 */ MATRIX M;
    /* 0xF0 */ SVECTOR C;
    /* 0xF8 */ SVECTOR D;
} TrlRec;

void tyrant_trail_push(TrlRec *recs, MATRIX *ma, MATRIX *mb, SVECTOR *c, SVECTOR *d, u8 idx, u8 n) {
    SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
    void SetRotMatrix(MATRIX *m);
    void SetTransMatrix(MATRIX *m);
    long RotAverage4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1, long *sxy2, long *sxy3, long *p, long *flag);
    void AddPrim(void *ot, void *p);
    /* copied into a local: changes the prologue scheduling of the a0 copy */
    TrlRec *base = recs;
    SVECTOR s28;
    SVECTOR s30;
    SVECTOR s38;
    SVECTOR s40;
    long p;
    long flag;
    TrlRec *r1;
    TrlRec *r2;
    TrlRec *r3;
    TrlRec *r4;
    TrlRec *r5;
    TrlRec *r6;
    u32 otz;
    s32 k;

    if (n == 0) {
        r1 = (TrlRec *)((idx << 8) + (s32)base);
        r1->A = *ma;
        r1->B = *mb;
        r1->M.m[0][0] = ma->m[0][0] + (mb->m[0][0] - ma->m[0][0]) / 2;
        r1->M.m[0][1] = ma->m[0][1] + (mb->m[0][1] - ma->m[0][1]) / 2;
        r1->M.m[0][2] = ma->m[0][2] + (mb->m[0][2] - ma->m[0][2]) / 2;
        r1->M.m[1][0] = ma->m[1][0] + (mb->m[1][0] - ma->m[1][0]) / 2;
        r1->M.m[1][1] = ma->m[1][1] + (mb->m[1][1] - ma->m[1][1]) / 2;
        r1->M.m[1][2] = ma->m[1][2] + (mb->m[1][2] - ma->m[1][2]) / 2;
        r1->M.m[2][0] = ma->m[2][0] + (mb->m[2][0] - ma->m[2][0]) / 2;
        r1->M.m[2][1] = ma->m[2][1] + (mb->m[2][1] - ma->m[2][1]) / 2;
        r1->M.m[2][2] = ma->m[2][2] + (mb->m[2][2] - ma->m[2][2]) / 2;
        r1->M.t[0] = (mb->t[0] - ma->t[0]) / 2 + ma->t[0];
        r1->M.t[1] = (mb->t[1] - ma->t[1]) / 2 + ma->t[1];
        r1->M.t[2] = (mb->t[2] - ma->t[2]) / 2 + ma->t[2];
        r1->C = *c;
        r1->D = *d;
        return;
    }
    LIT(s32, sp_tmp00) = n - 1;
    do {
        base[LIT(s32, sp_tmp00) + 1].A = base[LIT(s32, sp_tmp00)].A;
        base[LIT(s32, sp_tmp00) + 1].B = base[LIT(s32, sp_tmp00)].B;
        base[LIT(s32, sp_tmp00) + 1].M = base[LIT(s32, sp_tmp00)].M;
    } while (--LIT(s32, sp_tmp00) != -1);
    base->A = *ma;
    base->B = *mb;
    base->M.m[0][0] = ma->m[0][0] + (mb->m[0][0] - ma->m[0][0]) / 2;
    base->M.m[0][1] = ma->m[0][1] + (mb->m[0][1] - ma->m[0][1]) / 2;
    base->M.m[0][2] = ma->m[0][2] + (mb->m[0][2] - ma->m[0][2]) / 2;
    base->M.m[1][0] = ma->m[1][0] + (mb->m[1][0] - ma->m[1][0]) / 2;
    base->M.m[1][1] = ma->m[1][1] + (mb->m[1][1] - ma->m[1][1]) / 2;
    base->M.m[1][2] = ma->m[1][2] + (mb->m[1][2] - ma->m[1][2]) / 2;
    base->M.m[2][0] = ma->m[2][0] + (mb->m[2][0] - ma->m[2][0]) / 2;
    base->M.m[2][1] = ma->m[2][1] + (mb->m[2][1] - ma->m[2][1]) / 2;
    base->M.m[2][2] = ma->m[2][2] + (mb->m[2][2] - ma->m[2][2]) / 2;
    base->M.t[0] = (mb->t[0] - ma->t[0]) / 2 + ma->t[0];
    base->M.t[1] = (mb->t[1] - ma->t[1]) / 2 + ma->t[1];
    base->M.t[2] = (mb->t[2] - ma->t[2]) / 2 + ma->t[2];
    k = n - 1;
    LIT(s32, sp_tmp00) = k;
    r2 = &base[k];
    ApplyMatrixSV(&r2[1].B, &r2[1].C, &s38);
    s38.vx += base[LIT(s32, sp_tmp00) + 1].B.t[0];
    s38.vy += base[LIT(s32, sp_tmp00) + 1].B.t[1];
    s38.vz += base[LIT(s32, sp_tmp00) + 1].B.t[2];
    ApplyMatrixSV(&(&base[LIT(s32, sp_tmp00)])[1].A, &(&base[LIT(s32, sp_tmp00)])[1].D, &s40);
    s40.vx += base[LIT(s32, sp_tmp00) + 1].B.t[0];
    s40.vy += base[LIT(s32, sp_tmp00) + 1].B.t[1];
    s40.vz += base[LIT(s32, sp_tmp00) + 1].B.t[2];
    do {
        base[LIT(s32, sp_tmp00)].C.vy += 50;
        base[LIT(s32, sp_tmp00)].D.vy += 100;
        ApplyMatrixSV(&base[LIT(s32, sp_tmp00)].M, &base[LIT(s32, sp_tmp00)].C, &s28);
        s28.vx += base[LIT(s32, sp_tmp00)].M.t[0];
        s28.vy += base[LIT(s32, sp_tmp00)].M.t[1];
        s28.vz += base[LIT(s32, sp_tmp00)].M.t[2];
        ApplyMatrixSV(&base[LIT(s32, sp_tmp00)].M, &base[LIT(s32, sp_tmp00)].D, &s30);
        s30.vx += base[LIT(s32, sp_tmp00)].M.t[0];
        s30.vy += base[LIT(s32, sp_tmp00)].M.t[1];
        s30.vz += base[LIT(s32, sp_tmp00)].M.t[2];
        base[LIT(s32, sp_tmp00)].C.vy -= 50;
        base[LIT(s32, sp_tmp00)].D.vy -= 100;
        SetRotMatrix(LIT(MATRIX *, g_pWorldScreenMatrix));
        SetTransMatrix(LIT(MATRIX *, g_pWorldScreenMatrix));
        r3 = &base[LIT(s32, sp_tmp00)];
        otz = RotAverage4(&s28, &s30, &s38, &s40,
                          (long *)&r3->a[LIT(u8, g_ActiveBuffer)].p.x0, (long *)&r3->a[LIT(u8, g_ActiveBuffer)].p.x1,
                          (long *)&r3->a[LIT(u8, g_ActiveBuffer)].p.x2, (long *)&r3->a[LIT(u8, g_ActiveBuffer)].p.x3,
                          &p, &flag);
        if (otz >= 0xFF0) {
            otz = 0xFF0;
        }
        r4 = &base[LIT(s32, sp_tmp00)];
        AddPrim(LITP(GsOT, g_OtMain)[LIT(u8, g_ActiveBuffer)].org + (otz >> 4), &r4->a[LIT(u8, g_ActiveBuffer)]);
        r5 = &base[LIT(s32, sp_tmp00)];
        ApplyMatrixSV(&r5->B, &r5->C, &s38);
        s38.vx += base[LIT(s32, sp_tmp00)].B.t[0];
        s38.vy += base[LIT(s32, sp_tmp00)].B.t[1];
        s38.vz += base[LIT(s32, sp_tmp00)].B.t[2];
        ApplyMatrixSV(&base[LIT(s32, sp_tmp00)].B, &base[LIT(s32, sp_tmp00)].D, &s40);
        s40.vx += base[LIT(s32, sp_tmp00)].B.t[0];
        s40.vy += base[LIT(s32, sp_tmp00)].B.t[1];
        s40.vz += base[LIT(s32, sp_tmp00)].B.t[2];
        SetRotMatrix(LIT(MATRIX *, g_pWorldScreenMatrix));
        SetTransMatrix(LIT(MATRIX *, g_pWorldScreenMatrix));
        r6 = &base[LIT(s32, sp_tmp00)];
        otz = RotAverage4(&s28, &s30, &s38, &s40,
                          (long *)&r6->b[LIT(u8, g_ActiveBuffer)].p.x0, (long *)&r6->b[LIT(u8, g_ActiveBuffer)].p.x1,
                          (long *)&r6->b[LIT(u8, g_ActiveBuffer)].p.x2, (long *)&r6->b[LIT(u8, g_ActiveBuffer)].p.x3,
                          &p, &flag);
        if (otz >= 0xFF0) {
            otz = 0xFF0;
        }
        r4 = &base[LIT(s32, sp_tmp00)];
        AddPrim(LITP(GsOT, g_OtMain)[LIT(u8, g_ActiveBuffer)].org + (otz >> 4), &r4->b[LIT(u8, g_ActiveBuffer)]);
    } while (--LIT(s32, sp_tmp00) != -1);
}
