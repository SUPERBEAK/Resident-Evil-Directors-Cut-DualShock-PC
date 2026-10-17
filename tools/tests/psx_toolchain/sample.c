#define FIX(x) ((x) << 12)
typedef struct { short vx, vy, vz, pad; } SVECTOR;
typedef struct { int hp; short pos[3]; unsigned char state, sub; SVECTOR rot; void (*fn)(void *); } Enemy;
extern Enemy g_enemies[16];
extern int rsin(int a), rcos(int a);
static int table[8] = { 1, 2, 3, 5, 8, 13, 21, 34 };
int enemy_update(Enemy *e, int dt) {
    int i, sum = 0;
    switch (e->state) {
    case 0: e->hp -= dt; break;
    case 1: e->pos[0] += (rsin(e->rot.vy) * dt) >> 12; e->pos[2] += (rcos(e->rot.vy) * dt) >> 12; break;
    case 2: case 3: e->sub++; if (e->sub > 30) e->state = 4; break;
    case 4: for (i = 0; i < 8; i++) sum += table[i] * e->pos[i & 1]; e->hp = sum / 7; break;
    case 5: e->fn(e); break;
    default: return -1;
    }
    return e->hp > 0 ? FIX(e->hp) / 3 : 0;
}
void all(int dt) { Enemy *e; for (e = g_enemies; e < &g_enemies[16]; e++) if (e->fn) enemy_update(e, dt); }
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
long long mul64(long long a, int b) { return a * b; }
