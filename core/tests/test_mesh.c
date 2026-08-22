#include <string.h>

#include "manet/mpr.h"
#include "manet/slot.h"
#include "manet/neighbour.h"
#include "test.h"

#define MAXN 16

static manet_addr_t A(unsigned i) { return (manet_addr_t)(i + 1u); }

/*
 * Build node `self`'s view of a symmetric topology, the way it would actually arrive:
 * first by hearing each neighbour, then by receiving each neighbour's beacon listing
 * who *they* hear. The two-hop view is inferred, never handed over directly.
 */
static void build(manet_nb_table_t *t, unsigned self, unsigned n,
                  const bool adj[MAXN][MAXN], uint64_t slot, bool reverse)
{
    unsigned pass;
    manet_nb_init(t, A(self));

    for (pass = 0u; pass < 2u; pass++) {
        unsigned x;
        for (x = 0u; x < n; x++) {
            const unsigned j = reverse ? (n - 1u - x) : x;
            manet_advert_t adv[MAXN];
            size_t         m = 0u;
            unsigned       k;

            if (j == self || !adj[self][j]) {
                continue;
            }
            if (pass == 0u) {
                (void)manet_nb_heard(t, A(j), 200u, slot);
                continue;
            }
            for (k = 0u; k < n; k++) {
                if (k == j || !adj[j][k]) {
                    continue;
                }
                adv[m].addr = A(k);
                adv[m].code = MANET_ADV_SYM;
                m++;
            }
            (void)manet_nb_advert(t, A(j), adv, m, slot);
        }
    }
}

static void link_pair(bool adj[MAXN][MAXN], unsigned a, unsigned b)
{
    adj[a][b] = true;
    adj[b][a] = true;
}

static void clear_adj(bool adj[MAXN][MAXN])
{
    memset(adj, 0, sizeof(bool) * MAXN * MAXN);
}

/*
 * The defining behaviour of the product. Twelve leaders standing together, every radio
 * in range of every other: nobody is two hops away, so nothing relays, and the system
 * behaves exactly like conventional simplex with no latency or capacity penalty.
 */
static void test_cluster_relays_nothing(void)
{
    bool            adj[MAXN][MAXN];
    manet_nb_table_t t;
    manet_mpr_set_t  mpr;
    unsigned         i, j;
    const unsigned   N = 12u;

    clear_adj(adj);
    for (i = 0u; i < N; i++) {
        for (j = 0u; j < N; j++) {
            if (i != j) {
                link_pair(adj, i, j);
            }
        }
    }

    for (i = 0u; i < N; i++) {
        build(&t, i, N, adj, 100u, false);

        CHECK_EQ(manet_nb_symmetric(&t, NULL, 0), N - 1u);
        CHECK_EQ(manet_nb_two_hop(&t, NULL, 0), 0);

        manet_mpr_select(&t, &mpr);
        CHECK_EQ(mpr.count, 0);
        CHECK(manet_mpr_covers_all(&t, &mpr));
    }
}

/* A single leader with nobody in range works as a plain simplex radio. */
static void test_alone(void)
{
    bool             adj[MAXN][MAXN];
    manet_nb_table_t t;
    manet_mpr_set_t  mpr;

    clear_adj(adj);
    build(&t, 0u, 4u, adj, 100u, false);
    CHECK_EQ(manet_nb_count(&t), 0);
    CHECK_EQ(manet_nb_two_hop(&t, NULL, 0), 0);
    manet_mpr_select(&t, &mpr);
    CHECK_EQ(mpr.count, 0);
}

/* Strung out along a path: the only neighbour is the only route onward. */
static void test_chain(void)
{
    bool             adj[MAXN][MAXN];
    manet_nb_table_t t;
    manet_mpr_set_t  mpr;
    manet_addr_t     two[MANET_MAX_TWO_HOP];

    clear_adj(adj);
    link_pair(adj, 0u, 1u);
    link_pair(adj, 1u, 2u);
    link_pair(adj, 2u, 3u);
    link_pair(adj, 3u, 4u);

    build(&t, 0u, 5u, adj, 100u, false);
    CHECK_EQ(manet_nb_symmetric(&t, NULL, 0), 1);
    CHECK_EQ(manet_nb_two_hop(&t, two, MANET_MAX_TWO_HOP), 1);
    CHECK_EQ(two[0], A(2u));

    manet_mpr_select(&t, &mpr);
    CHECK_EQ(mpr.count, 1);
    CHECK(manet_mpr_contains(&mpr, A(1u)));

    /* The node in the middle of the chain has two-hop neighbours on both sides and must
     * select both of its neighbours — it is the sole route in each direction. */
    build(&t, 2u, 5u, adj, 100u, false);
    CHECK_EQ(manet_nb_two_hop(&t, NULL, 0), 2);
    manet_mpr_select(&t, &mpr);
    CHECK_EQ(mpr.count, 2);
    CHECK(manet_mpr_contains(&mpr, A(1u)));
    CHECK(manet_mpr_contains(&mpr, A(3u)));
}

/* Splinter group beyond direct range: route through whoever is between. */
static void test_star(void)
{
    bool             adj[MAXN][MAXN];
    manet_nb_table_t t;
    manet_mpr_set_t  mpr;
    unsigned         i;
    const unsigned   N = 6u;

    clear_adj(adj);
    for (i = 1u; i < N; i++) {
        link_pair(adj, 0u, i); /* node 0 is the one everyone can see */
    }

    build(&t, 1u, N, adj, 100u, false);
    CHECK_EQ(manet_nb_symmetric(&t, NULL, 0), 1);
    CHECK_EQ(manet_nb_two_hop(&t, NULL, 0), N - 2u);

    manet_mpr_select(&t, &mpr);
    CHECK_EQ(mpr.count, 1);
    CHECK(manet_mpr_contains(&mpr, A(0u)));

    /* The hub itself sees everyone directly, so it relays for nobody. */
    build(&t, 0u, N, adj, 100u, false);
    CHECK_EQ(manet_nb_two_hop(&t, NULL, 0), 0);
    manet_mpr_select(&t, &mpr);
    CHECK_EQ(mpr.count, 0);
}

/*
 * Two neighbours both reach the same distant node. Only one should be picked — every
 * extra relay is a retransmission on a channel with nothing to spare.
 */
static void test_minimality(void)
{
    bool             adj[MAXN][MAXN];
    manet_nb_table_t t;
    manet_mpr_set_t  mpr;

    clear_adj(adj);
    link_pair(adj, 0u, 1u);
    link_pair(adj, 0u, 2u);
    link_pair(adj, 1u, 3u);
    link_pair(adj, 2u, 3u);

    build(&t, 0u, 4u, adj, 100u, false);
    CHECK_EQ(manet_nb_symmetric(&t, NULL, 0), 2);
    CHECK_EQ(manet_nb_two_hop(&t, NULL, 0), 1);

    manet_mpr_select(&t, &mpr);
    CHECK_EQ(mpr.count, 1);
    CHECK(manet_mpr_covers_all(&t, &mpr));
}

/*
 * One neighbour is the sole route to someone, and happens to cover everything the other
 * covers too. The redundant one must be dropped.
 */
static void test_redundant_dropped(void)
{
    bool             adj[MAXN][MAXN];
    manet_nb_table_t t;
    manet_mpr_set_t  mpr;

    clear_adj(adj);
    link_pair(adj, 0u, 1u);  /* self -- A */
    link_pair(adj, 0u, 2u);  /* self -- B */
    link_pair(adj, 1u, 3u);  /* A -- X    */
    link_pair(adj, 2u, 3u);  /* B -- X    */
    link_pair(adj, 2u, 4u);  /* B -- Y, and only B reaches Y */

    build(&t, 0u, 5u, adj, 100u, false);
    CHECK_EQ(manet_nb_two_hop(&t, NULL, 0), 2);

    manet_mpr_select(&t, &mpr);
    CHECK_EQ(mpr.count, 1);
    CHECK(manet_mpr_contains(&mpr, A(2u)));
    CHECK(manet_mpr_covers_all(&t, &mpr));
}

/*
 * A one-way link is worse than no link: relaying through a radio that cannot hear you
 * fails silently and presents as a coverage problem. It must never be selected.
 */
static void test_asymmetric_is_unusable(void)
{
    manet_nb_table_t t;
    manet_mpr_set_t  mpr;
    manet_advert_t   adv[2];

    manet_nb_init(&t, A(0u));

    /* We hear node 1, but node 1 has never named us. */
    CHECK_EQ(manet_nb_heard(&t, A(1u), 200u, 100u), MANET_OK);
    CHECK_EQ(manet_nb_link(&t, A(1u)), MANET_LINK_ASYMMETRIC);

    /* Node 1's beacon lists node 2 but not us — so it still cannot hear us. */
    adv[0].addr = A(2u);
    adv[0].code = MANET_ADV_SYM;
    CHECK_EQ(manet_nb_advert(&t, A(1u), adv, 1u, 100u), MANET_OK);
    CHECK_EQ(manet_nb_link(&t, A(1u)), MANET_LINK_ASYMMETRIC);

    CHECK_EQ(manet_nb_symmetric(&t, NULL, 0), 0);
    CHECK_EQ(manet_nb_two_hop(&t, NULL, 0), 0);
    manet_mpr_select(&t, &mpr);
    CHECK_EQ(mpr.count, 0);

    /* Now node 1 names us. Any code proves it hears us, and the link becomes usable. */
    adv[0].addr = A(2u);
    adv[0].code = MANET_ADV_SYM;
    adv[1].addr = A(0u);
    adv[1].code = MANET_ADV_ASYM;
    CHECK_EQ(manet_nb_advert(&t, A(1u), adv, 2u, 100u), MANET_OK);
    CHECK_EQ(manet_nb_link(&t, A(1u)), MANET_LINK_SYMMETRIC);
    CHECK_EQ(manet_nb_two_hop(&t, NULL, 0), 1);
    manet_mpr_select(&t, &mpr);
    CHECK_EQ(mpr.count, 1);
}

/* Same neighbourhood, opposite arrival order, identical result. ADR-0006 requires a
 * failure seen in simulation to reproduce exactly on hardware. */
static void test_selection_is_deterministic(void)
{
    bool             adj[MAXN][MAXN];
    manet_nb_table_t fwd, rev;
    manet_mpr_set_t  a, b;
    size_t           i;

    clear_adj(adj);
    link_pair(adj, 0u, 1u);
    link_pair(adj, 0u, 2u);
    link_pair(adj, 0u, 3u);
    link_pair(adj, 1u, 4u);
    link_pair(adj, 2u, 4u);
    link_pair(adj, 2u, 5u);
    link_pair(adj, 3u, 5u);
    link_pair(adj, 3u, 6u);

    build(&fwd, 0u, 7u, adj, 100u, false);
    build(&rev, 0u, 7u, adj, 100u, true);

    manet_mpr_select(&fwd, &a);
    manet_mpr_select(&rev, &b);

    CHECK_EQ(a.count, b.count);
    CHECK(a.count > 0);
    for (i = 0u; i < a.count && i < b.count; i++) {
        CHECK_EQ(a.addr[i], b.addr[i]);
    }
    CHECK(manet_mpr_covers_all(&fwd, &a));
}

/* Someone walks out of range. The link ages out and the topology reconverges without
 * anyone touching anything. */
static void test_expiry(void)
{
    bool             adj[MAXN][MAXN];
    manet_nb_table_t t;
    manet_mpr_set_t  mpr;
    const uint64_t   heard_at = 1000u;

    clear_adj(adj);
    link_pair(adj, 0u, 1u);
    link_pair(adj, 1u, 2u);

    build(&t, 0u, 3u, adj, heard_at, false);
    CHECK_EQ(manet_nb_count(&t), 1);
    manet_mpr_select(&t, &mpr);
    CHECK_EQ(mpr.count, 1);

    /* Still inside the holding time. */
    CHECK_EQ(manet_nb_expire(&t, heard_at + MANET_NB_HOLD_SLOTS), 0);
    CHECK_EQ(manet_nb_count(&t), 1);

    /* Beyond it. */
    CHECK_EQ(manet_nb_expire(&t, heard_at + MANET_NB_HOLD_SLOTS + 1u), 1);
    CHECK_EQ(manet_nb_count(&t), 0);
    manet_mpr_select(&t, &mpr);
    CHECK_EQ(mpr.count, 0);
}

/* The relay gate: a radio relays for a neighbour only if that neighbour both hears it
 * and has asked it to. */
static void test_relay_gate(void)
{
    manet_nb_table_t t;
    manet_advert_t   adv[1];

    manet_nb_init(&t, A(0u));
    CHECK_EQ(manet_nb_heard(&t, A(1u), 200u, 10u), MANET_OK);
    CHECK(!manet_nb_should_relay_for(&t, A(1u)));

    /* Symmetric, but not selected. */
    adv[0].addr = A(0u);
    adv[0].code = MANET_ADV_SYM;
    CHECK_EQ(manet_nb_advert(&t, A(1u), adv, 1u, 10u), MANET_OK);
    CHECK(!manet_nb_should_relay_for(&t, A(1u)));

    /* Selected. */
    adv[0].code = MANET_ADV_MPR;
    CHECK_EQ(manet_nb_advert(&t, A(1u), adv, 1u, 10u), MANET_OK);
    CHECK(manet_nb_should_relay_for(&t, A(1u)));

    /* Deselected in a later beacon — the flag must not stick. */
    adv[0].code = MANET_ADV_SYM;
    CHECK_EQ(manet_nb_advert(&t, A(1u), adv, 1u, 10u), MANET_OK);
    CHECK(!manet_nb_should_relay_for(&t, A(1u)));

    /* Never relay for a stranger. */
    CHECK(!manet_nb_should_relay_for(&t, A(9u)));
}

/* What this radio puts in its own beacon. */
static void test_beacon_contents(void)
{
    bool             adj[MAXN][MAXN];
    manet_nb_table_t t;
    manet_mpr_set_t  mpr;
    manet_advert_t   out[MANET_MAX_NEIGHBOURS];
    size_t           n;
    size_t           i;
    size_t           mprs = 0u;

    clear_adj(adj);
    link_pair(adj, 0u, 1u);
    link_pair(adj, 0u, 2u);
    link_pair(adj, 1u, 3u);

    build(&t, 0u, 4u, adj, 100u, false);
    manet_mpr_select(&t, &mpr);
    CHECK_EQ(mpr.count, 1);

    n = manet_nb_beacon(&t, mpr.addr, mpr.count, out, MANET_MAX_NEIGHBOURS);
    CHECK_EQ(n, 2);
    for (i = 0u; i < n; i++) {
        CHECK(out[i].code != MANET_ADV_ASYM);
        if (out[i].code == MANET_ADV_MPR) {
            mprs++;
            CHECK(manet_mpr_contains(&mpr, out[i].addr));
        }
    }
    CHECK_EQ(mprs, 1);

    /* A beacon for a typical group has to fit in one slot — config.h asserts it, and
     * this is the runtime counterpart. */
    CHECK(MANET_BEACON_BITS <= MANET_MAX_PDU_BITS);
}

static void test_table_is_finite(void)
{
    manet_nb_table_t t;
    unsigned         i;

    manet_nb_init(&t, A(0u));
    for (i = 1u; i <= MANET_MAX_NEIGHBOURS; i++) {
        CHECK_EQ(manet_nb_heard(&t, (manet_addr_t)(i + 1u), 128u, 5u), MANET_OK);
    }
    CHECK_EQ(manet_nb_count(&t), MANET_MAX_NEIGHBOURS);

    /* No allocation, so it refuses rather than growing. */
    CHECK_EQ(manet_nb_heard(&t, (manet_addr_t)0x90u, 128u, 5u), MANET_ERR_BUFFER);

    /* A radio is not its own neighbour, and a group address cannot originate. */
    CHECK_EQ(manet_nb_heard(&t, A(0u), 128u, 5u), MANET_ERR_BAD_SOURCE);
    CHECK_EQ(manet_nb_heard(&t, 0xC5u, 128u, 5u), MANET_ERR_BAD_SOURCE);
    CHECK_EQ(manet_nb_heard(&t, 0xFFu, 128u, 5u), MANET_ERR_BAD_SOURCE);
}

/*
 * Neighbour table eviction. The table was first-come-first-served with no eviction: once
 * full, a radio could never learn a new neighbour however useful, and however stale what
 * it was already holding. Harmless in a flat cluster, but across 48 radios in twelve
 * groups the table filled with whoever spoke first and worst-case delivery fell to 68.2%.
 */
static void fill_table(manet_nb_table_t *t, uint8_t quality, uint64_t slot)
{
    unsigned i;
    for (i = 0u; i < (unsigned)MANET_MAX_NEIGHBOURS; i++) {
        CHECK_EQ(manet_nb_heard(t, (manet_addr_t)(i + 1u), quality, slot), MANET_OK);
    }
    CHECK_EQ(manet_nb_count(t), (size_t)MANET_MAX_NEIGHBOURS);
}

static void test_full_table_refuses_a_worse_newcomer(void)
{
    /* Clause 4, and it matters as much as the others: if the newcomer is not better, the
     * right answer is no. Otherwise two similar neighbours displace each other forever. */
    manet_nb_table_t t;
    manet_nb_init(&t, 200u);
    fill_table(&t, 200u, 100u);

    CHECK_EQ(manet_nb_heard(&t, 99u, 10u, 101u), MANET_ERR_BUFFER);
    CHECK(manet_nb_get(&t, 99u) == NULL);
    CHECK_EQ(manet_nb_count(&t), (size_t)MANET_MAX_NEIGHBOURS);
}

static void test_expired_entry_is_reclaimed_first(void)
{
    manet_nb_table_t t;
    manet_nb_init(&t, 200u);
    fill_table(&t, 200u, 100u);

    /* Far enough past the hold time that entry 1 is certainly gone. Everything else is
     * refreshed, so entry 1 is the only expired one. */
    {
        uint64_t later = 100u + MANET_NB_HOLD_SLOTS + 10u;
        unsigned i;
        for (i = 1u; i < (unsigned)MANET_MAX_NEIGHBOURS; i++) {
            CHECK_EQ(manet_nb_heard(&t, (manet_addr_t)(i + 1u), 200u, later), MANET_OK);
        }
        /* A weak newcomer still gets in, because it is replacing something that is gone
         * rather than competing with something that is present. */
        CHECK_EQ(manet_nb_heard(&t, 99u, 5u, later), MANET_OK);
        CHECK(manet_nb_get(&t, 99u) != NULL);
        CHECK(manet_nb_get(&t, 1u) == NULL);
        CHECK_EQ(manet_nb_count(&t), (size_t)MANET_MAX_NEIGHBOURS);
    }
}

static void test_clearly_stronger_newcomer_displaces_the_weakest(void)
{
    manet_nb_table_t t;
    unsigned i;
    manet_nb_init(&t, 200u);
    for (i = 0u; i < (unsigned)MANET_MAX_NEIGHBOURS; i++) {
        CHECK_EQ(manet_nb_heard(&t, (manet_addr_t)(i + 1u),
                                (uint8_t)(100u + i), 100u), MANET_OK);
    }
    /* Address 1 holds quality 100, the weakest. A newcomer well clear of the margin
     * should take its place; the rest of the table is untouched. */
    CHECK_EQ(manet_nb_heard(&t, 99u, 255u, 101u), MANET_OK);
    CHECK(manet_nb_get(&t, 99u) != NULL);
    CHECK(manet_nb_get(&t, 1u) == NULL);
    CHECK(manet_nb_get(&t, 2u) != NULL);
    CHECK_EQ(manet_nb_count(&t), (size_t)MANET_MAX_NEIGHBOURS);
}

static void test_margin_prevents_thrashing(void)
{
    /* Better, but not by the margin. Refused -- otherwise two radios a hair apart in
     * quality evict each other for as long as both can be heard. */
    manet_nb_table_t t;
    unsigned i;
    manet_nb_init(&t, 200u);
    for (i = 0u; i < (unsigned)MANET_MAX_NEIGHBOURS; i++) {
        CHECK_EQ(manet_nb_heard(&t, (manet_addr_t)(i + 1u), 100u, 100u), MANET_OK);
    }
    CHECK_EQ(manet_nb_heard(&t, 99u, (uint8_t)(100u + MANET_NB_EVICT_MARGIN - 1u), 101u),
             MANET_ERR_BUFFER);
    CHECK(manet_nb_get(&t, 99u) == NULL);
}

static void test_a_radio_relying_on_us_is_not_dropped(void)
{
    /* Clause 3's exception. Dropping a neighbour that selected us as its multipoint relay
     * silently breaks its forwarding path, and it has no way to find out we did it. */
    manet_nb_table_t t;
    manet_advert_t   adv[1];
    unsigned i;

    manet_nb_init(&t, 200u);
    for (i = 0u; i < (unsigned)MANET_MAX_NEIGHBOURS; i++) {
        CHECK_EQ(manet_nb_heard(&t, (manet_addr_t)(i + 1u), 10u, 100u), MANET_OK);
    }
    /* Address 1 names us as its MPR: weakest in the table, and now load-bearing. */
    adv[0].addr = 200u;
    adv[0].code = MANET_ADV_MPR;
    CHECK_EQ(manet_nb_advert(&t, 1u, adv, 1u, 100u), MANET_OK);

    CHECK_EQ(manet_nb_heard(&t, 99u, 255u, 101u), MANET_OK);
    CHECK(manet_nb_get(&t, 1u) != NULL);   /* protected */

    /* But only while it is present. Once expired it is reclaimable like anything else. */
    CHECK_EQ(manet_nb_heard(&t, 98u, 255u, 100u + MANET_NB_HOLD_SLOTS + 10u), MANET_OK);
    CHECK(manet_nb_get(&t, 1u) == NULL);
}

void test_mesh_all(void)
{
    test_full_table_refuses_a_worse_newcomer();
    test_expired_entry_is_reclaimed_first();
    test_clearly_stronger_newcomer_displaces_the_weakest();
    test_margin_prevents_thrashing();
    test_a_radio_relying_on_us_is_not_dropped();
    test_cluster_relays_nothing();
    test_alone();
    test_chain();
    test_star();
    test_minimality();
    test_redundant_dropped();
    test_asymmetric_is_unusable();
    test_selection_is_deterministic();
    test_expiry();
    test_relay_gate();
    test_beacon_contents();
    test_table_is_finite();
}

/* ---------------------------------------------- coverage-based suppression -- */

/*
 * The failure the old rule allowed. Two radios each queue a relay; both hear a third
 * relay the same frame; under "cancel on any duplicate" both fall silent — and if that
 * third radio reaches neither of their far sides, the frame stops there and everyone
 * beyond is lost, with no error anywhere.
 */
void test_suppression_is_coverage_based(void)
{
    bool             adj[MAXN][MAXN];
    manet_nb_table_t t;
    manet_addr_t     heard[4];

    /* self(0) -- A(1), self -- B(2); A -- X(3); B -- Y(4). A and B each reach somewhere
     * the other does not. */
    clear_adj(adj);
    link_pair(adj, 0u, 1u);
    link_pair(adj, 0u, 2u);
    link_pair(adj, 1u, 3u);
    link_pair(adj, 2u, 4u);
    build(&t, 0u, 5u, adj, 100u, false);

    /* Nobody has relayed: obviously still needed. */
    CHECK(manet_mpr_still_needed(&t, NULL, 0u));

    /* A relayed. A reaches itself and X, but not B — so we still carry it to B. */
    heard[0] = A(1u);
    CHECK(manet_mpr_still_needed(&t, heard, 1u));

    /* A and B have both relayed. Between them they cover everything we reach, so our
     * transmission would add nothing. */
    heard[1] = A(2u);
    CHECK(!manet_mpr_still_needed(&t, heard, 2u));

    /* A radio that reaches nothing of ours does not cover us. Under the old
     * cancel-on-first-duplicate rule, hearing this alone would have silenced us. */
    {
        manet_addr_t stranger[1];
        stranger[0] = A(9u);
        CHECK(manet_mpr_still_needed(&t, stranger, 1u));
    }
}

/* The scheduler must remember WHO relayed, not merely how many did. */
void test_sched_remembers_relayers(void)
{
    manet_sched_t s;
    manet_pdu_t   pdu;
    manet_addr_t  out[MANET_HEARD_MAX];
    size_t        n;

    memset(&pdu, 0, sizeof pdu);
    pdu.hdr.src = A(4u);
    pdu.hdr.prev = A(4u);
    pdu.hdr.dst = 0xC0u;
    pdu.hdr.seq = 0x33u;
    pdu.hdr.ttl = 8u;
    pdu.hdr.prio = MANET_PRIO_VOICE;

    manet_sched_init(&s);
    CHECK_EQ(manet_sched_originate(&s, &pdu, 40u), MANET_OK);

    CHECK_EQ(manet_sched_note_relay(&s, A(4u), 0x33u, A(7u)), 1);
    CHECK_EQ(manet_sched_note_relay(&s, A(4u), 0x33u, A(8u)), 2);
    /* the same relayer twice is still one relayer */
    CHECK_EQ(manet_sched_note_relay(&s, A(4u), 0x33u, A(7u)), 2);
    /* a frame we do not hold records nothing */
    CHECK_EQ(manet_sched_note_relay(&s, A(4u), 0x99u, A(7u)), 0);

    n = manet_sched_heard(&s, A(4u), 0x33u, out, MANET_HEARD_MAX);
    CHECK_EQ(n, 2);
    CHECK(out[0] == A(7u) || out[1] == A(7u));
    CHECK(out[0] == A(8u) || out[1] == A(8u));

    /* Still queued — noting relayers does not cancel anything by itself. */
    CHECK_EQ(manet_sched_depth(&s), 1);
    CHECK(manet_sched_suppress(&s, A(4u), 0x33u));
    CHECK_EQ(manet_sched_depth(&s), 0);
}
