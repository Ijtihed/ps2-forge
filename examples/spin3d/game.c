/*
 * spin3d -- minimal ps2-forge 3D demo. A rotating cube of voxels in ~20 lines,
 * showing the whole 3D API: e3d_begin -> e3d_voxel (many) -> e3d_end.
 */
#include "engine.h"

typedef struct { float yaw; } Game;

static void init(void *s, Ctx *c){ (void)c; ((Game*)s)->yaw = 0; }

static void update(void *s, Ctx *c){
    Game *g = s; g->yaw += 0.02f;
    if (ctx_is_held(c, BTN_LEFT))  g->yaw -= 0.04f;
    if (ctx_is_held(c, BTN_RIGHT)) g->yaw += 0.04f;
}

static void render(void *s, Ctx *c){
    Game *g = s;
    e3d_begin(c, g->yaw, 0.4f);
    for (int z=-7; z<=7; z++) for (int y=-7; y<=7; y++) for (int x=-7; x<=7; x++)
        if (x==-7||x==7||y==-7||y==7||z==-7||z==7)        /* cube shell */
            e3d_voxel(x, y, z, 128+x*16, 128+y*16, 200-z*8);
    e3d_end(c);
    e_text(c, 6, 6, 255,255,255, "PS2-FORGE 3D");
    e_text(c, 6, 224, 150,150,160, "L/R SPIN");
}

int main(void){
    static Game g;
    Scene sc = { .state=&g, .init=init, .update=update, .render=render };
    Config cfg = config_default();
    app_run(cfg, &sc);
    return 0;
}
