/*
 * bitmaps.h
 *
 *  Created on: Dec 31, 2010
 *      Author: corey
 */

#include <stdint.h>
#include <stdbool.h>

struct bitmap {
	int max_bits;
	uint64_t *map;
};

struct bitmap *alloc_bitmap(int num_bits);

void clear_all(struct bitmap *bm);
void set(struct bitmap *bm, int bit);
void reset(struct bitmap *bm, int bit);
bool is_set(struct bitmap *bm, int bit);

void free_bitmap(struct bitmap *bm);
