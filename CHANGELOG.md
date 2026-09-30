## 1.0.0

- **nd-stone is now an installable library rather than a build artifact of
  the engine.** It builds and installs exactly one file, `lib/libnd-stone.so`,
  following the same layout as `axil-tty` and `axil-auth`, and the same layout
  `nd-core` was converted to first. Previously `make` produced a `stone.so` named
  by the engine's `mods.load` and installed nothing. It installs no header
  because it exports no API: every symbol it defines is an `on_*` hook the
  engine declares itself in `nd/hooks.h`, so there is nothing for a consumer to
  include. There is no `lib/nd-stone.so` symlink:
  `mods.load` names this module `libnd-stone`, the installed filename, and
  `module_load_path()` only appends `.so`, so the load name must equal the
  installed name. A symlink would in any case have been dropped from the
  OpenBSD package, whose packing list is built from `find usr -type f`.

- **The link line is libxylem alone.** `LDLIBS := -lxylem`; the engine is not
  linked. A module is `dlopen`'d by an engine that already has XY resident, and
  XY_IMPL/XY_DECL resolve through the injected xy context, not through
  link-time symbols. `NEEDED` is `libxylem.so` and `libc.so.6`.

- **The game's service API is included as `<nd/xy.h>`**, from the engine's
  `$(PREFIX)/include/nd/` — the same include root as `<ttypt/xy.h>`, so this
  library needs no private `-I` for the game headers at all. It previously
  included `"papi/nd-xy.h"`, which no longer exists in any checkout: the engine
  moved the module-facing tree from `papi/` to `nd/` and `papi/` now holds only
  `nd.h`. That include could not compile, so this release also fixes the build.

- **Dropped the `nd-mod.mk` dependency.** This module resolves the game's
  headers itself, the way every other house library does, and `nd-mod.mk` is
  the SIC-era engine module build contract. It has now been deleted: this was
  the last kind of module that still included it.

- **`mods.load` names the installed filename.** A line with no in-tree module
  is passed to `xy_load()` verbatim so the dynamic linker resolves it. As
  before, the name carries no `.so` suffix — `xy_load()` appends it itself.
