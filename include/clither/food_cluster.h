#pragma once

#include "clither/config.h"
#include "clither/q.h"
#include "cstructures/hash.h"

C_BEGIN

#if 0
struct food_grid
{
	struct cs_vector food_clusters;  /* struct food_cluster */
};

struct food_corpse
{
	struct cs_vector bezier_handles;
};
#endif

/*!
 * \brief Defines a group of food pieces that are randomly distributed
 * within the AABB.
 * 
 * Details on implementation: We can get away with quite a bit of cheating
 * with no significant visual impact on how random the distribution looks.
 *   1) The food only needs to be distributed on one of the two axes randomly
 *      (in this case the Y axis was chosen). On the X axis the food is spaced
 *      at even intervals linearly from aabb.x1 to aabb.x2.
 *   2) The food position can be quantized from the normally 16 bits (set by
 *      food_cluster_size()) down to 7 or 8 bits without any significant visual
 *      impact, saving 8 bits of information.
 */
struct food_cluster
{
	struct qwaabb aabb;
	struct qwpos food[254];
	uint8_t food_count;
	cs_hash32 seed;
};

/* NOTE: Changing this also changes the quantization resolution and bit offsets.
 *       you'll need to make sure to update those in food_cluster.c */
#define FOOD_CLUSTER_SIZE  0x8000
#define FOOD_CLUSTER_QUANT 0x7F00  /* Top 7 bits are used */

cs_hash32
food_cluster_init(struct food_cluster* fc, struct qwpos center, uint8_t food_count, cs_hash32 seed);

int
food_eat(struct food_cluster* fc, struct qwpos eat_center, qw eat_range);

int
food_compress_bytes_required(const struct food_cluster* fc);

int
food_compress(uint8_t* buf, int len, const struct food_cluster* fc);

int
food_decompress(struct food_cluster* fc, const uint8_t* buf, int len);

C_END
