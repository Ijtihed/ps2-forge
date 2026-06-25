---
name: port-js-to-ps2
description: Port a JavaScript/TypeScript game (HTML5 canvas or p5.js) to the PlayStation 2 with the ps2-forge engine. Use when the user has a JS/TS game (canvas/p5/requestAnimationFrame) and wants it running on PS2, or asks to "port this to PS2 / console".
---

# port-js-to-ps2

Turn a browser game into a PS2 ELF with ps2-forge. The port is mostly
mechanical: same loop shape, same draw calls, different names. Full mapping in
the repo's `PORTING.md`; the `cv_*` sugar (`engine/canvas.h`) makes calls line
up near 1:1.

## Steps

1. **Read the JS/TS game.** Identify: the loop (`requestAnimationFrame` / p5
   `draw`), the `init`/`setup`, the state, the draw calls, the input, the canvas
   size, and any assets (images/audio).

2. **Scaffold.** `cp -r examples/template <name> && cd <name>`; set `EE_BIN` in
   the Makefile. Add `#include "canvas.h"` after `#include "engine.h"`.

3. **Split the loop.** Browser loop -> `update` (state + input) and `render`
   (drawing). Put `cv_begin(c);` at the top of both. Game state -> a `static`
   struct passed as `Scene.state`.

4. **Translate calls** using the `PORTING.md` table:
   - `ctx.fillStyle=...; ctx.fillRect(...)` -> `cv_fill(r,g,b); cv_rect(...)`
   - `ctx.fillText` -> `cv_text` (UPPERCASE, limited charset)
   - `ctx.arc+fill` -> `cv_circle` (square-ish)
   - `keyIsDown/keydown` -> `cv_key(BTN_*)` / `cv_pressed(BTN_*)`
   - `putImageData`/pixel buffers -> `cv_image(&im, rgba, w,h, dx,dy,dw,dh)`
   - `drawImage` -> embed the PNG as a C array (`pack.py` -> `assets.h`) +
     `e_sprite_*`
   - `Math.random` -> an xorshift; `Date.now`/dt -> fixed step or `c->frame`

5. **Scale coordinates** from the canvas size to the virtual 320x240 space
   (multiply, or just re-author at 320x240). Colours: parse `#rrggbb` -> ints.

6. **Embed assets.** No filesystem at boot; bake images/audio into C arrays.

7. **Build + verify the agentic way:**
   ```sh
   make test     # build -> boot headless -> prints RENDER: PASS/FAIL
   ```
   If FAIL: check the screenshot (`shot.png`), confirm draws land inside
   320x240, state is `static`, and you didn't depend on floats/alpha/circles.

## What won't port 1:1 (approximate)

Real circles, gradients, alpha blending, arbitrary fonts, mouse, DOM/audio APIs,
and heavy 3D. Use flat fills, square/dot approximations, the built-in uppercase
font, the pad, and `e3d_*` (voxels) for 3D. Particle games port especially well
(dots). See `PORTING.md` for the worked bouncing-ball example.
