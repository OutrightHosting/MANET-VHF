"""
Scenario distances, expressed as multiples of the measured radio horizon.

WHY THIS MODULE EXISTS. Three scenarios in a row shipped with distances hardcoded in
metres, chosen when the woodland horizon was one value and left behind when it became
another:

  gate Q1   stretched the group to 3000 m against a 4416 m horizon -> the mobility test
            never left one hop and could not fail, for weeks
  gate Q5   separated the halves by 4000 m against the same horizon -> the partition test
            never partitioned, and then reported a healing time for a split that had not
            happened
  hill.py   put the groups 1200 m apart and the ridge at 1500 m -> at any shorter horizon
            the valleys cannot reach the hilltop and the scenario silently stops testing
            relaying at all

Every one of them still reported PASS. The failure mode is not that a number is wrong; it
is that a number written in metres has no relationship to the model that gives it meaning,
so when the model moves the number stays and the test quietly stops testing.

THE RULE: a scenario states distances as INTENT -- one hop, three hops, just out of range --
and this module converts intent into metres against the horizon measured right now. Bare
metre literals in sim/scenarios/ are rejected by `make geometry-check`.

The exception is deliberate and narrow: quantities that are not radio distances at all --
antenna heights, ridge dimensions, cluster jitter -- are physical facts about the world and
do not scale with the link budget. Name them, do not derive them.
"""

from .radio import usable_range_m

# How far inside the horizon a link has to sit before it is dependable rather than marginal.
# At the horizon itself the wanted signal is exactly at the demodulator's limit, so a
# scenario placing nodes there is testing the sensitivity figure, not the protocol.
#
# BOTH CONSTANTS DESCRIBE A DETERMINISTIC CHANNEL, AND ONLY A DETERMINISTIC CHANNEL.
# With Channel.SHADOWING off a link either works or it does not, and 0.55 buys 7.8 dB of
# margin, which is plenty. Switch shadowing on (M-06) and neither word survives contact,
# because sigma is 7.0 dB and 7.8 dB of margin is only 1.1 sigma. Measured over 1600
# placements in woodland:
#
#     spacing                      links up, clean    with shadowing
#     0.55x horizon  DEPENDABLE             100.0%             86.8%
#     0.75x                                 100.0%             69.6%
#     0.90x                                 100.0%             58.6%
#     1.00x  at the horizon                   0.0%             52.2%
#     1.60x horizon  SEVERED                  0.0%             20.0%
#     3.00x                                    0.0%              1.9%
#
# So DEPENDABLE fails one link in seven, and SEVERED carries traffic one time in five.
# Restoring the literal meanings would need roughly 0.25x and 3.0x -- less than half and
# nearly double the present values, which would shrink every scenario in the library.
#
# THAT IS NOT THE RIGHT FIX, because the constants are not wrong about what matters.
# Whether a single link survives turns out to be a poor predictor of whether the NETWORK
# does, and the difference is redundancy rather than margin. Measured across eight draws of
# the shadowing field over all 33 atlas scenarios:
#
#   - every dispersed topology held every radio in every draw -- 20, 40, 60 and 100 radios
#     spread over ground, both dense clusters, and seven groups of six;
#   - twelve radios in a LINE at this same 0.55x spacing read 8/12 on the clean channel,
#     usually managed 12/12, and on unlucky ground collapsed to 1/12. A line only becomes
#     robust in every draw at about 0.35x horizon;
#   - the two- and three-radio cases are the other exposed shape, for the same reason: a
#     bare pair at 0.55x is a one-in-seven coin flip with nothing to carry the traffic
#     instead, which is why the atlas shows "Two radios, in range" losing one of two on its
#     worst ground.
#
# So read DEPENDABLE as "dependable when something else can carry the frame", which is the
# usual case and the case the product is for. Where a scenario has NO alternative route --
# a chain, a pair, a single bridging relay -- it is marginal, and it should be stated as
# marginal rather than dependable. See OQ-0036 for the one unmeasured parameter underneath
# all of this: how much of the fade two nearby radios share.
DEPENDABLE = 0.55

# How far outside the horizon a link has to sit before it is reliably absent. Cluster jitter
# and mobility both narrow real separations, so "just past the horizon" is not enough.
#
# Reliably absent ON A DETERMINISTIC CHANNEL. Under shadowing one link in five at this
# distance still carries traffic (table above), so a scenario relying on SEVERED to prove a
# partition is proving it only in the clean channel. Gate Q5 depends on this -- it asserts
# the two halves are genuinely split before measuring how fast they rejoin -- so if
# shadowing is ever turned on by default, Q5's separation needs re-deriving at roughly 3.0x
# or the precondition stops holding.
SEVERED = 1.6


def horizon_m(env, budget):
    """The radio horizon: distance at which a wanted signal just reaches the demodulator."""
    return usable_range_m(env, budget)


def hop_span_m(env, budget, hops):
    """
    Distance spanning `hops` radio hops, with each hop dependable rather than marginal.

    Use for "stretch the group until it is N hops deep". Note the result is what the
    TOPOLOGY spans; whether voice crosses it is a separate question owned by
    MANET_VOICE_TTL and the latency budget.
    """
    if hops < 1:
        raise ValueError("hop_span_m needs at least one hop")
    return hops * DEPENDABLE * horizon_m(env, budget)


def within_one_hop_m(env, budget):
    """A separation every node can bridge directly. The clustered case."""
    return DEPENDABLE * horizon_m(env, budget)


def severed_m(env, budget, jitter_m=0.0):
    """
    A separation that reliably breaks contact, allowing for `jitter_m` of scatter at each
    end pulling the closest pair together.

    Use for partitions. Passing the jitter explicitly is the point: gate Q5's 4000 m failed
    partly because +/-150 m of cluster scatter put the closest cross-boundary pair at 3700 m
    and nobody had subtracted it.
    """
    return SEVERED * horizon_m(env, budget) + 2.0 * jitter_m


def chain_spacing_m(env, budget):
    """
    Spacing for a forced chain: close enough that neighbours hear each other dependably,
    far enough that nobody reaches past their immediate neighbours.
    """
    return DEPENDABLE * horizon_m(env, budget)
