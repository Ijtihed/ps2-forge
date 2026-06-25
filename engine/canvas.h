/*
 * canvas.h -- a tiny HTML5-canvas / p5.js-style sugar layer over the ps2-forge
 * engine, so a JS/TS game ports almost line-for-line. Header-only.
 *
 *   JS                           ps2-forge (canvas.h)
 *   ctx.fillStyle='#ff0000'      cv_fill(255,0,0)
 *   ctx.fillRect(x,y,w,h)        cv_rect(x,y,w,h)
 *   ctx.fillText("HI",x,y)       cv_text(x,y,"HI")
 *   ctx.arc(x,y,r,..)+fill()     cv_circle(x,y,r)   (square-ish approx)
 *   key handling (keydown)       cv_key(BTN_LEFT) / cv_pressed(BTN_CROSS)
 *   requestAnimationFrame loop   Scene update/render (call cv_begin(c) first)
 *
 * Call cv_begin(c) at the top of BOTH update() and render() so the cv_* calls
 * know the current frame context. Coordinates are the virtual 320x240 space.
 */
#ifndef CANVAS_H
#define CANVAS_H
#include "engine.h"

static Ctx *cv_ctx;
static u8 cv_r = 255, cv_g = 255, cv_b = 255;

static inline void cv_begin(Ctx *c){ cv_ctx = c; }
static inline void cv_fill(u8 r, u8 g, u8 b){ cv_r=r; cv_g=g; cv_b=b; }
static inline void cv_rect(int x, int y, int w, int h){ e_rect(cv_ctx,x,y,w,h,cv_r,cv_g,cv_b); }
static inline void cv_text(int x, int y, const char *s){ e_text(cv_ctx,x,y,cv_r,cv_g,cv_b,s); }
/* filled "circle" -- approximated as a centered square (PS2 has no disc prim) */
static inline void cv_circle(int cx, int cy, int rad){ e_rect(cv_ctx,cx-rad,cy-rad,rad*2,rad*2,cv_r,cv_g,cv_b); }
/* clear the whole screen to the current fill colour */
static inline void cv_clear(void){ e_rect(cv_ctx,0,0,320,240,cv_r,cv_g,cv_b); }

static inline int cv_key(u16 btn){ return ctx_is_held(cv_ctx, btn); }
static inline int cv_pressed(u16 btn){ return ctx_just_pressed(cv_ctx, btn); }

/* blit a w*h RGBA buffer scaled to a rect (canvas putImageData / drawImage) */
static inline void cv_image(Image *im, void *rgba, int iw, int ih,
                            int dx, int dy, int dw, int dh){
    e_image_draw(cv_ctx, im, rgba, iw, ih, dx, dy, dw, dh);
}
#endif
