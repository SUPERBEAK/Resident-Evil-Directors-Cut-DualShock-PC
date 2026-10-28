#include "psx_gte.h"
#include <cmath>
#include <cstring>

// Fixed-point GTE trigonometry (4096 = 1.0)
static constexpr double GTE_ONE = 4096.0;

extern "C" {

static int32_t s_GeomOfx = 160;
static int32_t s_GeomOfy = 120;
static int32_t s_GeomScreen = 256;

void InitGeom() {
    s_GeomOfx = 160;
    s_GeomOfy = 120;
    s_GeomScreen = 256;
}

void SetGeomOffset(int32_t ofx, int32_t ofy) {
    s_GeomOfx = ofx;
    s_GeomOfy = ofy;
}

void SetGeomScreen(int32_t h) {
    s_GeomScreen = h;
}

MATRIX* RotMatrixX(int32_t r, MATRIX* m) {
    double angle = (r * 2.0 * 3.14159265358979323846) / 4096.0;
    int16_t c = static_cast<int16_t>(std::cos(angle) * GTE_ONE);
    int16_t s = static_cast<int16_t>(std::sin(angle) * GTE_ONE);

    m->m[0][0] = 4096; m->m[0][1] = 0;  m->m[0][2] = 0;
    m->m[1][0] = 0;    m->m[1][1] = c;  m->m[1][2] = -s;
    m->m[2][0] = 0;    m->m[2][1] = s;  m->m[2][2] = c;
    return m;
}

MATRIX* RotMatrixY(int32_t r, MATRIX* m) {
    double angle = (r * 2.0 * 3.14159265358979323846) / 4096.0;
    int16_t c = static_cast<int16_t>(std::cos(angle) * GTE_ONE);
    int16_t s = static_cast<int16_t>(std::sin(angle) * GTE_ONE);

    m->m[0][0] = c;    m->m[0][1] = 0;  m->m[0][2] = s;
    m->m[1][0] = 0;    m->m[1][1] = 4096; m->m[1][2] = 0;
    m->m[2][0] = -s;   m->m[2][1] = 0;  m->m[2][2] = c;
    return m;
}

MATRIX* RotMatrixZ(int32_t r, MATRIX* m) {
    double angle = (r * 2.0 * 3.14159265358979323846) / 4096.0;
    int16_t c = static_cast<int16_t>(std::cos(angle) * GTE_ONE);
    int16_t s = static_cast<int16_t>(std::sin(angle) * GTE_ONE);

    m->m[0][0] = c;    m->m[0][1] = -s; m->m[0][2] = 0;
    m->m[1][0] = s;    m->m[1][1] = c;  m->m[1][2] = 0;
    m->m[2][0] = 0;    m->m[2][1] = 0;  m->m[2][2] = 4096;
    return m;
}

MATRIX* RotMatrix(SVECTOR* r, MATRIX* m) {
    MATRIX mx, my, mz, tmp;
    RotMatrixX(r->vx, &mx);
    RotMatrixY(r->vy, &my);
    RotMatrixZ(r->vz, &mz);

    MulMatrix0(&my, &mx, &tmp);
    MulMatrix0(&mz, &tmp, m);
    return m;
}

MATRIX* TransMatrix(MATRIX* m, VECTOR* v) {
    m->t[0] = v->vx;
    m->t[1] = v->vy;
    m->t[2] = v->vz;
    return m;
}

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v) {
    for (int i = 0; i < 3; ++i) {
        m->m[0][i] = static_cast<int16_t>((m->m[0][i] * v->vx) >> 12);
        m->m[1][i] = static_cast<int16_t>((m->m[1][i] * v->vy) >> 12);
        m->m[2][i] = static_cast<int16_t>((m->m[2][i] * v->vz) >> 12);
    }
    return m;
}

MATRIX* MulMatrix0(MATRIX* m0, MATRIX* m1, MATRIX* m2) {
    MATRIX out;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            int32_t sum = (m0->m[r][0] * m1->m[0][c] +
                           m0->m[r][1] * m1->m[1][c] +
                           m0->m[r][2] * m1->m[2][c]) >> 12;
            out.m[r][c] = static_cast<int16_t>(sum);
        }
    }
    out.t[0] = m0->t[0];
    out.t[1] = m0->t[1];
    out.t[2] = m0->t[2];
    std::memcpy(m2, &out, sizeof(MATRIX));
    return m2;
}

MATRIX* CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2) {
    MulMatrix0(m0, m1, m2);
    m2->t[0] = ((m0->m[0][0] * m1->t[0] + m0->m[0][1] * m1->t[1] + m0->m[0][2] * m1->t[2]) >> 12) + m0->t[0];
    m2->t[1] = ((m0->m[1][0] * m1->t[0] + m0->m[1][1] * m1->t[1] + m0->m[1][2] * m1->t[2]) >> 12) + m0->t[1];
    m2->t[2] = ((m0->m[2][0] * m1->t[0] + m0->m[2][1] * m1->t[1] + m0->m[2][2] * m1->t[2]) >> 12) + m0->t[2];
    return m2;
}

int32_t RotTransPers(SVECTOR* v0, int32_t* sxy, int32_t* p, int32_t* flag) {
    if (flag) *flag = 0;
    // Basic fallback projection; expanded in renderer pipeline
    int32_t z = v0->vz + 1;
    if (z <= 0) z = 1;
    int16_t sx = static_cast<int16_t>((v0->vx * s_GeomScreen / z) + s_GeomOfx);
    int16_t sy = static_cast<int16_t>((v0->vy * s_GeomScreen / z) + s_GeomOfy);
    if (sxy) *sxy = (sy << 16) | (sx & 0xFFFF);
    if (p) *p = z;
    return z;
}

} // extern "C"
