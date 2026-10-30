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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ttypt/corm.h>

/* XXH32 for the art-variant hash. The old module got this from <nd/nd.h>,
 * which is gone; nothing in nd/ exposes it. <xxhash.h> is self-contained
 * for XXH32 (no -lxxhash needed), and the call is unchanged, so the variants
 * this picks are the same ones the old engine picked. */
#include <xxhash.h>

#include <nd/xy.h>
#include <nd/core.h>
#include <nd/stone.h>

/* Mirrors CORE_ET_* in nd-view's type.h (the installed nd/core.h predates it). */
#ifndef CORE_ET_ASCII
#define CORE_ET_ASCII 0x80u
#define CORE_ET_SIDE_MASK 0x03u
#endif

unsigned stone_skel_id, rock_skel_id, type_mineral;

static struct bio
stone_noise_decorate(struct bio bio, uint32_t he, uint32_t w, uint32_t tm, uint32_t cl)
{
	(void)cl;
	stone_tile_t *sd = (stone_tile_t *)&bio.raw[4];
	memset(sd, 0, sizeof(*sd));

	if (he <= w || bio.bio_idx == BIOME_WATER)
		return bio;

	uint32_t sv = XXH32(&he, sizeof(uint32_t), 0x5709Eu ^ tm);
	unsigned char n = sv & 0x3;
	if (!(n && (sv & 0x18) && (sv & 0x20)))
		return bio;

	unsigned char placed = 0;
	for (int i = 0; i < n && i < 3; i++) {
		unsigned side = (unsigned)((sv >> (8 + i * 3)) % 3);
		if ((placed >> side) & 1)
			side = (side + 1) % 3;
		placed |= (unsigned char)(1 << side);

		sd->id[side] = stone_skel_id;
		/* size: 1 = small stone ('*'), 2 = medium rock ('o'), 3 = big boulder ('O') */
		unsigned char sz = stone_size_roll(sv >> (16 + i * 3));
		sd->n |= (unsigned char)(sz << (side * 2));
	}
	sd->max = n;

	return bio;
}

#define STONE_HARVEST_MAX 1024
#define STONE_RESPAWN_COOLDOWN 300 /* 5 minutes */

typedef struct {
	unsigned room;
	uint32_t harvested_at;
} stone_harvest_entry_t;

static stone_harvest_entry_t s_harvest[STONE_HARVEST_MAX];
static size_t s_harvest_n = 0;

static int is_room_observed(unsigned room_ref)
{
	pos_t room_pos;
	if (map_where(room_pos, room_ref) != 0)
		return 1; /* safe fallback: assume observed if position unknown */

	unsigned fds_hd = nd_table_handle(HD_FD);
	if (!fds_hd)
		return 0;

	unsigned c = corm_iter(fds_hd, NULL, 0);
	const void *kp, *vp;
	int observed = 0;

	while (corm_next(&kp, &vp, c)) {
		unsigned player_ref = *(const unsigned *)kp;
		OBJ player;
		if (nd_get(HD_OBJ, &player, &player_ref) != 0)
			continue;
		pos_t player_pos;
		if (map_where(player_pos, player.location) != 0)
			continue;

		if (player_pos[3] != room_pos[3] || player_pos[2] != room_pos[2])
			continue;

		int dx = (int)room_pos[0] - (int)player_pos[0];
		int dy = (int)room_pos[1] - (int)player_pos[1];
		if (dx < 0) dx = -dx;
		if (dy < 0) dy = -dy;

		if (dx <= 3 && dy <= 3) {
			observed = 1;
			break;
		}
	}
	corm_fin(c);
	return observed;
}

static sic_str_t
stone_empty_tile_decorate(view_tile_t t, unsigned side, sic_str_t ss)
{
	/* If cell is already claimed by flora or another decorator, leave it */
	if (ss.str[0] && ss.str[0] != ' ')
		return ss;

	const stone_tile_t *sd = (const stone_tile_t *)&t.raw[4];
	int ascii = side & CORE_ET_ASCII;

	side &= CORE_ET_SIDE_MASK;
	unsigned char sz = (unsigned char)((sd->n >> (side * 2)) & 3);
	if (!sz)
		return ss;

	/* If room is instantiated in the world, check if resource was harvested */
	if (t.room != NOTHING && t.room != 0) {
		unsigned c_hd = nd_table_handle(HD_CONTENTS);
		if (c_hd) {
			unsigned c = corm_iter(c_hd, &t.room, CM_RANGE);
			const void *kp, *vp;
			int has_mineral = 0;
			while (corm_next(&kp, &vp, c)) {
				unsigned thing_ref = *(const unsigned *)vp;
				OBJ obj;
				if (nd_get(HD_OBJ, &obj, &thing_ref) == 0) {
					if (obj.type == type_mineral) {
						has_mineral = 1;
						break;
					}
				}
			}
			corm_fin(c);
			if (!has_mineral) {
				/* The stone was picked up and is no longer in the room */
				OBJ room_obj;
				if (nd_get(HD_OBJ, &room_obj, &t.room) == 0) {
					ROO *rroom = (ROO *)&room_obj.data;
					if (!(rroom->flags & RF_TEMP) || room_obj.owner != 0) {
						/* Claimed rooms never replenish stones */
						return ss;
					}
				}
				uint32_t now = (uint32_t)time(NULL);
				int found = -1;
				for (size_t i = 0; i < s_harvest_n; i++) {
					if (s_harvest[i].room == t.room) {
						found = (int)i;
						break;
					}
				}
				if (found < 0) {
					if (s_harvest_n < STONE_HARVEST_MAX) {
						s_harvest[s_harvest_n].room = t.room;
						s_harvest[s_harvest_n].harvested_at = now;
						s_harvest_n++;
					}
					return ss;
				}
				/* If on cooldown, stay harvested */
				if (now - s_harvest[found].harvested_at < STONE_RESPAWN_COOLDOWN) {
					return ss;
				}
				/* Cooldown expired: check if unobserved */
				if (is_room_observed(t.room)) {
					return ss; /* still in view: suppress respawn until players walk away */
				}
				/* Unobserved and cooldown expired: respawn the stone! */
				OBJ stone;
				uint32_t v2 = (uint32_t)now;
				unsigned skid = sd->id[side];
				if (!skid)
					skid = (sz >= 2 && rock_skel_id != NOTHING) ? rock_skel_id : stone_skel_id;
				unsigned stone_ref = object_add(&stone, skid, t.room, v2);
				if (stone_ref != NOTHING)
					nd_put(HD_OBJ, &stone_ref, &stone);

				s_harvest[found] = s_harvest[--s_harvest_n];
			}
		}
	}

	char *b = ss.str;
	char glyph;
	/* The ASCII map only hints at rocks, dim grey (never a bold white O),
	 * and never draws small stones; flora, entities and stairs already won
	 * the cell above. map_data (Seven, lamina) gets every mineral. */
	if (ascii && sz < 2)
		return ss;
	switch (sz) {
	case 1:
		glyph = '*';
		b = stpcpy(b, ANSI_FG_WHITE);
		break;
	case 2:
		glyph = 'o';
		b = stpcpy(b, ascii ? ANSI_BOLD ANSI_FG_BLACK : ANSI_FG_YELLOW);
		break;
	case 3:
		glyph = 'O';
		b = stpcpy(b, ascii ? ANSI_BOLD ANSI_FG_BLACK : ANSI_BOLD ANSI_FG_WHITE);
		break;
	default:
		return ss;
	}
	*b++ = glyph;
	b = stpcpy(b, ANSI_RESET);
	*b = '\0';
	return ss;
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
	const stone_tile_t *sd = (const stone_tile_t *)&bio.raw[4];
	for (unsigned s = 0; s < 3; s++) {
		unsigned char sz = (unsigned char)((sd->n >> (s * 2)) & 3);
		if (!sz)
			continue;
		OBJ stone;
		uint32_t v2 = (uint32_t)(v ^ (s * 0x9e3779b9u));
		unsigned skid = sd->id[s];
		if (!skid)
			skid = (sz >= 2 && rock_skel_id != NOTHING) ? rock_skel_id : stone_skel_id;
		unsigned stone_ref = object_add(&stone, skid,
			loc_ref, v2);
		if (stone_ref != NOTHING)
			nd_put(HD_OBJ, &stone_ref, &stone);
	}
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

static int
stone_row(const char *const *f, size_t n, void *user)
{
	(void)user;
	if (n < 3)
		return 0;
	SKEL skel = {
		.type = type_mineral,
		.max_art = (unsigned)atoi(f[2]),
	};
	strlcpy((char *)skel.name, f[0], sizeof(skel.name));
	unsigned id = nd_put(HD_SKEL, NULL, &skel);
	if (!strcmp(f[0], "rock"))
		rock_skel_id = id;
	else
		stone_skel_id = id;
	return 0;
}

XY_MODULE_API void
xy_install(void)
{
	xy_load("libnd-core");
	type_mineral = nd_put(HD_TYPE, NULL, "mineral");

	if (nd_table_load("nd-stone", "stones", stone_row, NULL) < 0) {
		SKEL skel = {
			.name = "stone",
			.type = type_mineral,
			.max_art = 17,
		};
		stone_skel_id = nd_put(HD_SKEL, NULL, &skel);
	}
	if (rock_skel_id == NOTHING) {
		SKEL rskel = {
			.name = "rock",
			.type = type_mineral,
			.max_art = 17,
		};
		rock_skel_id = nd_put(HD_SKEL, NULL, &rskel);
	}

	core_noise_decorate(stone_noise_decorate);
	core_empty_tile_decorate(stone_empty_tile_decorate);

	WARN("nd-stone: xy_install, type \"mineral\" = %u, skel %u\n",
		type_mineral, stone_skel_id);
}
