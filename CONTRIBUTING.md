# Contributing to ps2-forge

Thanks for wanting to hack on a PlayStation 2 game engine. This is a small,
readable codebase on purpose. Keep it that way.

## Build setup

Linux x86_64 is the supported build host (macOS/Windows: grab the matching asset
from the [ps2dev releases](https://github.com/ps2dev/ps2dev/releases) and set the
same env vars).

```sh
tools/bootstrap.sh                 # downloads the ps2dev toolchain (no sudo)
# then export what it prints:
export PS2DEV=~/ps2dev PS2SDK=$PS2DEV/ps2sdk GSKIT=$PS2DEV/gsKit
export PATH=$PS2DEV/ee/bin:$PS2DEV/bin:$PS2SDK/bin:$PATH
```

Build any example with `make`, or use the `forge` CLI:

```sh
./forge doctor          # check the toolchain is ready
./forge play spin3d     # build + run a bundled example
./forge new mygame      # scaffold a new game
```

## How a game is structured

A game is **one C file** that fills a `Scene` (`init` / `update` / `render`) and
calls `app_run`. The engine owns the loop, the GS, the pad, and timing. The
complete API and conventions are on one page: [`AGENTS.md`](AGENTS.md). Read it
before adding features.

Conventions:
- All gameplay math in the virtual **320x240** space; keep it integer where you can.
- `static` your game state and any large arrays (they live in `.bss`; the EE has 32MB).
- No `malloc` for small games. Embed assets as C arrays (no filesystem at boot).
- One file per game. New examples go in `examples/<name>/` with a `game.c` and a
  Makefile (copy `examples/template/` and set `EE_BIN`).

## The verdict loop (run this before you submit)

```sh
make test        # build -> boot headless -> prints RENDER: PASS|FAIL + exit code
```
A change should leave every example building and `make test` printing
`RENDER: PASS`. The headless test needs `Play!` (or set `$PLAY` to your
emulator), `Xvfb`, software GL, and a Python with `mss` + `Pillow`.

## CI

Every push and PR runs [`.github/workflows/build.yml`](.github/workflows/build.yml),
which compiles every example in the official `ps2dev/ps2dev` image. Keep it green:
if you change the engine, make sure all of `examples/*` still build.

## Code style

- Match the surrounding C: 4-space indent, terse, no churn.
- The public API is **declared in `engine/engine.h`** and **defined in
  `engine/engine.c`**. If you add/remove a public function, update both, plus the
  function-count and any affected docs.
- **No em dashes** anywhere (docs or comments). Use commas, colons, parentheses,
  or ASCII hyphens. This is enforced.

## Pull requests

- Keep PRs focused. One feature or fix per PR.
- If you touch the API, keep [`AGENTS.md`](AGENTS.md) and [`README.md`](README.md)
  in sync (they are the contract people and agents read).
- Don't commit build artifacts (`*.elf`, `*.o`, `shot.png`) - they're gitignored;
  keep it that way.
- Describe what you changed and how you verified it (which examples built, the
  `make test` verdict).

## License

By contributing you agree your contributions are licensed under the repository's
MIT license (engine code). PS2SDK and gsKit keep their own licenses.
