#include "manet/dedup.h"
#include "test.h"

void test_dedup_all(void)
{
    manet_dedup_t d;
    unsigned      i;

    manet_dedup_init(&d);
    CHECK_EQ(manet_dedup_count(&d), 0);

    /* First sighting is new; every echo after it is not. */
    CHECK(manet_dedup_check(&d, 0x04u, 0x20u, 100u));
    CHECK(!manet_dedup_check(&d, 0x04u, 0x20u, 101u));
    CHECK(!manet_dedup_check(&d, 0x04u, 0x20u, 102u));
    CHECK_EQ(manet_dedup_count(&d), 1);

    /* Identified by origin and sequence, so a different origin with the same sequence
     * is a different frame — which is what stops one talker's numbering colliding with
     * another's. */
    CHECK(manet_dedup_check(&d, 0x05u, 0x20u, 103u));
    CHECK(manet_dedup_check(&d, 0x04u, 0x21u, 104u));
    CHECK_EQ(manet_dedup_count(&d), 3);

    /* Testing without consuming. */
    CHECK(manet_dedup_seen(&d, 0x04u, 0x20u));
    CHECK(!manet_dedup_seen(&d, 0x09u, 0x01u));
    CHECK_EQ(manet_dedup_count(&d), 3);

    /* The ring is finite and overwrites oldest-first rather than growing. */
    manet_dedup_init(&d);
    for (i = 0u; i < MANET_DEDUP_DEPTH; i++) {
        CHECK(manet_dedup_check(&d, 0x01u, (uint8_t)i, 200u + i));
    }
    CHECK_EQ(manet_dedup_count(&d), MANET_DEDUP_DEPTH);
    CHECK(manet_dedup_check(&d, 0x01u, (uint8_t)MANET_DEDUP_DEPTH, 300u));
    CHECK_EQ(manet_dedup_count(&d), MANET_DEDUP_DEPTH);
    CHECK(!manet_dedup_seen(&d, 0x01u, 0u)); /* the oldest was evicted */

    /* Ageing matters because sequence numbers are only 8 bits and wrap. Without it a
     * wrapped sequence would look like an echo and be dropped. OQ-0012. */
    manet_dedup_init(&d);
    CHECK(manet_dedup_check(&d, 0x02u, 0x07u, 1000u));
    CHECK_EQ(manet_dedup_expire(&d, 1500u, 1000u), 0);
    CHECK(!manet_dedup_check(&d, 0x02u, 0x07u, 1500u));
    CHECK_EQ(manet_dedup_expire(&d, 2001u, 1000u), 1);
    CHECK(manet_dedup_check(&d, 0x02u, 0x07u, 2001u));
}
