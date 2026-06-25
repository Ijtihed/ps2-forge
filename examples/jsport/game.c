/*
 * jsport -- a bouncing-ball HTML5-canvas game ported to ps2-forge using the
 * canvas.h sugar layer. Compare to the JS in PORTING.md: it's near line-for-line.
 */
#include "engine.h"
#include "canvas.h"

typedef struct { int x, y, vx, vy; } Game;

static void init(void *s, Ctx *c){ (void)c; Game *g=s; g->x=40; g->y=40; g->vx=2; g->vy=3; }

static void update(void *s, Ctx *c){
    Game *g=s; cv_begin(c);
    /* arrow keys nudge the ball, like a keydown handler would */
    if (cv_key(BTN_LEFT))  g->vx--; if (cv_key(BTN_RIGHT)) g->vx++;
    g->x += g->vx; g->y += g->vy;
    if (g->x<0||g->x>312) g->vx=-g->vx;
    if (g->y<0||g->y>232) g->vy=-g->vy;
}

static void render(void *s, Ctx *c){
    Game *g=s; cv_begin(c);
    cv_fill(12,12,20);   cv_clear();              /* background */
    cv_fill(90,200,255); cv_rect(g->x,g->y,8,8);  /* the ball */
    cv_fill(255,230,120); cv_text(8,8,"JS PORT");
}

int main(void){
    static Game g;
    Scene sc = { .state=&g, .init=init, .update=update, .render=render };
    app_run(config_default(), &sc);
    return 0;
}
