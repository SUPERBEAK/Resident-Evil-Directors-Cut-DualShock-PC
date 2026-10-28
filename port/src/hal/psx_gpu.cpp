#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {

typedef struct {
    int16_t x, y, w, h;
} RECT;

typedef struct {
    RECT clip;
    int16_t ofs[2];
    RECT tw;
    uint16_t tpage;
    uint8_t dtd;
    uint8_t dfe;
    uint8_t isbg;
    uint8_t r0, g0, b0;
    uint8_t p;
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    uint8_t isinter;
    uint8_t isrgb24;
    uint8_t pad[2];
} DISPENV;

DRAWENV* SetDefDrawEnv(DRAWENV* env, int16_t x, int16_t y, int16_t w, int16_t h) {
    std::memset(env, 0, sizeof(DRAWENV));
    env->clip.x = x;
    env->clip.y = y;
    env->clip.w = w;
    env->clip.h = h;
    env->ofs[0] = x;
    env->ofs[1] = y;
    return env;
}

DISPENV* SetDefDispEnv(DISPENV* env, int16_t x, int16_t y, int16_t w, int16_t h) {
    std::memset(env, 0, sizeof(DISPENV));
    env->disp.x = x;
    env->disp.y = y;
    env->disp.w = w;
    env->disp.h = h;
    return env;
}

DRAWENV* PutDrawEnv(DRAWENV* env) {
    return env;
}

DISPENV* PutDispEnv(DISPENV* env) {
    return env;
}

int32_t ClearImage(RECT* rect, uint8_t r, uint8_t g, uint8_t b) {
    return 0;
}

int32_t LoadImage(RECT* rect, uint32_t* p) {
    return 0;
}

int32_t MoveImage(RECT* rect, int16_t x, int16_t y) {
    return 0;
}

int32_t DrawSync(int32_t mode) {
    return 0;
}

int32_t VSync(int32_t mode) {
    return 0;
}

int32_t ResetGraph(int32_t mode) {
    return 0;
}

int32_t SetGraphDebug(int32_t level) {
    return 0;
}

} // extern "C"
