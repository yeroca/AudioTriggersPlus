/*
 * bitmaps.c
 *
 *  Created on: Dec 31, 2010
 *      Author: corey
 */
#include "bitmaps.h"
#include <stdlib.h>

struct bitmap *alloc_bitmap(int num_bits)
{
	struct bitmap *bm = malloc(sizeof(struct bitmap));

	bm->max_bits = ((num_bits + 63) / 64) * 64;
	bm->map = malloc(bm->max_bits / 8);
	return bm;
}

void clear_all(struct bitmap *bm)
{
	int i, num_words = bm->max_bits / 64;

	for (i = 0; i < num_words; i++) {
		bm->map[i] = 0;
	}
}

void set(struct bitmap *bm, int bit)
{
	bm->map[bit / 64] |= 0x1ULL << (bit % 64);
}

void reset(struct bitmap *bm, int bit)
{
	bm->map[bit / 64] &= ~(0x1ULL << (bit % 64));
}


bool is_set(struct bitmap *bm, int bit)
{
	return (bm->map[bit / 64] & (0x1ULL << (bit % 64))) != 0;
}

void free_bitmap(struct bitmap *bm)
{
	free(bm->map);
	free(bm);
}
