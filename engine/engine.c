/*
 * engine.c -- PS2 implementation of the PSoXide-style Scene/App engine.
 *
 * Subsystems wired here (the PS2 equivalents of PSoXide's SDK crates):
 *   - GS via gsKit            (was psx-gpu / ordering tables)
 *   - libpad via the IOP      (was psx-pad)
 *   - gsKit FONTM             (was psx-font)
 *   - the EE main loop        (was psx-rt + App::run)
 */
#include "engine.h"

#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <gsKit.h>
#include <gsToolkit.h>
#include <dmaKit.h>
#include <libpad.h>
#include <audsrv.h>

#define E_RGB(r, g, b) GS_SETREG_RGBAQ((r), (g), (b), 0x80, 0x00)
/* Neutral modulate colour so a textured sprite shows its texels as-is.
 * GS MODULATE uses 0x80 as 1.0; but this gsKit path renders textures at
 * ~half unless the modulate is full (0xFF). */
#define E_TEXMOD       GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0xFF, 0x00)

/* libpad needs a 256-byte aligned DMA buffer per pad. */
static char s_pad_buf[256] __attribute__((aligned(64)));

/* Single-pad, single-scene engine: one global Ctx is fine and keeps the
 * Scene callbacks clean (they receive a pointer to it). */
static Ctx s_ctx;

/* ---- Input -------------------------------------------------------------- */

int ctx_is_held(const Ctx *c, u32 b)      { return (c->btns & b) != 0; }
int ctx_just_pressed(const Ctx *c, u32 b) { return (c->btns & b) && !(c->btns_prev & b); }
int ctx_just_released(const Ctx *c, u32 b){ return !(c->btns & b) && (c->btns_prev & b); }

static void load_modules(void)
{
    int ret;
    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    if (ret < 0) { printf("[engine] load SIO2MAN failed: %d\n", ret); }
    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
    if (ret < 0) { printf("[engine] load PADMAN failed: %d\n", ret); }
}

static void wait_pad_ready(int port, int slot)
{
    int state = padGetState(port, slot);
    while (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        if (state == PAD_STATE_DISCONN) {
            /* No pad plugged in (e.g. emulator with no controller bound):
             * give up waiting so the demo still runs and renders. */
            return;
        }
        state = padGetState(port, slot);
    }
}

/* Poll port 0 / slot 0 into an active-high mask. Holds last frame's mask
 * if the pad is momentarily not ready, so motion stays smooth. */
static u32 poll_pad(u32 last)
{
    struct padButtonStatus b;
    int st = padGetState(0, 0);
    if (st == PAD_STATE_STABLE || st == PAD_STATE_FINDCTP1) {
        if (padRead(0, 0, &b) != 0) {
            return 0xffff ^ b.btns; /* libpad is active-low; invert */
        }
    }
    return last;
}

/* ---- Drawing ------------------------------------------------------------ */

void e_rect(Ctx *c, int x, int y, int w, int h, u8 r, u8 g, u8 b)
{
    float x1 = x * c->sx,        y1 = y * c->sy;
    float x2 = (x + w) * c->sx,  y2 = (y + h) * c->sy;
    gsKit_prim_sprite(c->gs, x1, y1, x2, y2, 0, E_RGB(r, g, b));
}

/* ---- Built-in 3x5 rect font --------------------------------------------
 * gsKit's FONTM needs alpha blending to read as text, and enabling global
 * alpha blends the whole frame to black under this GS path. So text is its
 * own tiny bitmap font, drawn as solid rects -- same "everything is rects"
 * spirit as PSoXide's primitives, and 8px advance to match its 8x16 font
 * layout math. Each glyph is 5 rows; the low 3 bits of each row are
 * columns (bit2=left .. bit0=right). Only the chars pong uses are defined;
 * lowercase folds to uppercase, unknown chars draw blank.
 */
#define GLYPH_DOT   2   /* virtual px per "on" cell */
#define GLYPH_ADV   8   /* virtual px advance per char (matches PSoXide) */

static const u8 *glyph_rows(char ch)
{
    static const u8 D0[5] = {7,5,5,5,7}, D1[5] = {2,6,2,2,7};
    static const u8 D2[5] = {7,1,7,4,7}, D3[5] = {7,1,7,1,7};
    static const u8 D4[5] = {5,5,7,1,1}, D5[5] = {7,4,7,1,7};
    static const u8 D6[5] = {7,4,7,5,7}, D7[5] = {7,1,2,2,2};
    static const u8 D8[5] = {7,5,7,5,7}, D9[5] = {7,5,7,1,7};
    static const u8 A[5]={7,5,7,5,5}, B[5]={6,5,6,5,6}, C[5]={3,4,4,4,3};
    static const u8 D[5]={6,5,5,5,6}, E[5]={7,4,6,4,7}, F[5]={7,4,6,4,4};
    static const u8 G[5]={7,4,5,5,7}, H[5]={5,5,7,5,5}, I[5]={7,2,2,2,7};
    static const u8 J[5]={1,1,1,5,7}, K[5]={5,6,4,6,5}, L[5]={4,4,4,4,7};
    static const u8 M[5]={5,7,5,5,5}, N[5]={5,7,7,7,5}, O[5]={7,5,5,5,7};
    static const u8 P[5]={7,5,7,4,4}, Q[5]={7,5,5,7,1}, R[5]={7,5,7,6,5};
    static const u8 S[5]={7,4,7,1,7}, T[5]={7,2,2,2,2}, U[5]={5,5,5,5,7};
    static const u8 V[5]={5,5,5,5,2}, W[5]={5,5,5,7,7}, X[5]={5,5,2,5,5};
    static const u8 Y[5]={5,5,2,2,2}, Z[5]={7,1,2,4,7};
    static const u8 BANG[5]={2,2,2,0,2}, QM[5]={7,1,2,0,2}, DOT[5]={0,0,0,0,2};
    static const u8 DASH[5]={0,0,7,0,0}, STAR[5]={5,2,7,2,5}, COL[5]={0,2,0,2,0};

    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    switch (ch) {
        case '0': return D0; case '1': return D1; case '2': return D2;
        case '3': return D3; case '4': return D4; case '5': return D5;
        case '6': return D6; case '7': return D7; case '8': return D8;
        case '9': return D9;
        case 'A': return A; case 'B': return B; case 'C': return C;
        case 'D': return D; case 'E': return E; case 'F': return F;
        case 'G': return G; case 'H': return H; case 'I': return I;
        case 'J': return J; case 'K': return K; case 'L': return L;
        case 'M': return M; case 'N': return N; case 'O': return O;
        case 'P': return P; case 'Q': return Q; case 'R': return R;
        case 'S': return S; case 'T': return T; case 'U': return U;
        case 'V': return V; case 'W': return W; case 'X': return X;
        case 'Y': return Y; case 'Z': return Z;
        case '!': return BANG; case '?': return QM;  case '.': return DOT;
        case '-': return DASH; case '*': return STAR; case ':': return COL;
        default:  return 0; /* space / unknown */
    }
}

/* Font atlas: 96 glyphs (ASCII 32..127) baked into 8x8 cells of a 128x48
 * texture, white where set (alpha 0x80) / transparent (alpha 0) elsewhere.
 * e_text draws one alpha-tested textured quad per char -- vastly fewer GS
 * primitives than the old per-pixel rects (that was the 4fps killer). */
#define ATL_W 128
#define ATL_H 48
static unsigned int s_atlas[ATL_W*ATL_H] __attribute__((aligned(64)));
static GSTEXTURE     s_atlas_tex;
static int           s_atlas_ready = 0;

static void build_atlas(void)
{
    for (int i = 0; i < ATL_W*ATL_H; i++) s_atlas[i] = 0;   /* transparent */
    for (int ch = 32; ch < 128; ch++) {
        const u8 *rows = glyph_rows((char)ch);
        if (!rows) continue;
        int cell = ch - 32, cx = (cell & 15) * 8, cy = (cell >> 4) * 8;
        for (int row = 0; row < 5; row++)
            for (int col = 0; col < 3; col++)
                if (rows[row] & (1 << (2 - col)))
                    s_atlas[(cy+row)*ATL_W + (cx+col)] = 0xFFFFFFFFu;  /* white, A=FF */
    }
    char *p = (char*)&s_atlas_tex;
    for (unsigned i = 0; i < sizeof(s_atlas_tex); i++) p[i] = 0;
    s_atlas_tex.Width = ATL_W; s_atlas_tex.Height = ATL_H; s_atlas_tex.PSM = GS_PSM_CT32;
    s_atlas_tex.Mem = s_atlas; s_atlas_tex.Filter = GS_FILTER_NEAREST;
    gsKit_setup_tbw(&s_atlas_tex);
    s_atlas_ready = 1;
}

void e_text(Ctx *c, int x, int y, u8 r, u8 g, u8 b, const char *s)
{
    if (!s_atlas_ready) build_atlas();
    gsKit_TexManager_bind(c->gs, &s_atlas_tex);
    gsKit_set_test(c->gs, GS_ATEST_ON);     /* discard transparent texels */
    u64 col = GS_SETREG_RGBAQ(r, g, b, 0x80, 0x00);
    for (; *s; s++, x += GLYPH_ADV) {
        char ch = *s; if (ch < 32 || (unsigned char)ch >= 128) continue;
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        int cell = ch - 32; float u0 = (cell & 15) * 8, v0 = (cell >> 4) * 8;
        float x1 = x*c->sx, y1 = y*c->sy, x2 = (x+6)*c->sx, y2 = (y+10)*c->sy;
        gsKit_prim_sprite_texture(c->gs, &s_atlas_tex, x1, y1, u0, v0,
                                  x2, y2, u0+3, v0+5, 0, col);
    }
    gsKit_set_test(c->gs, GS_ATEST_OFF);
}

/* ---- Sprites ------------------------------------------------------------ */

void e_sprite_init(Sprite *s, const void *rgba, int w, int h)
{
    GSTEXTURE *t = &s->tex;
    /* Zero the struct so unused fields (Clut, Vram, Delayed, ...) are sane. */
    char *p = (char *)t;
    for (unsigned i = 0; i < sizeof(*t); i++) p[i] = 0;

    t->Width  = w;
    t->Height = h;
    t->PSM    = GS_PSM_CT32;
    t->Mem    = (u32 *)rgba;
    t->Filter = GS_FILTER_NEAREST;
    gsKit_setup_tbw(t);
}

void e_sprite_draw(Ctx *c, Sprite *s, int x, int y, int w, int h)
{
    gsKit_TexManager_bind(c->gs, &s->tex);
    float x1 = x * c->sx,        y1 = y * c->sy;
    float x2 = (x + w) * c->sx,  y2 = (y + h) * c->sy;
    gsKit_prim_sprite_texture(c->gs, &s->tex, x1, y1, 0.0f, 0.0f,
                              x2, y2, (float)s->tex.Width, (float)s->tex.Height,
                              0, E_TEXMOD);
}

void e_quad(Ctx *c, int x0, int y0, int x1, int y1,
            int x2, int y2, int x3, int y3, u8 r, u8 g, u8 b)
{
    float sx = c->sx, sy = c->sy;
    gsKit_prim_quad(c->gs,
        x0*sx, y0*sy, x1*sx, y1*sy, x2*sx, y2*sy, x3*sx, y3*sy,
        0, E_RGB(r, g, b));
}

void e_sprite_draw_tinted(Ctx *c, Sprite *s, int x, int y, int w, int h,
                          u8 r, u8 g, u8 b)
{
    gsKit_TexManager_bind(c->gs, &s->tex);
    float x1 = x*c->sx, y1 = y*c->sy, x2 = (x+w)*c->sx, y2 = (y+h)*c->sy;
    gsKit_prim_sprite_texture(c->gs, &s->tex, x1, y1, 0, 0,
                              x2, y2, (float)s->tex.Width, (float)s->tex.Height,
                              0, GS_SETREG_RGBAQ(r, g, b, 0x80, 0x00));
}

void e_scissor(Ctx *c, int x, int y, int w, int h)
{
    int x0 = (int)(x*c->sx), y0 = (int)(y*c->sy);
    int x1 = (int)((x+w)*c->sx) - 1, y1 = (int)((y+h)*c->sy) - 1;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    gsKit_set_scissor(c->gs, GS_SETREG_SCISSOR(x0, x1, y0, y1));
}
void e_scissor_off(Ctx *c) { gsKit_set_scissor(c->gs, GS_SCISSOR_RESET); }

void e_image_draw(Ctx *c, Image *im, void *rgba, int iw, int ih,
                  int dx, int dy, int dw, int dh)
{
    if (!im->alloced) {
        char *p = (char *)&im->tex;
        for (unsigned i = 0; i < sizeof(im->tex); i++) p[i] = 0;
        im->tex.Width = iw; im->tex.Height = ih; im->tex.PSM = GS_PSM_CT32;
        im->tex.Mem = (u32 *)rgba; im->tex.Filter = GS_FILTER_NEAREST;
        gsKit_setup_tbw(&im->tex);
        im->tex.Vram = gsKit_vram_alloc(c->gs,
            gsKit_texture_size(iw, ih, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);
        im->alloced = 1;
    }
    im->tex.Mem = (u32 *)rgba;
    gsKit_texture_upload(c->gs, &im->tex);
    float x1 = dx*c->sx, y1 = dy*c->sy, x2 = (dx+dw)*c->sx, y2 = (dy+dh)*c->sy;
    gsKit_prim_sprite_texture(c->gs, &im->tex, x1, y1, 0, 0,
                              x2, y2, (float)iw, (float)ih,
                              0, GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0x00));
}

/* ---- Built-in 3D voxel renderer ---------------------------------------- */
#define E3_RW 160
#define E3_RH 120
#define E3_MAXV 6000
#define E3_NB 96
static unsigned int e3_buf[E3_RW*E3_RH] __attribute__((aligned(64)));
static Image  e3_img;
static float  e3_ca, e3_sa, e3_cb, e3_sb;
static int    e3_n;
static float  e3_sx[E3_MAXV], e3_sy[E3_MAXV], e3_sz[E3_MAXV];
static unsigned int e3_col[E3_MAXV];
static short  e3_head[E3_NB], e3_next[E3_MAXV];
#define E3_RGBA(r,g,b) ((unsigned)(r)|((unsigned)(g)<<8)|((unsigned)(b)<<16)|(0x80u<<24))

void e3d_begin(Ctx *c, float yaw, float pitch)
{
    (void)c;
    unsigned bg = E3_RGBA(6,6,12);
    for (int i = 0; i < E3_RW*E3_RH; i++) e3_buf[i] = bg;
    e3_ca = cosf(yaw); e3_sa = sinf(yaw);
    e3_cb = cosf(pitch); e3_sb = sinf(pitch);
    e3_n = 0;
    for (int i = 0; i < E3_NB; i++) e3_head[i] = -1;
}

void e3d_voxel(float x, float y, float z, u8 r, u8 g, u8 b)
{
    if (e3_n >= E3_MAXV) return;
    const float DIST = 40.f, FOV = 160.f;
    float x1 = x*e3_ca + z*e3_sa, z1 = -x*e3_sa + z*e3_ca;
    float y2 = y*e3_cb - z1*e3_sb, z2 = y*e3_sb + z1*e3_cb;
    float zc = z2 + DIST; if (zc < 1) zc = 1;
    float p = FOV / zc;
    float dim = 0.45f + 0.55f*(1.0f - zc/(DIST+30.f));
    int rr=(int)(r*dim), gg=(int)(g*dim), bb=(int)(b*dim);
    if(rr>255)rr=255; if(gg>255)gg=255; if(bb>255)bb=255; if(rr<0)rr=0; if(gg<0)gg=0; if(bb<0)bb=0;
    int n = e3_n++;
    e3_sx[n] = E3_RW/2 + x1*p; e3_sy[n] = E3_RH/2 + y2*p; e3_sz[n] = p;
    e3_col[n] = E3_RGBA(rr,gg,bb);
    int bk = (int)((z2/40.f + 0.5f)*(E3_NB-1)); if(bk<0)bk=0; if(bk>=E3_NB)bk=E3_NB-1;
    e3_next[n] = e3_head[bk]; e3_head[bk] = (short)n;
}

void e3d_end(Ctx *c)
{
    for (int bk = 0; bk < E3_NB; bk++)
        for (int i = e3_head[bk]; i >= 0; i = e3_next[i]) {
            int sz = (int)e3_sz[i]; if (sz < 2) sz = 2; if (sz > 6) sz = 6;
            int x = (int)(e3_sx[i] - sz*0.5f), y = (int)(e3_sy[i] - sz*0.5f);
            for (int py = y; py < y+sz; py++) { if (py<0||py>=E3_RH) continue;
                for (int px = x; px < x+sz; px++) { if (px<0||px>=E3_RW) continue;
                    e3_buf[py*E3_RW + px] = e3_col[i]; } }
        }
    e_image_draw(c, &e3_img, e3_buf, E3_RW, E3_RH, 0, 0, 320, 240);
}

/* ---- Audio (SFX) -------------------------------------------------------- */

static audsrv_adpcm_t s_sfx[8];
static int s_sfx_n = 0;
static int s_audio_ok = 0;

void e_audio_init(const void *irx, int irx_size)
{
    int ret = 0;
    SifLoadModule("rom0:LIBSD", 0, NULL);
    SifExecModuleBuffer((void *)irx, irx_size, 0, NULL, &ret);
    if (audsrv_init() != 0) {
        printf("[engine] audsrv init failed: %s\n", audsrv_get_error_string());
        return;
    }
    audsrv_adpcm_init();
    audsrv_set_volume(MAX_VOLUME);
    s_audio_ok = 1;
    printf("[engine] audio ready\n");
}

int e_sfx_load(const void *adpcm, int size)
{
    if (!s_audio_ok || s_sfx_n >= 8) return -1;
    int id = s_sfx_n++;
    audsrv_load_adpcm(&s_sfx[id], (void *)adpcm, size);
    return id;
}

void e_sfx_play(int handle)
{
    if (s_audio_ok && handle >= 0) audsrv_ch_play_adpcm(-1, &s_sfx[handle]);
}

int e_audio_ok(void) { return s_audio_ok; }

/* ---- App::run ----------------------------------------------------------- */

Config config_default(void)
{
    Config cfg;
    cfg.clear_r = 0; cfg.clear_g = 0; cfg.clear_b = 0;
    return cfg;
}

static GSGLOBAL *init_gs(void)
{
    /* Let gsKit auto-detect the video mode (NTSC/PAL) and framebuffer
     * geometry. Overriding Mode/Width/Height/Field by hand after
     * init_global desyncs the PCRTC display offsets and blanks the
     * screen, so we only set the pixel format and buffering and then
     * read back gs->Width/Height for our virtual->physical scale. */
    GSGLOBAL *gs = gsKit_init_global();

    gs->PSM             = GS_PSM_CT24;
    gs->ZBuffering      = GS_SETTING_OFF;
    gs->DoubleBuffering = GS_SETTING_ON;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    return gs;
}

void app_run(Config cfg, Scene *scene)
{
    printf("[engine] boot: PSoXide-style PS2 engine\n");

    sceSifInitRpc(0);
    load_modules();
    padInit(0);
    if (padPortOpen(0, 0, s_pad_buf) == 0) {
        printf("[engine] padPortOpen failed (continuing, no input)\n");
    }
    wait_pad_ready(0, 0);

    GSGLOBAL *gs = init_gs();
    printf("[engine] GS %dx%d\n", gs->Width, gs->Height);

    s_ctx.gs        = gs;
    s_ctx.btns      = 0;
    s_ctx.btns_prev = 0;
    s_ctx.frame     = 0;
    s_ctx.sx        = (float)gs->Width  / (float)VIRT_W;
    s_ctx.sy        = (float)gs->Height / (float)VIRT_H;

    u64 clear = E_RGB(cfg.clear_r, cfg.clear_g, cfg.clear_b);

    if (scene->init) scene->init(scene->state, &s_ctx);
    printf("[engine] scene init done, entering loop\n");

    for (;;) {
        s_ctx.btns = poll_pad(s_ctx.btns);

        scene->update(scene->state, &s_ctx);

        gsKit_clear(gs, clear);
        scene->render(scene->state, &s_ctx);

        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
        gsKit_TexManager_nextFrame(gs);

        s_ctx.btns_prev = s_ctx.btns;
        s_ctx.frame++;
    }
}
