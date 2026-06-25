/*
 * life -- Conway's Game of Life in ~45 lines. Shows the cellular-automaton
 * pattern: simulate a grid, write per-cell colours into one RGBA buffer, and
 * blit it scaled with e_image_draw (one draw call, not thousands of rects).
 */
#include "engine.h"
#include <string.h>

#define GW 80
#define GH 60
static unsigned char a[GW*GH], b[GW*GH];
static unsigned int  rgba[GW*GH] __attribute__((aligned(64)));
static Image img;

static unsigned rng = 12345;
static unsigned xr(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }

typedef struct { int _; } Game;

static void seed(void){ for (int i=0;i<GW*GH;i++) a[i] = (xr()&3)==0; }
static void init(void *s, Ctx *c){ (void)s;(void)c; seed(); }

static void update(void *s, Ctx *c){
    (void)s;
    if (ctx_just_pressed(c, BTN_CROSS)) seed();
    for (int y=0;y<GH;y++) for (int x=0;x<GW;x++){
        int n=0;
        for (int dy=-1;dy<=1;dy++) for (int dx=-1;dx<=1;dx++){ if(!dx&&!dy) continue;
            n += a[((y+dy+GH)%GH)*GW + ((x+dx+GW)%GW)]; }
        int i=y*GW+x; b[i] = a[i] ? (n==2||n==3) : (n==3);
    }
    memcpy(a, b, GW*GH);
}

static void render(void *s, Ctx *c){
    (void)s;
    for (int i=0;i<GW*GH;i++) rgba[i] = a[i] ? 0xFFE0E0E0u : 0xFF101418u;
    e_image_draw(c, &img, rgba, GW, GH, 0,0, 320,240);
    e_text(c, 6, 6, 255,255,255, "GAME OF LIFE");
    e_text(c, 6, 224, 150,150,160, "X RESEED");
}

int main(void){
    static Game g;
    Scene sc = { .state=&g, .init=init, .update=update, .render=render };
    app_run(config_default(), &sc);
    return 0;
}
