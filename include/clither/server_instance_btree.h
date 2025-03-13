#pragma once

#include "clither/btree.h"
#include "clither/server_instance.h"

BTREE_DECLARE(server_instance_btree, uint16_t, struct server_instance, 16)
