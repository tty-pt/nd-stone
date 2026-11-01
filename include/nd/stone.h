/* stone.h — nd-stone's cross-module type: the map tile stones/minerals appear on. */

#ifndef ND_STONE_H
#define ND_STONE_H

typedef struct {
	unsigned id[3];
	unsigned char n, max;
} stone_tile_t;

/* Size of one stone from 3 hash bits: 5/8 small stone (1), 2/8 rock (2),
 * 1/8 boulder (3). Mostly pebbles, some rocks, the odd boulder. */
static inline unsigned char
stone_size_roll(unsigned bits)
{
	unsigned r = bits & 7;

	return (unsigned char)(r < 5 ? 1 : r < 7 ? 2 : 3);
}

#endif /* !ND_STONE_H */
