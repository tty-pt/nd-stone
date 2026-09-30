/* main.c — nd-stone, ported to libxylem.
 *
 * Scatters mineral stones into a tile when it spawns, and picks each stone's
 * art variant from its own skeleton once the engine has built it.
 *
 * Wave 1 (MODS.md §7): no module dependencies. What it exercises beyond the
 * Phase 1 slice is a *struct-by-value* event argument (`struct bio` in
 * on_spawn) and two engine calls the other slice modules never made —
 * `map_where` and `object_add`.
 *
 * This TU XY_IMPLs on_spawn and on_add, so it must NOT include
 * nd/hooks.h (an XY_IMPL and an XY_DECL of the same name in one TU is
 * the XY form of the old SIC_DEF/SIC_DECL collision). The canonical
 * signatures live in that header.
 */

#include <ttypt/xy-mod.h>

/* XXH32 for the art-variant hash. The old module got this from <nd/nd.h>,
 * which is gone; nothing in nd/ exposes it. <xxhash.h> is self-contained
 * for XXH32 (no -lxxhash needed), and the call is unchanged, so the variants
 * this picks are the same ones the old engine picked. */
#include <xxhash.h>

#include <nd/xy.h>

unsigned stone_skel_id, type_mineral;

static inline void
stones_add(unsigned where_ref, struct bio *bio, uint64_t v)
{
	unsigned char n = v & 0x3;

	if (bio->bio_idx == BIOME_WATER)
		return;

	if (!(n && (v & 0x18) && (v & 0x20)))
		return;

	pos_t pos;
	map_where(pos, where_ref);
	uint32_t v2 = XXH32((const char *) &v, sizeof(v), 2);

	for (int i = 0; i < n; i++, v2 >>= 4) {
		OBJ stone;
		unsigned stone_ref = object_add(&stone, stone_skel_id,
			where_ref, v2);
		nd_put(HD_OBJ, &stone_ref, &stone);
	}
}

/* The old signature was (player_ref, where_ref, bio, v); loc_ref is
 * where_ref under the engine's canonical name (nd-hooks.h:54). player_ref is
 * genuinely unused -- spawning a tile is not player-scoped -- and is silenced
 * with a void cast rather than an attribute, because XY_IMPL turns its
 * argument list into a typedef, where the attribute would not apply. */
XY_IMPL(int, on_spawn, unsigned, player_ref, unsigned, loc_ref,
	struct bio, bio, uint64_t, v)
{
	(void)player_ref;
	stones_add(loc_ref, &bio, v);
	return 0;
}

XY_IMPL(int, on_add, unsigned, ref, unsigned, type, uint64_t, v)
{
	OBJ obj;
	SKEL skel;

	if (type != type_mineral)
		return 1;

	nd_get(HD_OBJ, &obj, &ref);
	nd_get(HD_SKEL, &skel, &obj.skid);
	obj.art_id = skel.max_art ? 1 + ((v & 0xf) % skel.max_art) : 0;
	nd_put(HD_OBJ, &ref, &obj);
	return 0;
}

XY_MODULE_API void
xy_install(void)
{
	type_mineral = nd_put(HD_TYPE, NULL, "mineral");

	SKEL skel = {
		.name = "stone",
		.type = type_mineral,
		.max_art = 17,
	};

	stone_skel_id = nd_put(HD_SKEL, NULL, &skel);

	WARN("nd-stone: xy_install, type \"mineral\" = %u, skel %u\n",
		type_mineral, stone_skel_id);
}
