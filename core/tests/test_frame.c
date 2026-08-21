#include <string.h>

#include "manet/frame.h"
#include "test.h"

/*
 * Golden wire vector. Pins the header layout so it cannot drift silently.
 *
 *   src  8  0x12  0001 0010   (origin — never rewritten)
 *   prev 8  0x34  0011 0100   (last hop — rewritten every hop)
 *   dst  8  0xC5  1100 0101
 *   type 4  0x0   0000
 *   seq  8  0x9A  1001 1010
 *   ttl  4  0xF   1111
 *   prio 2  0x1   01
 *
 * MSB first:          00010010 00110100 11000101 0000 10011010 1111 01
 * Grouped into bytes: 0x12 0x34 0xC5 0x09 0xAF then '01' in the top two bits of [5].
 */
static void test_golden_vector(void)
{
    static const manet_header_t h = {
        0x12u, 0x34u, 0xC5u, MANET_FRAME_VOICE, 0x9Au, 0x0Fu, MANET_PRIO_VOICE
    };
    uint8_t buf[8];

    /* Zero-filled: the six bits past the header stay zero. */
    memset(buf, 0x00, sizeof buf);
    CHECK_EQ(manet_header_pack(&h, buf, sizeof buf), MANET_OK);
    CHECK_EQ(buf[0], 0x12);
    CHECK_EQ(buf[1], 0x34);
    CHECK_EQ(buf[2], 0xC5);
    CHECK_EQ(buf[3], 0x09);
    CHECK_EQ(buf[4], 0xAF);
    CHECK_EQ(buf[5], 0x40);

    /* One-filled: the same 34 bits are written, and the trailing six are untouched.
     * This is what lets a caller pack a header into a buffer that already holds
     * payload or FEC without clobbering it. */
    memset(buf, 0xFF, sizeof buf);
    CHECK_EQ(manet_header_pack(&h, buf, sizeof buf), MANET_OK);
    CHECK_EQ(buf[0], 0x12);
    CHECK_EQ(buf[1], 0x34);
    CHECK_EQ(buf[2], 0xC5);
    CHECK_EQ(buf[3], 0x09);
    CHECK_EQ(buf[4], 0xAF);
    CHECK_EQ(buf[5], 0x7F);
    CHECK_EQ(buf[6], 0xFF);
}

static void test_roundtrip(void)
{
    uint8_t buf[MANET_HEADER_BYTES];
    unsigned type, ttl, prio, i;

    /* Every type x ttl x priority combination, with src/dst walking the space. */
    i = 0u;
    for (type = 0u; type < (1u << MANET_TYPE_BITS); type++) {
        for (ttl = 0u; ttl < (1u << MANET_TTL_BITS); ttl++) {
            for (prio = 0u; prio < (1u << MANET_PRIO_BITS); prio++) {
                manet_header_t in;
                manet_header_t out;

                in.src  = (manet_addr_t)(i & 0xFFu);
                in.prev = (manet_addr_t)((i * 3u) & 0xFFu);
                in.dst  = (manet_addr_t)((i * 7u) & 0xFFu);
                in.type = (uint8_t)type;
                in.seq  = (uint8_t)((i * 31u) & 0xFFu);
                in.ttl  = (uint8_t)ttl;
                in.prio = (uint8_t)prio;
                i++;

                memset(buf, 0x00, sizeof buf);
                CHECK_EQ(manet_header_pack(&in, buf, sizeof buf), MANET_OK);

                memset(&out, 0xAA, sizeof out);
                CHECK_EQ(manet_header_unpack(&out, buf, sizeof buf), MANET_OK);

                CHECK_EQ(out.src, in.src);
                CHECK_EQ(out.prev, in.prev);
                CHECK_EQ(out.dst, in.dst);
                CHECK_EQ(out.type, in.type);
                CHECK_EQ(out.seq, in.seq);
                CHECK_EQ(out.ttl, in.ttl);
                CHECK_EQ(out.prio, in.prio);
            }
        }
    }

    /* Every source and sequence value, independently. */
    for (i = 0u; i <= 0xFFu; i++) {
        manet_header_t in  = { 0x01u, 0x02u, 0xFFu, MANET_FRAME_BEACON, 0x00u, 0x01u, MANET_PRIO_SIGNALLING };
        manet_header_t out;

        in.src = (manet_addr_t)i;
        in.seq = (uint8_t)(0xFFu - i);
        if (!manet_addr_is_individual(in.src)) { continue; }

        memset(buf, 0x00, sizeof buf);
        CHECK_EQ(manet_header_pack(&in, buf, sizeof buf), MANET_OK);
        CHECK_EQ(manet_header_unpack(&out, buf, sizeof buf), MANET_OK);
        CHECK_EQ(out.src, in.src);
        CHECK_EQ(out.seq, in.seq);
    }
}

static void test_pack_rejects(void)
{
    const manet_header_t h = { 0x01u, 0x02u, 0xFFu, MANET_FRAME_VOICE, 0x00u, 0x0Fu, MANET_PRIO_VOICE };
    manet_header_t bad;
    uint8_t buf[MANET_HEADER_BYTES];

    CHECK_EQ(manet_header_pack(NULL, buf, sizeof buf), MANET_ERR_NULL_ARG);
    CHECK_EQ(manet_header_pack(&h, NULL, sizeof buf), MANET_ERR_NULL_ARG);

    /* One byte short of the header. */
    CHECK_EQ(manet_header_pack(&h, buf, MANET_HEADER_BYTES - 1u), MANET_ERR_BUFFER);

    /* Fields wider than their configured width must not reach the air, even though
     * the C type can hold them. */
    bad = h;
    bad.ttl = (uint8_t)(MANET_TTL_MAX + 1u);
    CHECK_EQ(manet_header_pack(&bad, buf, sizeof buf), MANET_ERR_FIELD_RANGE);

    bad = h;
    bad.type = 0x10u;
    CHECK_EQ(manet_header_pack(&bad, buf, sizeof buf), MANET_ERR_FIELD_RANGE);

    bad = h;
    bad.prio = 0x04u;
    CHECK_EQ(manet_header_pack(&bad, buf, sizeof buf), MANET_ERR_FIELD_RANGE);
}

/*
 * The upgradability rule, and the most important behaviour in this module: unpack is
 * structural and rejects nothing on semantic grounds, so a node can relay traffic for
 * a network newer than itself. See the note at the top of manet/frame.h.
 */
static void test_unpack_is_structural(void)
{
    manet_header_t out;
    uint8_t buf[MANET_HEADER_BYTES];

    /* A frame of a type this build has never heard of, addressed to a reserved node
     * class, from a source this build would refuse to originate from. All must parse. */
    {
        const manet_header_t exotic = { 0x0Au, 0x0Bu, 0xF3u, 0x0Fu, 0x77u, 0x05u, MANET_PRIO_DATA };

        memset(buf, 0x00, sizeof buf);
        CHECK_EQ(manet_header_pack(&exotic, buf, sizeof buf), MANET_OK);
        CHECK_EQ(manet_header_unpack(&out, buf, sizeof buf), MANET_OK);
        CHECK_EQ(out.type, 0x0F);
        CHECK_EQ(out.dst, 0xF3);
        CHECK(!manet_frame_type_is_known(out.type));

        /* ...but validate, which decides local delivery and origination, says no. */
        CHECK_EQ(manet_header_validate(&out), MANET_ERR_RESERVED_TYPE);

        /* The frame is still relayable: TTL decrements normally. */
        CHECK(manet_header_ttl_decrement(&out));
        CHECK_EQ(out.ttl, 0x04);
    }

    /* Every possible bit pattern of a full header must unpack without error. */
    {
        unsigned b0, b1;
        for (b0 = 0u; b0 <= 0xFFu; b0++) {
            for (b1 = 0u; b1 <= 0xFFu; b1++) {
                memset(buf, (int)b1, sizeof buf);
                buf[0] = (uint8_t)b0;
                CHECK_EQ(manet_header_unpack(&out, buf, sizeof buf), MANET_OK);
            }
        }
    }

    CHECK_EQ(manet_header_unpack(NULL, buf, sizeof buf), MANET_ERR_NULL_ARG);
    CHECK_EQ(manet_header_unpack(&out, NULL, sizeof buf), MANET_ERR_NULL_ARG);
    CHECK_EQ(manet_header_unpack(&out, buf, MANET_HEADER_BYTES - 1u), MANET_ERR_BUFFER);
}

static void test_validate(void)
{
    const manet_header_t good = { 0x05u, 0x02u, 0xC1u, MANET_FRAME_VOICE, 0x20u, 0x0Fu, MANET_PRIO_VOICE };
    manet_header_t h;

    CHECK_EQ(manet_header_validate(&good), MANET_OK);
    CHECK_EQ(manet_header_validate(NULL), MANET_ERR_NULL_ARG);

    /* A gateway may originate — it is an ordinary node (Addendum 01 s4). */
    h = good; h.src = 0xA0u;
    CHECK_EQ(manet_header_validate(&h), MANET_OK);

    /* A group, a broadcast address, a reserved address and null may not. */
    h = good; h.src = 0xC0u;
    CHECK_EQ(manet_header_validate(&h), MANET_ERR_BAD_SOURCE);
    h = good; h.src = 0xFFu;
    CHECK_EQ(manet_header_validate(&h), MANET_ERR_BAD_SOURCE);
    h = good; h.src = 0xF5u;
    CHECK_EQ(manet_header_validate(&h), MANET_ERR_BAD_SOURCE);
    h = good; h.src = 0x00u;
    CHECK_EQ(manet_header_validate(&h), MANET_ERR_BAD_SOURCE);

    /* Broadcast and group are fine as destinations; null is not. */
    h = good; h.dst = 0xFFu;
    CHECK_EQ(manet_header_validate(&h), MANET_OK);
    h = good; h.dst = 0x00u;
    CHECK_EQ(manet_header_validate(&h), MANET_ERR_BAD_DEST);

    h = good; h.type = 0x08u;
    CHECK_EQ(manet_header_validate(&h), MANET_ERR_RESERVED_TYPE);

    h = good; h.ttl = 0x00u;
    CHECK_EQ(manet_header_validate(&h), MANET_ERR_TTL_EXPIRED);
}

static void test_ttl(void)
{
    manet_header_t h = { 0x01u, 0x02u, 0xFFu, MANET_FRAME_VOICE, 0x00u, 0x03u, MANET_PRIO_VOICE };

    CHECK(manet_header_ttl_decrement(&h));   /* 3 -> 2, still relayable */
    CHECK_EQ(h.ttl, 2);
    CHECK(manet_header_ttl_decrement(&h));   /* 2 -> 1 */
    CHECK_EQ(h.ttl, 1);
    CHECK(!manet_header_ttl_decrement(&h));  /* 1 -> 0, expired: drop, do not relay */
    CHECK_EQ(h.ttl, 0);
    CHECK(!manet_header_ttl_decrement(&h));  /* stays at 0, never wraps */
    CHECK_EQ(h.ttl, 0);
    CHECK(!manet_header_ttl_decrement(NULL));

    /* The configured TTL width must reach the brief's stated 15-hop maximum. */
    CHECK_EQ(MANET_TTL_MAX, 15);
}

static void test_default_priority(void)
{
    CHECK_EQ(manet_frame_default_priority(MANET_FRAME_EMERGENCY), MANET_PRIO_EMERGENCY);
    CHECK_EQ(manet_frame_default_priority(MANET_FRAME_VOICE), MANET_PRIO_VOICE);
    CHECK_EQ(manet_frame_default_priority(MANET_FRAME_VOICE_END), MANET_PRIO_VOICE);
    CHECK_EQ(manet_frame_default_priority(MANET_FRAME_BEACON), MANET_PRIO_SIGNALLING);
    CHECK_EQ(manet_frame_default_priority(MANET_FRAME_TC), MANET_PRIO_SIGNALLING);
    CHECK_EQ(manet_frame_default_priority(MANET_FRAME_TEXT), MANET_PRIO_DATA);
    CHECK_EQ(manet_frame_default_priority(MANET_FRAME_POSITION), MANET_PRIO_DATA);
    CHECK_EQ(manet_frame_default_priority(MANET_FRAME_CONFIG), MANET_PRIO_DATA);

    /* An unrecognised frame type can never pre-empt voice. */
    {
        unsigned t;
        for (t = (unsigned)MANET_FRAME_TYPE_MAX + 1u; t < (1u << MANET_TYPE_BITS); t++) {
            CHECK_EQ(manet_frame_default_priority((uint8_t)t), MANET_PRIO_DATA);
        }
    }
}

static void test_type_known(void)
{
    unsigned t;
    for (t = 0u; t < (1u << MANET_TYPE_BITS); t++) {
        const bool known = manet_frame_type_is_known((uint8_t)t);
        CHECK_EQ(known, t <= (unsigned)MANET_FRAME_TYPE_MAX);
    }
}

void test_frame_all(void)
{
    test_golden_vector();
    test_roundtrip();
    test_pack_rejects();
    test_unpack_is_structural();
    test_validate();
    test_ttl();
    test_default_priority();
    test_type_known();
}
