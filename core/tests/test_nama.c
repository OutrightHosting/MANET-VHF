#include <string.h>

#include "manet/nama.h"
#include "test.h"

#define MAXN 16

static manet_addr_t A(unsigned i) { return (manet_addr_t)(i + 1u); }

static void build(manet_nb_table_t *t, unsigned self, unsigned n,
                  const bool adj[MAXN][MAXN])
{
    unsigned pass, j, k;
    manet_nb_init(t, A(self));
    for (pass = 0u; pass < 2u; pass++) {
        for (j = 0u; j < n; j++) {
            manet_advert_t adv[MAXN];
            size_t         m = 0u;
            if (j == self || !adj[self][j]) {
                continue;
            }
            if (pass == 0u) {
                (void)manet_nb_heard(t, A(j), 200u, 100u);
                continue;
            }
            for (k = 0u; k < n; k++) {
                if (k != j && adj[j][k]) {
                    adv[m].addr = A(k);
                    adv[m].code = MANET_ADV_SYM;
                    m++;
                }
            }
            (void)manet_nb_advert(t, A(j), adv, m, 100u);
        }
    }
}

static void link_pair(bool adj[MAXN][MAXN], unsigned a, unsigned b)
{
    adj[a][b] = true;
    adj[b][a] = true;
}

/* Same inputs, same answer — on any machine. The whole scheme collapses otherwise. */
static void test_deterministic(void)
{
    unsigned i;
    for (i = 0u; i < 64u; i++) {
        const uint32_t a = manet_nama_priority((manet_addr_t)(i + 1u), 12345u);
        const uint32_t b = manet_nama_priority((manet_addr_t)(i + 1u), 12345u);
        CHECK_EQ(a, b);
    }
    /* Different context, different priority — otherwise one radio would win forever. */
    {
        unsigned differ = 0u;
        for (i = 0u; i < 64u; i++) {
            if (manet_nama_priority(A(3u), i) != manet_nama_priority(A(3u), i + 1u)) {
                differ++;
            }
        }
        CHECK(differ >= 60u);
    }
}

/* No two radios share a priority in the same context. Ties would let both transmit. */
static void test_priorities_are_unique(void)
{
    unsigned ctx, i, j;
    for (ctx = 0u; ctx < 200u; ctx++) {
        for (i = 0u; i < 40u; i++) {
            for (j = i + 1u; j < 40u; j++) {
                CHECK(manet_nama_priority(A(i), ctx) != manet_nama_priority(A(j), ctx));
            }
        }
    }
}

/*
 * THE property. Two radios within two hops of each other must never both win the same
 * context — that is the collision the scheme exists to make impossible, and the one a
 * hash-to-slot assignment cannot prevent.
 */
static void test_mutual_exclusion(void)
{
    bool             adj[MAXN][MAXN];
    manet_nb_table_t t[MAXN];
    unsigned         i, j, k, ctx;
    const unsigned   N = 12u;
    unsigned         both = 0u;
    unsigned         nobody = 0u;

    /* A realistic dispersed group: a chain with cross-links, so two-hop sets overlap. */
    memset(adj, 0, sizeof adj);
    for (i = 0u; i + 1u < N; i++) {
        link_pair(adj, i, i + 1u);
    }
    for (i = 0u; i + 2u < N; i += 3u) {
        link_pair(adj, i, i + 2u);
    }

    for (i = 0u; i < N; i++) {
        build(&t[i], i, N, adj);
    }

    for (ctx = 0u; ctx < 500u; ctx++) {
        unsigned winners = 0u;
        bool     won[MAXN];

        for (i = 0u; i < N; i++) {
            won[i] = manet_nama_wins(&t[i], ctx);
            if (won[i]) {
                winners++;
            }
        }
        if (winners == 0u) {
            nobody++;
        }

        /* Any two winners must be at least three hops apart. */
        for (i = 0u; i < N; i++) {
            if (!won[i]) {
                continue;
            }
            for (j = 0u; j < N; j++) {
                if (i == j || !won[j]) {
                    continue;
                }
                CHECK(!adj[i][j]);                 /* never one hop apart  */
                for (k = 0u; k < N; k++) {
                    if (adj[i][k] && adj[k][j]) {
                        both++;                    /* never two hops apart */
                    }
                }
            }
        }
    }
    CHECK_EQ(both, 0);

    /* Some context must always have a winner, or the channel goes unused. */
    CHECK(nobody < 250u);
}

/* Every radio gets a turn. A scheme that starves one is no better than a collision. */
static void test_fairness(void)
{
    bool             adj[MAXN][MAXN];
    manet_nb_table_t t[MAXN];
    unsigned         i, ctx;
    const unsigned   N = 12u;
    unsigned         wins[MAXN];

    memset(adj, 0, sizeof adj);
    for (i = 0u; i + 1u < N; i++) {
        link_pair(adj, i, i + 1u);
    }
    for (i = 0u; i < N; i++) {
        build(&t[i], i, N, adj);
        wins[i] = 0u;
    }
    for (ctx = 0u; ctx < 600u; ctx++) {
        for (i = 0u; i < N; i++) {
            if (manet_nama_wins(&t[i], ctx)) {
                wins[i]++;
            }
        }
    }
    for (i = 0u; i < N; i++) {
        /* In a chain the contention set is four or five, so a fair share is well over
         * a tenth of contexts. Nobody should be anywhere near starved. */
        CHECK(wins[i] > 40u);
    }
}

/*
 * A radio that has heard nobody must NOT win every context. It cannot tell an empty
 * channel from a channel full of radios it has not heard yet, and if a whole group
 * powers on together and all transmit in every slot they collide forever and never
 * converge — measured as a twelve-radio cluster settling at three of twelve reachable.
 * So it backs off to roughly one context in MANET_NAMA_MIN_CONTENDERS until it knows
 * better.
 */
static void test_net_entry_backs_off(void)
{
    manet_nb_table_t t;
    unsigned         ctx, wins = 0u;

    manet_nb_init(&t, A(5u));
    CHECK_EQ(manet_nama_contenders(&t), 1);
    for (ctx = 0u; ctx < 1200u; ctx++) {
        if (manet_nama_wins(&t, ctx)) {
            wins++;
        }
    }
    CHECK(wins > 40u);      /* still transmits often enough to be discovered */
    CHECK(wins < 250u);     /* but nothing like every slot */

    /* Two radios entering the net together must not both claim every context. */
    {
        manet_nb_table_t u;
        unsigned both = 0u;
        manet_nb_init(&u, A(6u));
        for (ctx = 0u; ctx < 1200u; ctx++) {
            if (manet_nama_wins(&t, ctx) && manet_nama_wins(&u, ctx)) {
                both++;
            }
        }
        CHECK(both < 60u);
    }
}

/* A clustered group is one contention set: exactly one winner per context, always. */
static void test_cluster_elects_exactly_one(void)
{
    bool             adj[MAXN][MAXN];
    manet_nb_table_t t[MAXN];
    unsigned         i, j, ctx;
    const unsigned   N = 12u;

    memset(adj, 0, sizeof adj);
    for (i = 0u; i < N; i++) {
        for (j = 0u; j < N; j++) {
            if (i != j) {
                link_pair(adj, i, j);
            }
        }
    }
    for (i = 0u; i < N; i++) {
        build(&t[i], i, N, adj);
        CHECK_EQ(manet_nama_contenders(&t[i]), N);   /* self + 11 direct, no two-hop */
    }
    for (ctx = 0u; ctx < 300u; ctx++) {
        unsigned winners = 0u;
        for (i = 0u; i < N; i++) {
            if (manet_nama_wins(&t[i], ctx)) {
                winners++;
            }
        }
        CHECK_EQ(winners, 1);
    }
}

static void test_next_win(void)
{
    manet_nb_table_t t;
    uint64_t         got = 0u;

    manet_nb_init(&t, A(2u));
    /* A radio still entering the net does not win immediately — it backs off — but it
     * must find a context within a reasonable search, or it can never announce itself. */
    CHECK(manet_nama_next_win(&t, 1000u, 64u, &got));
    CHECK(got >= 1000u);
    CHECK(got < 1064u);
    CHECK(manet_nama_wins(&t, got));

    /* A search window shorter than the backoff may legitimately find nothing. */
    CHECK(!manet_nama_next_win(NULL, 0u, 8u, &got));
}

void test_nama_all(void)
{
    test_deterministic();
    test_priorities_are_unique();
    test_mutual_exclusion();
    test_fairness();
    test_net_entry_backs_off();
    test_cluster_elects_exactly_one();
    test_next_win();
}
