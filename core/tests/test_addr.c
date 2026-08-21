#include "manet/addr.h"
#include "test.h"

static void test_boundaries(void)
{
    CHECK_EQ(manet_addr_kind(0x00u), MANET_ADDR_KIND_NULL);

    CHECK_EQ(manet_addr_kind(0x01u), MANET_ADDR_KIND_HANDHELD);
    CHECK_EQ(manet_addr_kind(0x9Fu), MANET_ADDR_KIND_HANDHELD);

    CHECK_EQ(manet_addr_kind(0xA0u), MANET_ADDR_KIND_GATEWAY);
    CHECK_EQ(manet_addr_kind(0xBFu), MANET_ADDR_KIND_GATEWAY);

    CHECK_EQ(manet_addr_kind(0xC0u), MANET_ADDR_KIND_GROUP);
    CHECK_EQ(manet_addr_kind(0xEFu), MANET_ADDR_KIND_GROUP);

    CHECK_EQ(manet_addr_kind(0xF0u), MANET_ADDR_KIND_RESERVED);
    CHECK_EQ(manet_addr_kind(0xFEu), MANET_ADDR_KIND_RESERVED);

    CHECK_EQ(manet_addr_kind(0xFFu), MANET_ADDR_KIND_BROADCAST);
}

/* The kinds must partition the whole space exactly, with the capacities the address
 * map documents. This is the test that catches an off-by-one in a boundary constant. */
static void test_partition(void)
{
    int counts[6] = {0, 0, 0, 0, 0, 0};
    int i;

    for (i = 0; i <= 0xFF; i++) {
        const manet_addr_t a = (manet_addr_t)i;
        const manet_addr_kind_t k = manet_addr_kind(a);

        CHECK(k <= MANET_ADDR_KIND_BROADCAST);
        counts[k]++;

        /* Individual and multicast are disjoint, and neither covers null or reserved. */
        CHECK(!(manet_addr_is_individual(a) && manet_addr_is_multicast(a)));
    }

    CHECK_EQ(counts[MANET_ADDR_KIND_NULL], 1);
    CHECK_EQ(counts[MANET_ADDR_KIND_HANDHELD], 159);
    CHECK_EQ(counts[MANET_ADDR_KIND_GATEWAY], 32);
    CHECK_EQ(counts[MANET_ADDR_KIND_GROUP], 48);
    CHECK_EQ(counts[MANET_ADDR_KIND_RESERVED], 15);
    CHECK_EQ(counts[MANET_ADDR_KIND_BROADCAST], 1);
    CHECK_EQ(counts[0] + counts[1] + counts[2] + counts[3] + counts[4] + counts[5], 256);
}

static void test_predicates(void)
{
    /* Individual covers handheld and gateway, and nothing else. */
    CHECK(manet_addr_is_individual(0x01u));
    CHECK(manet_addr_is_individual(0xA0u));
    CHECK(!manet_addr_is_individual(0x00u));
    CHECK(!manet_addr_is_individual(0xC0u));
    CHECK(!manet_addr_is_individual(0xF0u));
    CHECK(!manet_addr_is_individual(0xFFu));

    /* Multicast covers group and broadcast. */
    CHECK(manet_addr_is_multicast(0xC0u));
    CHECK(manet_addr_is_multicast(0xFFu));
    CHECK(!manet_addr_is_multicast(0x01u));
    CHECK(!manet_addr_is_multicast(0xF0u));

    /* A frame originates from exactly one radio — never a group or broadcast. */
    CHECK(manet_addr_is_valid_source(0x01u));
    CHECK(manet_addr_is_valid_source(0xA5u));
    CHECK(!manet_addr_is_valid_source(0x00u));
    CHECK(!manet_addr_is_valid_source(0xC0u));
    CHECK(!manet_addr_is_valid_source(0xFFu));
    CHECK(!manet_addr_is_valid_source(0xF0u));

    /* Anything but null is addressable — including reserved, so that a node can
     * relay traffic for a node class it does not know about. */
    CHECK(!manet_addr_is_valid_dest(0x00u));
    CHECK(manet_addr_is_valid_dest(0x01u));
    CHECK(manet_addr_is_valid_dest(0xC0u));
    CHECK(manet_addr_is_valid_dest(0xF0u));
    CHECK(manet_addr_is_valid_dest(0xFFu));
}

void test_addr_all(void)
{
    test_boundaries();
    test_partition();
    test_predicates();
}
