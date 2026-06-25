/*
 * template game for ps2-forge. Move the box with the D-pad; it leaves a fading
 * trail and bounces a ball. Shows the core API: Scene callbacks, input, e_rect,
 * e_text. Copy this folder and edit to start a new game.
 */
#include "engine.h"

#define VW 320
#define VH 240

typedef struct {
    int x, y;            /* player box */
    int bx, by, bvx, bvy;/* bouncing ball */
} Game;

static void init(void *s, Ctx *c)
{
    (void)c;
    Game *g = (Game *)s;
    g->x = VW/2; g->y = VH/2;
    g->bx = 40; g->by = 40; g->bvx = 2; g->bvy = 3;
}

static void update(void *s, Ctx *c)
{
    Game *g = (Game *)s;
    if (ctx_is_held(c, BTN_LEFT))  g->x -= 3;
    if (ctx_is_held(c, BTN_RIGHT)) g->x += 3;
    if (ctx_is_held(c, BTN_UP))    g->y -= 3;
    if (ctx_is_held(c, BTN_DOWN))  g->y += 3;
    if (g->x < 0) g->x = 0; if (g->x > VW-12) g->x = VW-12;
    if (g->y < 0) g->y = 0; if (g->y > VH-12) g->y = VH-12;

    g->bx += g->bvx; g->by += g->bvy;
    if (g->bx < 0 || g->bx > VW-8)  g->bvx = -g->bvx;
    if (g->by < 0 || g->by > VH-8)  g->bvy = -g->bvy;
}

static void render(void *s, Ctx *c)
{
    Game *g = (Game *)s;
    e_text(c, 8, 8, 255, 230, 120, "PS2-FORGE TEMPLATE");
    e_rect(c, g->bx, g->by, 8, 8, 90, 200, 255);          /* ball */
    e_rect(c, g->x, g->y, 12, 12, 255, 90, 90);           /* player */
    e_text(c, 8, VH-16, 150, 150, 160, "DPAD TO MOVE");
}

int main(void)
{
    static Game g;
    Scene sc;
    sc.state = &g; sc.init = init; sc.update = update; sc.render = render;
    Config cfg = config_default();
    cfg.clear_r = 12; cfg.clear_g = 12; cfg.clear_b = 20;
    app_run(cfg, &sc);
    return 0;
}
