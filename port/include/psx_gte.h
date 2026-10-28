#pragma once
#include <cstdint>

// Sony PsyQ GTE Types
extern "C" {

typedef struct {
    int16_t m[3][3];
    int32_t t[3];
} MATRIX;

typedef struct {
    int16_t vx, vy, vz;
    int16_t pad;
} SVECTOR;

typedef struct {
    int32_t vx, vy, vz;
    int32_t pad;
} VECTOR;

typedef struct {
    uint8_t r, g, b, cd;
} CVECTOR;

// GTE API Prototypes
void InitGeom();
void SetGeomOffset(int32_t ofx, int32_t ofy);
void SetGeomScreen(int32_t h);

MATRIX* RotMatrix(SVECTOR* r, MATRIX* m);
MATRIX* RotMatrixX(int32_t r, MATRIX* m);
MATRIX* RotMatrixY(int32_t r, MATRIX* m);
MATRIX* RotMatrixZ(int32_t r, MATRIX* m);
MATRIX* TransMatrix(MATRIX* m, VECTOR* v);
MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix0(MATRIX* m0, MATRIX* m1, MATRIX* m2);
MATRIX* CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2);

int32_t RotTransPers(SVECTOR* v0, int32_t* sxy, int32_t* p, int32_t* flag);

} // extern "C"
