#include "manet/config.h"
#include "test.h"

/*
 * Pins the straw-man budget from ADR-0007. If any of these move, the arithmetic in
 * docs/open-questions.md#oq-0002 is stale and must be updated with it.
 */
void test_config_all(void)
{
    /* Frame structure */
    CHECK_EQ(MANET_SLOT_DURATION_US, 15000);
    CHECK_EQ(MANET_GUARD_US, 1245);
    CHECK_EQ(MANET_BURST_US, 13755);

    /* Header — ADR-0007 straw-man is 34 bits in 5 bytes */
    CHECK_EQ(MANET_HEADER_BITS, 34);
    CHECK_EQ(MANET_HEADER_BYTES, 5);

    /* The budget, and the number that makes OQ-0002 a blocker: 14 bits for FEC */
    CHECK_EQ(MANET_SLOT_RAW_BITS, 288);
    CHECK_EQ(MANET_SLOT_ONAIR_BITS, 264);
    CHECK_EQ(MANET_FEC_BITS_AVAILABLE, 14);
    CHECK_EQ(MANET_FEC_PERCENT, 6);

    /* Sanity: the address map's documented capacities */
    CHECK_EQ(MANET_ADDR_HANDHELD_MAX - MANET_ADDR_HANDHELD_MIN + 1, 159);
    CHECK_EQ(MANET_ADDR_GATEWAY_MAX - MANET_ADDR_GATEWAY_MIN + 1, 32);
    CHECK_EQ(MANET_ADDR_GROUP_MAX - MANET_ADDR_GROUP_MIN + 1, 48);
    CHECK_EQ(MANET_ADDR_RESERVED_MAX - MANET_ADDR_RESERVED_MIN + 1, 15);
}
