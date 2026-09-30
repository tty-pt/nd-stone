# axil-nd-stone

`nd-stone` for [axil-nd](../axil-nd), ported from SIC to libxylem.

Spawns mineral objects and handles objects being added to the world. It is one of
the four Wave 1 leaves in MODS.md §7: no module dependencies, and it exercises
two events rather than none.

## Install

```sh
make install
```

Installs one file:

```
lib/libnd-stone.so
```

There is deliberately no `lib/nd-stone.so` symlink. `xy_load()` appends `.so`
itself and does not retry with a `lib` prefix, so the engine's `mods.load` names
this module `libnd-stone` and `dlopen`s `libnd-stone.so`. A soname symlink would
also have been silently dropped from the OpenBSD package: `tty-pt/ci` builds the
packing list from `find usr -type f`, which never lists a symlink, so the package
would have shipped the library under one name and asked the loader for another.

Also packaged for deb, apk, rpm, brew and openbsd from a `v*` tag.

It installs no header because it exports no API — `on_spawn` and `on_add` are
event hooks the engine declares itself in `nd/hooks.h`, not something another
module calls.

## Build from source

```sh
make
```

Needs [libxylem](https://github.com/tty-pt/libxylem) (headers already in
`$(PREFIX)/include`), the engine's game API `<nd/xy.h>` from either an `axil-nd`
checkout beside this repo or an installed `axil-nd`, and `<xxhash.h>`:

```sh
git clone https://github.com/tty-pt/nd-stone && cd nd-stone
git clone https://github.com/tty-pt/axil-nd ../axil-nd
make
```

`<nd/xy.h>` installs to `$(PREFIX)/include/nd/`, the same directory that already
carries `<ttypt/xy.h>`, so an **installed** engine needs no `-I` of its own here.
Against a checkout beside this repo it is `-I../axil-nd/include`; both paths are
on `CFLAGS` at once and a missing `-I` is ignored, so the same command works
either way.

`<xxhash.h>` is an ordinary system header and is the one build dep this module
has beyond the house pair. `XXH32` is self-contained in that header, so **no
`-lxxhash` is needed** — only the header.

## What it does

* `xy_install()` registers the object type `mineral` and its skeleton, logging
  both the type id and the skeleton id.
* `on_spawn(player_ref, loc_ref, ...)` places one mineral object in the room the
  player is entering.
* `on_add(ref, type, v)` is where the art variant is chosen:
  `obj.art_id = XXH32(&v, sizeof(v), 2) % skel.max_art`, so the same `v` always
  yields the same variant and the world is reproducible.

## Testing

There is no `test.sh` here. Behaviour is asserted by the engine's own suite,
which builds every module in its `mods.load`, boots, and greps stderr:

```sh
cd ../axil-nd
make && ./test.sh
```

nd-stone is not yet in the engine's `mods.load` (MODS.md §9 records it as ported
but neither loaded nor asserted), so no suite assertion covers it yet. Its
`xy_install` logs `nd-stone: xy_install, type "mineral" = %u, skel %u`, which is
the line such an assertion would grep for.

## Notes from the port

* **The include changed spelling.** It was `"papi/nd-xy.h"`, a file that no
  longer exists in any checkout: the engine moved its module-facing tree from
  `papi/` to `nd/`, and `papi/` now holds only `nd.h`. The module could not
  compile until this was fixed.
* **`XXH32` moved out of the engine.** The original got it from `<nd/nd.h>`,
  which is gone, and nothing in the module-facing tree exposes it. It now
  includes `<xxhash.h>` directly. The call and its `seed 2` are unchanged, so the
  variants a given world produces are identical.
* A `(void)` cast is used rather than an attribute for the unused spawn
  parameter, because `XY_IMPL` turns its parameter list into an argument-struct
  literal and the suppression has to attach to that use.
* This TU `XY_IMPL`s `on_spawn` and `on_add`, so it must **not** include
  `nd/hooks.h`: an `XY_IMPL` and an `XY_DECL` of the same name in one TU is the
  XY equivalent of the old `SIC_DEF`/`SIC_DECL` collision.

## License

BSD 2-Clause, carried over from `tty-pt/nd-stone`. See `LICENSE`.
