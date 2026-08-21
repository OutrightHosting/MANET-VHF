#include "manet/config.h"
#include "test.h"

/*
 * Pins the straw-man budget from ADR-0007. If any of these move, the arithmetic in
 * docs/open-questions.md#oq-0002 is stale and must be updated with it.
 */
void test_config_all(void)
{
    /* Structural invariants, true of any configuration. */
    CHECK_EQ(MANET_SLOT_DURATION_US * MANET_SLOTS_PER_FRAME, MANET_FRAME_DURATION_US);
    CHECK_EQ(MANET_BURST_US + MANET_GUARD_US, MANET_SLOT_DURATION_US);
    CHECK(MANET_SLOT_ONAIR_BITS < MANET_SLOT_RAW_BITS);
    CHECK(MANET_FEC_BITS_AVAILABLE > 0);
    CHECK_EQ(MANET_HEADER_BYTES, (MANET_HEADER_BITS + 7) / 8);

#if (MANET_SLOTS_PER_FRAME == 4L) && (MANET_GROSS_BITRATE_BPS == 19200L) \
    && (MANET_VOICE_PAYLOAD_BITS == 192L) && (MANET_ADDR_BITS == 8u) && (MANET_SEQ_BITS == 8u) && (MANET_TTL_BITS == 5u)
    /* Straw-man values below. Skipped when the configuration is swept — see
     * tools/budget.sh and `make test-3slot`. */
    /* Frame structure */
    CHECK_EQ(MANET_SLOT_DURATION_US, 15000);
    CHECK_EQ(MANET_GUARD_US, 1245);
    CHECK_EQ(MANET_BURST_US, 13755);

    /* Header — 42 bits in 6 bytes since the previous-hop field (OQ-0018) */
    CHECK_EQ(MANET_HEADER_BITS, 43);
    CHECK_EQ(MANET_HEADER_BYTES, 6);

    /* The budget. Six bits for FEC — OQ-0002, made worse by OQ-0018. */
    CHECK_EQ(MANET_SLOT_RAW_BITS, 288);
    CHECK_EQ(MANET_SLOT_ONAIR_BITS, 264);
    CHECK_EQ(MANET_FEC_BITS_AVAILABLE, 5);
    CHECK_EQ(MANET_FEC_PERCENT, 2);

    /* Sanity: the address map's documented capacities */
    CHECK_EQ(MANET_ADDR_HANDHELD_MAX - MANET_ADDR_HANDHELD_MIN + 1, 159);
    CHECK_EQ(MANET_ADDR_GATEWAY_MAX - MANET_ADDR_GATEWAY_MIN + 1, 32);
    CHECK_EQ(MANET_ADDR_GROUP_MAX - MANET_ADDR_GROUP_MIN + 1, 48);
    CHECK_EQ(MANET_ADDR_RESERVED_MAX - MANET_ADDR_RESERVED_MIN + 1, 15);
#endif
}
