#include <string.h>

#include "manet/slot.h"
#include "test.h"

static manet_pdu_t make_pdu(manet_addr_t src, uint8_t seq, uint8_t ttl, uint8_t prio)
{
    manet_pdu_t p;
    memset(&p, 0, sizeof p);
    p.hdr.src  = src;
    p.hdr.prev = src;
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
    CHECK_EQ(manet_sched_relay(&s, &in, 7u, 0x7Fu), MANET_OK);
    CHECK_EQ(manet_sched_depth(&s), 1);
    CHECK(!manet_sched_take(&s, 7u, &out));
    CHECK(manet_sched_take(&s, 8u, &out));
    CHECK_EQ(out.hdr.ttl, 7);      /* decremented in transit */
    CHECK_EQ(out.hdr.src, 0x05);   /* origin preserved, not rewritten to the relay */
    CHECK_EQ(out.hdr.prev, 0x7F);  /* previous hop IS rewritten — that is what relaying does */
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
        CHECK_EQ(manet_sched_relay(&s, &in, last, 0x7Fu), MANET_OK);
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
    CHECK_EQ(manet_sched_relay(&s, &in, 3u, 0x7Fu), MANET_ERR_TTL_EXPIRED);
    CHECK_EQ(manet_sched_depth(&s), 0);

    in.hdr.ttl = 0u;
    CHECK_EQ(manet_sched_relay(&s, &in, 3u, 0x7Fu), MANET_ERR_TTL_EXPIRED);
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
    CHECK_EQ(manet_sched_relay(&s, &in, 4u, 0x7Fu), MANET_OK);

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
 * The bucket chain, end to end. Node 0 originates in slot 0; each node hears the previous
 * one and relays in the following slot. Verifies the property the whole design rests on:
 * one hop per slot, sustained, with no frame-boundary stall.
 *
 * One hop per VOICE slot. Since B-15 a reserved signalling slot recurs every
 * MANET_SIGNAL_SLOT_PERIOD, and voice steps over it rather than transmitting in it, so the
 * chain skips those slots and arrives that many slots later. The property being defended
 * has not changed — the chain never waits for a frame boundary, and the delay is bounded
 * and known in advance rather than being a stall. What is NOT acceptable, and what this
 * test would still catch, is the frame losing its place entirely: before the step-over the
 * payload landing in a reserved slot was dropped, which cost a whole hop, not a slot.
 */
static void test_chain_advances_one_hop_per_slot(void)
{
    enum { HOPS = 9 };
    manet_sched_t sched[HOPS];
    manet_pdu_t   carried;
    unsigned      hop;
    uint64_t      slot    = manet_slot_next_voice(0u);
    uint64_t      first   = slot;
    unsigned      skipped = 0u;

    for (hop = 0u; hop < (unsigned)HOPS; hop++) {
        manet_sched_init(&sched[hop]);
    }

    carried = make_pdu(0x01u, 0x77u, 15u, MANET_PRIO_VOICE);
    CHECK_EQ(manet_sched_originate(&sched[0], &carried, 0u), MANET_OK);

    for (hop = 0u; hop < (unsigned)HOPS; hop++) {
        manet_pdu_t tx;

        /* This node transmits in exactly the slot its hop index predicts, counting only
         * slots voice is allowed to use. */
        CHECK(!manet_slot_is_control(slot));
        CHECK(manet_sched_take(&sched[hop], slot, &tx));
        CHECK_EQ(tx.hdr.src, 0x01);       /* origin, all the way down the chain */
        CHECK_EQ(tx.hdr.seq, 0x77);
        CHECK_EQ(tx.hdr.ttl, 15 - hop);
        /* ...while the previous hop advances with the frame. The forwarding rule tests
         * this field, not the origin. */
        CHECK_EQ(tx.hdr.prev, hop == 0u ? 0x01u : hop + 1u);

        if (hop + 1u < (unsigned)HOPS) {
            CHECK_EQ(manet_sched_relay(&sched[hop + 1u], &tx, slot, (manet_addr_t)(hop + 2u)),
                     MANET_OK);
            if (manet_slot_is_control(slot + 1u)) {
                skipped++;
            }
            slot = manet_slot_next_voice(slot + 1u);
        }
    }

    /* Nine hops in nine voice slots, plus the reserved slots stepped over. At one slot in
     * four that is two of them across nine hops. Waiting for the next frame would have cost
     * nine frames instead, and the product would not work. */
    CHECK_EQ(slot - first, (uint64_t)(HOPS - 1) + (uint64_t)skipped);
    {
        manet_slot_pos_t a, b;
        manet_slot_from_number(first, &a);
        manet_slot_from_number(slot, &b);
        CHECK_EQ(b.start_us - a.start_us,
                 ((uint64_t)(HOPS - 1) + (uint64_t)skipped)
                     * (uint64_t)MANET_SLOT_DURATION_US);
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

/*
 * Origination phase. The property under test is NOT that every address gets a distinct
 * phase — with four phases and twelve radios that is arithmetically impossible, and
 * backlog B-04's original done-test asked for it anyway. It is that concurrent talkers
 * can always be separated up to the structural limit.
 */
static void test_voice_phase_is_in_range_and_stable(void)
{
    unsigned a;
    for (a = 0u; a < 256u; a++) {
        uint8_t p = manet_voice_phase((manet_addr_t)a);
        CHECK(p < (uint8_t)MANET_SLOTS_PER_FRAME);
        CHECK_EQ(p, manet_voice_phase((manet_addr_t)a));   /* pure */
    }
}

static void test_voice_phase_spreads_consecutive_addresses(void)
{
    /* A group issued sequential serial numbers must not land on sequential phases.
     * Counts every phase over the address space and requires no phase to take more
     * than 40% of it — a plain multiply-and-shift fails this, which is why the
     * implementation avalanches. */
    size_t count[MANET_SLOTS_PER_FRAME];
    unsigned a, i;
    for (i = 0u; i < (unsigned)MANET_SLOTS_PER_FRAME; i++) {
        count[i] = 0u;
    }
    for (a = 1u; a <= 255u; a++) {
        count[manet_voice_phase((manet_addr_t)a)]++;
    }
    for (i = 0u; i < (unsigned)MANET_SLOTS_PER_FRAME; i++) {
        CHECK(count[i] > 0u);
        CHECK(count[i] * 10u < 255u * 4u);
    }
}

static void test_voice_phase_avoids_occupied(void)
{
    manet_addr_t a = (manet_addr_t)0x42;
    uint8_t base = manet_voice_phase(a);
    uint32_t occupied = (uint32_t)1u << base;
    uint8_t moved = manet_voice_phase_avoiding(a, occupied);

    CHECK_EQ(manet_voice_phase_avoiding(a, 0u), base);   /* free -> keep default */
    CHECK(moved != base);                                  /* taken -> move */
    CHECK((occupied & ((uint32_t)1u << moved)) == 0u);
    CHECK(moved < (uint8_t)MANET_SLOTS_PER_FRAME);
}

static void test_voice_phase_is_deterministic_across_radios(void)
{
    /* Two radios resolving the same collision from the same information must reach
     * the same answer, or they move onto each other. No signalling involved. */
    manet_addr_t a = (manet_addr_t)0x7E;
    uint32_t occ;
    for (occ = 0u; occ < ((uint32_t)1u << MANET_SLOTS_PER_FRAME); occ++) {
        CHECK_EQ(manet_voice_phase_avoiding(a, occ),
                 manet_voice_phase_avoiding(a, occ));
    }
}

static void test_voice_phase_reports_saturation(void)
{
    uint32_t all = ((uint32_t)1u << MANET_SLOTS_PER_FRAME) - 1u;
    unsigned i;

    CHECK(manet_voice_phase_free(0u));
    CHECK(!manet_voice_phase_free(all));
    for (i = 0u; i < (unsigned)MANET_SLOTS_PER_FRAME; i++) {
        CHECK(manet_voice_phase_free(all & ~((uint32_t)1u << i)));
    }
    /* Saturated: nowhere to move, so the default comes back rather than a lie. */
    CHECK_EQ(manet_voice_phase_avoiding((manet_addr_t)9, all),
             manet_voice_phase((manet_addr_t)9));
}

static void test_every_phase_is_reachable_by_avoidance(void)
{
    /* The real requirement: whatever a radio's default, it can be steered to any free
     * phase. This is what makes MANET_SLOTS_PER_FRAME concurrent talkers possible at
     * all, and it is the property B-04 should have asked for. */
    unsigned target;
    for (target = 0u; target < (unsigned)MANET_SLOTS_PER_FRAME; target++) {
        uint32_t occupied = (((uint32_t)1u << MANET_SLOTS_PER_FRAME) - 1u) & ~((uint32_t)1u << target);
        unsigned a;
        for (a = 1u; a <= 64u; a++) {
            CHECK_EQ(manet_voice_phase_avoiding((manet_addr_t)a, occupied),
                     (uint8_t)target);
        }
    }
}

void test_slot_all(void)
{
    test_voice_phase_is_in_range_and_stable();
    test_voice_phase_spreads_consecutive_addresses();
    test_voice_phase_avoids_occupied();
    test_voice_phase_is_deterministic_across_radios();
    test_voice_phase_reports_saturation();
    test_every_phase_is_reachable_by_avoidance();
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
