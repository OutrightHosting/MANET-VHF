#include "manet/addr.h"

manet_addr_kind_t manet_addr_kind(manet_addr_t a)
{
    /* Order matters: the two singleton addresses sit at the ends of the space. */
    if (a == MANET_ADDR_NULL) {
        return MANET_ADDR_KIND_NULL;
    }
    if (a == MANET_ADDR_BROADCAST) {
        return MANET_ADDR_KIND_BROADCAST;
    }
    if (a <= MANET_ADDR_HANDHELD_MAX) {
        return MANET_ADDR_KIND_HANDHELD;
    }
    if (a <= MANET_ADDR_GATEWAY_MAX) {
        return MANET_ADDR_KIND_GATEWAY;
    }
    if (a <= MANET_ADDR_GROUP_MAX) {
        return MANET_ADDR_KIND_GROUP;
    }
    return MANET_ADDR_KIND_RESERVED;
}

bool manet_addr_is_individual(manet_addr_t a)
{
    const manet_addr_kind_t k = manet_addr_kind(a);
    return k == MANET_ADDR_KIND_HANDHELD || k == MANET_ADDR_KIND_GATEWAY;
}

bool manet_addr_is_multicast(manet_addr_t a)
{
    const manet_addr_kind_t k = manet_addr_kind(a);
    return k == MANET_ADDR_KIND_GROUP || k == MANET_ADDR_KIND_BROADCAST;
}

bool manet_addr_is_valid_source(manet_addr_t a)
{
    return manet_addr_is_individual(a);
}

bool manet_addr_is_valid_dest(manet_addr_t a)
{
    return manet_addr_kind(a) != MANET_ADDR_KIND_NULL;
}
