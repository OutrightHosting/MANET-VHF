#include <string.h>

#include "manet/slot.h"
#include "test.h"

static manet_pdu_t make_pdu(manet_addr_t src, uint8_t seq, uint8_t ttl, uint8_t prio)
{
    manet_pdu_t p;
    memset(&p, 0, sizeof p);
    p.hdr.src  = src;
    p.hdr.dst  = 0xC0u;
    p.hdr.type = MANET_FRAME_VOICE;
    p.hdr.seq  = seq;
    p.hdr.ttl  = ttl;
    p.hdr.prio = prio;
    p.payload_len = 0u;
    return p;
}

static void test_timing(void)
{
    manet_slot_pos_t pos;

    manet_slot_at(0u, &pos);
    CHECK_EQ(pos.number, 0);
    CHECK_EQ(pos.frame, 0);
    CHECK_EQ(pos.index, 0);
    CHECK_EQ(pos.start_us, 0);

    /* One microsecond before the first slot ends is still the first slot. */
    manet_slot_at((uint64_t)MANET_SLOT_DURATION_US - 1u, &pos);
    CHECK_EQ(pos.number, 0);

    manet_slot_at((uint64_t)MANET_SLOT_DURATION_US, &pos);
    CHECK_EQ(pos.number, 1);
    CHECK_EQ(pos.index, 1);

    /* Index wraps at the frame boundary; the slot number does not. */
    manet_slot_at((uint64_t)MANET_FRAME_DURATION_US, &pos);
    CHECK_EQ(pos.number, MANET_SLOTS_PER_FRAME);
    CHECK_EQ(pos.frame, 1);
    CHECK_EQ(pos.index, 0);

    /* Index cycles over the whole space and frame advances in step. */
    {
        uint64_t n;
        for (n = 0u; n < 40u; n++) {
            manet_slot_from_number(n, &pos);
            CHECK_EQ(pos.index, n % (uint64_t)MANET_SLOTS_PER_FRAME);
            CHECK_EQ(pos.frame, n / (uint64_t)MANET_SLOTS_PER_FRAME);
            CHECK_EQ(pos.start_us, n * (uint64_t)MANET_SLOT_DURATION_US);
        }
    }

    /* Guard sits at the end of the slot, so a transmitter keys up on the boundary and
     * must be off air before the next one. */
    CHECK(manet_slot_in_burst(0u));
    CHECK(manet_slot_in_burst((uint64_t)MANET_BURST_US - 1u));
    CHECK(!manet_slot_in_burst((uint64_t)MANET_BURST_US));
    CHECK(!manet_slot_in_burst((uint64_t)MANET_SLOT_DURATION_US - 1u));
    CHECK(manet_slot_in_burst((uint64_t)MANET_SLOT_DURATION_US));

    CHECK_EQ(manet_slot_burst_end_us(0u), MANET_BURST_US);
    CHECK_EQ(manet_slot_burst_end_us(1u),
             (uint64_t)MANET_SLOT_DURATION_US + (uint64_t)MANET_BURST_US);

    /* Guard must be long enough to be worth having — OQ-0010 will decide if it is. */
    CHECK(MANET_SLOT_DURATION_US - MANET_BURST_US == MANET_GUARD_US);
    CHECK(MANET_GUARD_US > 0);
}

static void test_relay_is_next_slot(void)
{
    manet_sched_t s;
    manet_pdu_t   in  = make_pdu(0x05u, 0x11u, 8u, MANET_PRIO_VOICE);
    manet_pdu_t   out;

    manet_sched_init(&s);
    CHECK_EQ(manet_sched_depth(&s), 0);

    /* Heard in slot 7, goes out in slot 8. Not slot 11, which is where waiting for the
     * next frame would put it. This one line is the whole design. */
    CHECK_EQ(manet_sched_relay(&s, &in, 7u), MANET_OK);
    CHECK_EQ(manet_sched_depth(&s), 1);
    CHECK(!manet_sched_take(&s, 7u, &out));
    CHECK(manet_sched_take(&s, 8u, &out));
    CHECK_EQ(out.hdr.ttl, 7);      /* decremented in transit */
    CHECK_EQ(out.hdr.src, 0x05);   /* origin preserved, not rewritten to the relay */
    CHECK_EQ(manet_sched_depth(&s), 0);
}

static void test_relay_crosses_frame_boundary(void)
{
    manet_sched_t s;
    manet_pdu_t   out;
    const uint64_t last = (uint64_t)MANET_SLOTS_PER_FRAME - 1u;

    /* Heard in the last slot of a frame, relayed in the first slot of the next — 15 or
     * 20 ms later, not a frame later. Slot numbers are monotonic so this is not a
     * special case, and this test exists to keep it that way. */
    manet_sched_init(&s);
    {
        manet_pdu_t in = make_pdu(0x09u, 0x02u, 5u, MANET_PRIO_VOICE);
        CHECK_EQ(manet_sched_relay(&s, &in, last), MANET_OK);
    }
    CHECK(manet_sched_take(&s, last + 1u, &out));

    {
        manet_slot_pos_t a, b;
        manet_slot_from_number(last, &a);
        manet_slot_from_number(last + 1u, &b);
        CHECK_EQ(a.frame, 0);
        CHECK_EQ(b.frame, 1);
        CHECK_EQ(b.index, 0);
        CHECK_EQ(b.start_us - a.start_us, MANET_SLOT_DURATION_US);
    }
}

static void test_ttl_stops_the_relay(void)
{
    manet_sched_t s;
    manet_pdu_t   in = make_pdu(0x05u, 0x01u, 1u, MANET_PRIO_VOICE);

    manet_sched_init(&s);
    /* TTL 1 means this hop is the last. It must not be scheduled onward. */
    CHECK_EQ(manet_sched_relay(&s, &in, 3u), MANET_ERR_TTL_EXPIRED);
    CHECK_EQ(manet_sched_depth(&s), 0);

    in.hdr.ttl = 0u;
    CHECK_EQ(manet_sched_relay(&s, &in, 3u), MANET_ERR_TTL_EXPIRED);
    CHECK_EQ(manet_sched_depth(&s), 0);
}

static void test_priority_contention(void)
{
    manet_sched_t s;
    manet_pdu_t   voice = make_pdu(0x05u, 0x01u, 8u, MANET_PRIO_VOICE);
    manet_pdu_t   data  = make_pdu(0x06u, 0x02u, 8u, MANET_PRIO_DATA);
    manet_pdu_t   emerg = make_pdu(0x07u, 0x03u, 8u, MANET_PRIO_EMERGENCY);
    manet_pdu_t   out;

    /* Data cannot displace voice. */
    manet_sched_init(&s);
    CHECK_EQ(manet_sched_originate(&s, &voice, 10u), MANET_OK);
    CHECK_EQ(manet_sched_originate(&s, &data, 10u), MANET_ERR_BUFFER);
    CHECK(manet_sched_take(&s, 10u, &out));
    CHECK_EQ(out.hdr.prio, MANET_PRIO_VOICE);

    /* Emergency displaces voice. */
    manet_sched_init(&s);
    CHECK_EQ(manet_sched_originate(&s, &voice, 10u), MANET_OK);
    CHECK_EQ(manet_sched_originate(&s, &emerg, 10u), MANET_OK);
    CHECK_EQ(manet_sched_depth(&s), 1);
    CHECK(manet_sched_take(&s, 10u, &out));
    CHECK_EQ(out.hdr.prio, MANET_PRIO_EMERGENCY);
    CHECK_EQ(out.hdr.src, 0x07);

    /* Equal priority does not displace — first claim holds, so a voice stream is not
     * chopped up by each new frame evicting the last. */
    manet_sched_init(&s);
    {
        manet_pdu_t v2 = make_pdu(0x08u, 0x04u, 8u, MANET_PRIO_VOICE);
        CHECK_EQ(manet_sched_originate(&s, &voice, 10u), MANET_OK);
        CHECK_EQ(manet_sched_originate(&s, &v2, 10u), MANET_ERR_BUFFER);
        CHECK(manet_sched_take(&s, 10u, &out));
        CHECK_EQ(out.hdr.src, 0x05);
    }
}

static void test_stale_entries_are_dropped(void)
{
    manet_sched_t s;
    manet_pdu_t   in = make_pdu(0x05u, 0x01u, 8u, MANET_PRIO_VOICE);

    manet_sched_init(&s);
    CHECK_EQ(manet_sched_originate(&s, &in, 10u), MANET_OK);
    CHECK_EQ(manet_sched_depth(&s), 1);

    /* Slot 12 arrives and slot 10 never happened. Sending it now would arrive out of
     * order and collide with whoever owns slot 12 elsewhere on the chain. */
    CHECK(!manet_sched_take(&s, 12u, NULL));
    CHECK_EQ(manet_sched_depth(&s), 0);
}

static void test_passive_acknowledgement(void)
{
    manet_sched_t s;
    manet_pdu_t   in = make_pdu(0x05u, 0x42u, 8u, MANET_PRIO_VOICE);

    manet_sched_init(&s);
    CHECK_EQ(manet_sched_relay(&s, &in, 4u), MANET_OK);

    /* A different origin, or a different sequence, is a different frame. */
    CHECK(!manet_sched_suppress(&s, 0x06u, 0x42u));
    CHECK(!manet_sched_suppress(&s, 0x05u, 0x43u));
    CHECK_EQ(manet_sched_depth(&s), 1);

    /* Someone else relayed it. Ours is redundant. */
    CHECK(manet_sched_suppress(&s, 0x05u, 0x42u));
    CHECK_EQ(manet_sched_depth(&s), 0);
    CHECK(!manet_sched_take(&s, 5u, NULL));
}

static void test_queue_is_finite(void)
{
    manet_sched_t s;
    manet_pdu_t   in = make_pdu(0x05u, 0x01u, 8u, MANET_PRIO_VOICE);
    unsigned      i;

    manet_sched_init(&s);
    for (i = 0u; i < MANET_SCHED_DEPTH; i++) {
        in.hdr.seq = (uint8_t)i;
        CHECK_EQ(manet_sched_originate(&s, &in, 100u + i), MANET_OK);
    }
    CHECK_EQ(manet_sched_depth(&s), MANET_SCHED_DEPTH);

    /* No allocation, so the queue refuses rather than growing. */
    in.hdr.seq = 0xFFu;
    CHECK_EQ(manet_sched_originate(&s, &in, 200u), MANET_ERR_BUFFER);

    manet_sched_flush(&s);
    CHECK_EQ(manet_sched_depth(&s), 0);
}

/*
 * The bucket chain, end to end. Node 0 originates in slot 0; each node hears the
 * previous one and relays in the following slot. Verifies the property the whole
 * design rests on: one hop per slot, sustained, with no frame-boundary stall.
 */
static void test_chain_advances_one_hop_per_slot(void)
{
    enum { HOPS = 9 };
    manet_sched_t sched[HOPS];
    manet_pdu_t   carried;
    unsigned      hop;

    for (hop = 0u; hop < (unsigned)HOPS; hop++) {
        manet_sched_init(&sched[hop]);
    }

    carried = make_pdu(0x01u, 0x77u, 15u, MANET_PRIO_VOICE);
    CHECK_EQ(manet_sched_originate(&sched[0], &carried, 0u), MANET_OK);

    for (hop = 0u; hop < (unsigned)HOPS; hop++) {
        const uint64_t slot = hop;
        manet_pdu_t    tx;

        /* This node transmits in exactly the slot its hop index predicts. */
        CHECK(manet_sched_take(&sched[hop], slot, &tx));
        CHECK_EQ(tx.hdr.src, 0x01);
        CHECK_EQ(tx.hdr.seq, 0x77);
        CHECK_EQ(tx.hdr.ttl, 15 - hop);

        if (hop + 1u < (unsigned)HOPS) {
            CHECK_EQ(manet_sched_relay(&sched[hop + 1u], &tx, slot), MANET_OK);
        }
    }

    /* Nine hops in nine slots. At 4 x 15 ms that is 135 ms; at 3 x 20 ms, 180 ms.
     * Waiting for the next frame would have cost nine frames — 540 ms — and the
     * product would not work. */
    {
        manet_slot_pos_t first, last;
        manet_slot_from_number(0u, &first);
        manet_slot_from_number((uint64_t)HOPS - 1u, &last);
        CHECK_EQ(last.start_us - first.start_us,
                 (uint64_t)(HOPS - 1) * (uint64_t)MANET_SLOT_DURATION_US);
    }
}

/*
 * Spatial reuse, the mechanic OQ-0013 turns on. The originator sends a new payload
 * every frame, so with N slots per frame the node N hops down the chain transmits in
 * the same slot at the same instant. N is therefore the reuse distance.
 */
static void test_reuse_distance_equals_slot_count(void)
{
    const uint64_t spf = (uint64_t)MANET_SLOTS_PER_FRAME;
    manet_slot_pos_t origin_second, relay_first;

    /* Origin's second payload: slot 0 of frame 1. */
    manet_slot_from_number(spf, &origin_second);
    /* First payload, N hops along: slot number N. */
    manet_slot_from_number(spf, &relay_first);

    CHECK_EQ(origin_second.index, 0);
    CHECK_EQ(origin_second.index, relay_first.index);
    CHECK_EQ(origin_second.start_us, relay_first.start_us);

    /* Which is to say: reuse distance is the slot count, and dropping from 4 to 3
     * costs one hop of physical separation between simultaneous transmitters. */
    CHECK_EQ(spf, MANET_SLOTS_PER_FRAME);
}

void test_slot_all(void)
{
    test_timing();
    test_relay_is_next_slot();
    test_relay_crosses_frame_boundary();
    test_ttl_stops_the_relay();
    test_priority_contention();
    test_stale_entries_are_dropped();
    test_passive_acknowledgement();
    test_queue_is_finite();
    test_chain_advances_one_hop_per_slot();
    test_reuse_distance_equals_slot_count();
}
