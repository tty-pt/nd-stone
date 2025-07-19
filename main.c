#include <nd/nd.h>

unsigned stone_skel_id, type_mineral;

static inline void
stones_add(unsigned where_ref, struct bio *bio, uint64_t v) {
	unsigned char n = v & 0x3, i;

	if (bio->bio_idx == BIOME_WATER)
		return;

        if (!(n && (v & 0x18) && (v & 0x20)))
                return;

	pos_t pos;
	map_where(pos, where_ref);
	uint32_t v2 = XXH32((const char *) &v, sizeof(v), 2);

        for (int i = 0; i < n; i++, v2 >>= 4) {
		OBJ stone;
                unsigned stone_ref = object_add(&stone, stone_skel_id, where_ref, v2);
		nd_put(HD_OBJ, &stone_ref, &stone);
	}
}

int
on_spawn(unsigned player_ref, unsigned where_ref, struct bio bio, uint64_t v)
{
	stones_add(where_ref, &bio, v);
	return 0;
}

int on_add(unsigned ref, uint64_t v) {
	OBJ obj;
	SKEL skel;
	nd_get(HD_OBJ, &obj, &ref);

	if (obj.type != type_mineral)
		return 1;

	nd_get(HD_SKEL, &skel, &obj.skid);
	obj.art_id = skel.max_art ? 1 + ((v & 0xf) % skel.max_art) : 0;
	nd_put(HD_OBJ, &ref, &obj);
	return 0;
}

void
mod_install(void) {
	unsigned mid;

	type_mineral = nd_put(HD_TYPE, NULL, "mineral");

	SKEL skel = {
		.name = "stone",
		.type = type_mineral,
		.max_art = 17,
	};

	stone_skel_id = nd_put(HD_SKEL, NULL, &skel);
}
