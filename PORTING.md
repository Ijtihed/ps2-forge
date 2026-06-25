# Porting a JS/TS game to ps2-forge

Most agent-made games are HTML5-canvas or p5.js. Porting one to the PS2 with
ps2-forge is mostly mechanical: same loop shape, same draw calls, different
names. Include [`engine/canvas.h`](engine/canvas.h) and the calls line up.

## The one structural change

A browser game is a `requestAnimationFrame` loop (or p5 `draw()`); ps2-forge
splits it into `update` (logic + input) and `render` (drawing). Call
`cv_begin(c)` at the top of each so the `cv_*` calls have the frame context.

```
JS                                ps2-forge
----------------------------------------------------------------
setup() / init                    Scene.init
function loop(){ ... raf(loop) }  Scene.update (logic) + Scene.render (draw)
canvas 800x600                    virtual 320x240  (scale your coords down)
let state = {...}                 a static struct (game state)
```

## Mapping table

| JS / canvas / p5            | ps2-forge (`canvas.h`)                  |
|-----------------------------|-----------------------------------------|
| `ctx.fillStyle = '#rrggbb'` | `cv_fill(r, g, b)`                      |
| `ctx.fillRect(x,y,w,h)`     | `cv_rect(x,y,w,h)`                      |
| `background(r,g,b)` / clear | `cv_fill(r,g,b); cv_clear();` or `Config.clear_*` |
| `ctx.fillText(s,x,y)`       | `cv_text(x,y,s)`  (UPPERCASE, A-Z 0-9 . - ! ? : *) |
| `ctx.arc(x,y,r)+fill()`     | `cv_circle(x,y,r)` (square-ish; PS2 has no disc) |
| `keyIsDown(LEFT_ARROW)`     | `cv_key(BTN_LEFT)`                      |
| `keydown` edge / `keyPressed`| `cv_pressed(BTN_CROSS)`                |
| `Math.random()`             | a small `xorshift` (see below)          |
| `ctx.putImageData(img)`     | `cv_image(&im, rgba, w,h, dx,dy,dw,dh)` (RGBA grid blit) |
| `ctx.drawImage(sprite,...)` | `e_sprite_init` + `e_sprite_draw` (embed the PNG as a C array) |
| audio beep / sfx            | `e_audio_init` + `e_sfx_load/play` (embed ADPCM) |
| 3D (three.js points)        | `e3d_begin / e3d_voxel / e3d_end`       |

Keys: arrows -> `BTN_UP/DOWN/LEFT/RIGHT`; common action keys -> `BTN_CROSS`
(confirm/jump), `BTN_CIRCLE`, `BTN_SQUARE`, `BTN_TRIANGLE`, `BTN_START`.

## Gotchas (browser -> console)

- **Scale coordinates.** Canvas is often 800x600; the PS2 virtual space is
  320x240. Multiply positions/sizes by ~0.4, or just author in 320x240.
- **No DOM, no files.** Embed images/audio as C arrays (a `pack.py` -> `assets.h`);
  the ELF boots with no filesystem.
- **No `malloc` needed.** Put state and big arrays in `static` (the EE has 32MB).
- **Colors are 0-255 ints**, not CSS strings. Parse `#rrggbb` to three ints.
- **Text is uppercase, limited charset.** Uppercase your strings.
- **No real circles/gradients/alpha blending.** Approximate (square dots,
  flat fills). Many particle games look great as 1px/2px dots anyway.
- **dt:** the loop runs at the console vsync (~60Hz NTSC). Use fixed steps or
  `c->frame`; don't depend on wall-clock `Date.now()`.
- **Random:** `static unsigned rng=12345; unsigned xr(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}`

## Worked example: a bouncing-ball canvas game

JS:
```js
let x=40,y=40,vx=2,vy=3;
function loop(){
  ctx.fillStyle='#0c0c14'; ctx.fillRect(0,0,320,240);
  x+=vx; y+=vy; if(x<0||x>312)vx=-vx; if(y<0||y>232)vy=-vy;
  ctx.fillStyle='#5ac8ff'; ctx.fillRect(x,y,8,8);
  requestAnimationFrame(loop);
}
```

ps2-forge:
```c
#include "engine.h"
#include "canvas.h"
typedef struct { int x,y,vx,vy; } Game;
static void init(void *s, Ctx *c){ (void)c; Game *g=s; g->x=40;g->y=40;g->vx=2;g->vy=3; }
static void update(void *s, Ctx *c){ Game *g=s; cv_begin(c);
    g->x+=g->vx; g->y+=g->vy;
    if(g->x<0||g->x>312)g->vx=-g->vx; if(g->y<0||g->y>232)g->vy=-g->vy; }
static void render(void *s, Ctx *c){ Game *g=s; cv_begin(c);
    cv_fill(12,12,20); cv_clear();
    cv_fill(90,200,255); cv_rect(g->x,g->y,8,8); }
int main(void){ static Game g; Scene sc={.state=&g,.init=init,.update=update,.render=render};
    app_run(config_default(), &sc); }
```

Build + verify: `make && make test` (prints `RENDER: PASS/FAIL`). See
[`AGENTS.md`](AGENTS.md) for the full API.
