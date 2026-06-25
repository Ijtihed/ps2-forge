/*
 * engine.h -- a tiny PS2 runtime engine that mirrors PSoXide's
 * `psx-engine` Scene/App framework, re-expressed in C + PS2SDK/gsKit.
 *
 * PSoXide (PS1) shape we are copying:
 *   - App::run(Config, &mut Scene) drives the cadence:
 *       poll-pad -> update -> clear -> render -> draw-sync -> vsync -> swap
 *   - trait Scene { init(&mut Ctx); update(&mut Ctx); render(&mut Ctx); }
 *   - Ctx::is_held / just_pressed / just_released for edge-detected input
 *   - 2D draw of solid rects + text, ordered back-to-front
 *
 * PS1 -> PS2 reality: none of PSoXide's GPU/OT/GTE code transfers (different
 * silicon). The *interface* transfers. Under the hood this is gsKit on the
 * GS, libpad on the IOP, and the EE main loop -- "how PS2 does things".
 *
 * The game is authored in a virtual 320x240 space (same as the PSoXide pong)
 * and scaled to the GS framebuffer, so the ported game logic is unchanged.
 */
#ifndef PS2ENGINE_H
#define PS2ENGINE_H

#include <tamtypes.h>
#include <gsKit.h>

/* ----------------------------------------------------------------------
 * Buttons. PSoXide's psx-pad and PS2's libpad both follow the Sony
 * digital-pad bit layout, so these constants line up 1:1. Active-high
 * (1 = pressed) like PSoXide's `is_held`.
 * -------------------------------------------------------------------- */
#define BTN_SELECT   0x0001
#define BTN_L3       0x0002
#define BTN_R3       0x0004
#define BTN_START    0x0008
#define BTN_UP       0x0010
#define BTN_RIGHT    0x0020
#define BTN_DOWN     0x0040
#define BTN_LEFT     0x0080
#define BTN_L2       0x0100
#define BTN_R2       0x0200
#define BTN_L1       0x0400
#define BTN_R1       0x0800
#define BTN_TRIANGLE 0x1000
#define BTN_CIRCLE   0x2000
#define BTN_CROSS    0x4000
#define BTN_SQUARE   0x8000

/* Virtual authoring resolution -- matches the PSoXide pong. */
#define VIRT_W 320
#define VIRT_H 240

/* ----------------------------------------------------------------------
 * Ctx -- per-frame context handed to every Scene callback, mirroring
 * PSoXide's `Ctx`. Holds the GS handle, font, and edge-detected pad.
 * -------------------------------------------------------------------- */
typedef struct {
    GSGLOBAL *gs;
    u32 btns;       /* active-high button mask, this frame  */
    u32 btns_prev;  /* last frame, for edge detection        */
    u32 frame;      /* frames since boot                     */
    float sx, sy;   /* virtual -> physical scale factors     */
} Ctx;

int ctx_is_held(const Ctx *c, u32 b);
int ctx_just_pressed(const Ctx *c, u32 b);
int ctx_just_released(const Ctx *c, u32 b);

/* 2D draw in VIRTUAL 320x240 space (PSoXide-compatible coordinates).
 * Rects are submitted in call order (no Z buffer) so draw back-to-front,
 * exactly like populating PSoXide's ordering-table slots. */
void e_rect(Ctx *c, int x, int y, int w, int h, u8 r, u8 g, u8 b);
void e_text(Ctx *c, int x, int y, u8 r, u8 g, u8 b, const char *s);

/* ----------------------------------------------------------------------
 * Textured sprites. Backed by a GS texture uploaded from embedded RGBA
 * (PSM_CT32). `rgba` stays owned by the caller (point it at a static
 * array). Draw in virtual coords; the texture is scaled to w x h.
 * -------------------------------------------------------------------- */
typedef struct { GSTEXTURE tex; } Sprite;

void e_sprite_init(Sprite *s, const void *rgba, int w, int h);
void e_sprite_draw(Ctx *c, Sprite *s, int x, int y, int w, int h);

/* Filled quad from 4 virtual-space points (for rotated beams). Points in
 * order p0,p1 (near edge) then p2,p3 (far edge). */
void e_quad(Ctx *c, int x0, int y0, int x1, int y1,
            int x2, int y2, int x3, int y3, u8 r, u8 g, u8 b);

/* Textured sprite modulated by a tint colour (white master sprites become the
 * tint; pre-coloured sprites use 255,255,255 to draw as-is). */
void e_sprite_draw_tinted(Ctx *c, Sprite *s, int x, int y, int w, int h,
                          u8 r, u8 g, u8 b);

/* Hardware scissor (clip) rectangle, virtual coords. Reset with e_scissor_off. */
void e_scissor(Ctx *c, int x, int y, int w, int h);
void e_scissor_off(Ctx *c);

/* Dynamic image blit: a CT32 RGBA buffer (iw*ih) re-uploaded each frame and
 * drawn scaled to (dx,dy,dw,dh) in virtual coords. For grid/framebuffer-style
 * rendering (cellular automata, etc). */
typedef struct { GSTEXTURE tex; int alloced; } Image;
void e_image_draw(Ctx *c, Image *im, void *rgba, int iw, int ih,
                  int dx, int dy, int dw, int dh);

/* ----------------------------------------------------------------------
 * Built-in 3D: a software voxel/point renderer. Point coords are centered
 * on the origin (use roughly -16..+16). Three calls per frame:
 *   e3d_begin(c, yaw, pitch);                     // clear + set rotation
 *   for each voxel: e3d_voxel(x,y,z, r,g,b);       // queued, depth-shaded
 *   e3d_end(c);                                    // depth-sort + blit fullscreen
 * Draw 2D HUD (e_text/e_rect) AFTER e3d_end to overlay it. Fast: one blit.
 * -------------------------------------------------------------------- */
void e3d_begin(Ctx *c, float yaw, float pitch);
void e3d_voxel(float x, float y, float z, u8 r, u8 g, u8 b);
void e3d_end(Ctx *c);

/* ----------------------------------------------------------------------
 * Audio (SFX). Fire-and-forget ADPCM samples via audsrv, mirroring
 * PSoXide's `sfx::play`. e_audio_init takes the embedded audsrv.irx
 * (booting from --elf has no filesystem to SifLoadModule from).
 * -------------------------------------------------------------------- */
void e_audio_init(const void *audsrv_irx, int irx_size);
int  e_sfx_load(const void *adpcm, int size);  /* returns a handle */
void e_sfx_play(int handle);
int  e_audio_ok(void);   /* 1 if audsrv initialised, else 0 */

/* ----------------------------------------------------------------------
 * Config + Scene + App -- the PSoXide framework surface.
 * -------------------------------------------------------------------- */
typedef struct {
    u8 clear_r, clear_g, clear_b;  /* background, like Config::clear_color */
} Config;

Config config_default(void);

typedef struct Scene {
    void *state;
    void (*init)(void *state, Ctx *c);     /* optional (may be NULL) */
    void (*update)(void *state, Ctx *c);   /* required */
    void (*render)(void *state, Ctx *c);   /* required */
} Scene;

/* App::run -- sets up GS + pad, then drives the fixed loop forever. */
void app_run(Config cfg, Scene *scene);

#endif /* PS2ENGINE_H */
