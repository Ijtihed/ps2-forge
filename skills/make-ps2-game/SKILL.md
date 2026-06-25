---
name: make-ps2-game
description: Scaffold, build, and run a PlayStation 2 game with the ps2-forge engine (2D and 3D). Use when the user wants to create a PS2 game/demo, port something to PS2, or add a new example to ps2-forge.
---

# make-ps2-game

ps2-forge is a tiny agentic-first PS2 engine (C). A game is one file: fill a
`Scene` (`init`/`update`/`render`) and call `app_run`. Read `AGENTS.md` in the
repo root for the complete API; this skill is the workflow.

## Steps

1. **Scaffold.** Copy the template:
   ```sh
   cp -r examples/template examples/<name> && cd examples/<name>
   ```
   Rename `EE_BIN` in the Makefile to `<name>.elf`. Edit `game.c`.

2. **Write the game.** One file. State in a `static struct`. Pattern:
   ```c
   #include "engine.h"
   typedef struct { /* ... */ } Game;
   static void init  (void *s, Ctx *c){ }
   static void update(void *s, Ctx *c){ /* ctx_is_held / ctx_just_pressed */ }
   static void render(void *s, Ctx *c){ /* e_rect / e_text / e_sprite / e3d_* */ }
   int main(void){ static Game g; Scene sc={.state=&g,.init=init,.update=update,.render=render};
       Config cfg=config_default(); app_run(cfg,&sc); }
   ```
   - 2D: `e_rect`, `e_text`, `e_quad`, sprites, `e_image_draw` (grids/framebuffers).
   - 3D: `e3d_begin(c,yaw,pitch)` → many `e3d_voxel(x,y,z,r,g,b)` → `e3d_end(c)`.
   - Coords: virtual 320x240 (2D), origin-centered ~-16..16 (3D).
   - Embed assets as C arrays (no filesystem at boot). Big arrays = `static`.

3. **Build** (needs ps2dev toolchain env set; see AGENTS.md "Build"):
   ```sh
   make            # -> <name>.elf
   ```

4. **Run / verify.**
   - Emulator: `Play --elf <name>.elf` (no BIOS). Bind keys in its controller config.
   - Headless screenshot to verify: run the ELF under Xvfb + software GL and grab a
     frame (see `tools/`); read the PNG to confirm it renders.
   - Real PS2: copy the ELF to USB / memory card, launch via FMCB / wLaunchELF.

## Tips (avoid known pitfalls)

- Render many cells/particles by writing colours into one RGBA buffer and
  `e_image_draw`-ing it, not thousands of `e_rect` calls.
- Text uses an alpha-tested font atlas (1 quad/glyph); just call `e_text`.
- Don't touch `gsGlobal` mode/size after init; the engine sets up the screen.
- Keep gameplay integer where possible; floats work (`-lm` linked) but are slower.
