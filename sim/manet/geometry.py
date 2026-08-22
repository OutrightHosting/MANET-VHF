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
DEPENDABLE = 0.55

# How far outside the horizon a link has to sit before it is reliably absent. Cluster jitter
# and mobility both narrow real separations, so "just past the horizon" is not enough.
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
