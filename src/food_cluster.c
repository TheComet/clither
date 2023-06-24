#include "clither/food_cluster.h"
#include <stdlib.h>

/* ------------------------------------------------------------------------- */
cs_hash32
food_cluster_init(struct food_cluster* fc, struct qwpos center, uint8_t food_count, cs_hash32 seed)
{
	int i;
	const qw size_2 = FOOD_CLUSTER_SIZE / 2 - 1;
	fc->food_count = food_count;
	fc->aabb = make_qwaabbqw(
		qw_sub(center.x, size_2), qw_sub(center.y, size_2),
		qw_add(center.y, size_2), qw_add(center.y, size_2));
	fc->seed = seed;

	for (i = 0; i != (int)food_count; ++i)
	{
		/* Distribute linearly on X axis*/
		fc->food[i].x = qw_add(fc->aabb.x1, (i * FOOD_CLUSTER_SIZE / i) & ~0xFF);

		/* Distribute randomly on Y axis */
		fc->seed = hash32_jenkins_oaat(&fc->seed, sizeof(fc->seed));
		fc->food[i].y = qw_add(fc->aabb.y1, fc->seed & (FOOD_CLUSTER_SIZE - 1) & ~0xFF);
	}
	/* Note: X and Y positions are quantized with mask 0x7F00 (7 bits) at this point */

	return seed;
}

/* ------------------------------------------------------------------------- */
int
food_eat(struct food_cluster* fc, struct qwpos eat_center, qw eat_range)
{
	return 0;
}

/* ------------------------------------------------------------------------- */
int
food_compress_bytes_required(const struct food_cluster* fc)
{
	return 0;
}

/* ------------------------------------------------------------------------- */
int
food_compress(uint8_t* buf, int len, const struct food_cluster* fc)
{
	return 0;
}

/* ------------------------------------------------------------------------- */
int
food_decompress(struct food_cluster* fc, const uint8_t* buf, int len)
{
	return 0;
}
