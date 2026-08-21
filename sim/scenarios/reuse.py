"""
OQ-0013 — does spatial reuse hold at three slots as well as four?

Under slot pipelining the originator transmits every frame, so with N slots the node N
hops down the chain transmits in the same slot at the same instant. The question is
whether a receiver trying to hear its immediate neighbour is spoiled by that distant
transmitter.

The geometry: a receiver at hop k+1 is listening to hop k, one spacing away. The other
simultaneous transmitter sits at hop k+N, which is N-1 spacings away. So the wanted and
unwanted signals differ only by the path loss between one spacing and N-1 spacings —
which is a much smaller ratio than intuition suggests.
"""

from ..manet.radio import ENVIRONMENTS, LinkBudget, usable_range_m
from ..manet.world import Simulation


def chain(n, spacing):
    return [(i * spacing, 0.0) for i in range(n)]


def run(nodes=8, spacing_frac=0.9, slots=1200, env_name="woodland"):
    env = ENVIRONMENTS[env_name]
    budget = LinkBudget()
    reach = usable_range_m(env, budget)
    spacing = spacing_frac * reach

    sim = Simulation(chain(nodes, spacing), env, budget, talker=0)
    sim.run(slots // 3)                     # let the topology settle
    settle = sim.slot
    sim.run(slots - settle, voice_from=settle)

    src = sim.nodes[0].addr
    return {
        "env": env.name,
        "reach_m": reach,
        "spacing_m": spacing,
        "converged": sim.converged(),
        "delivery": sim.delivery(src),
        "latency": sim.latency_slots(src),
        "relays": sim.relay_total(),
        "collisions": sim.collisions,
        "originated": sim.nodes[0].originated,
        "margin_db": _margin(env, budget, spacing),
    }


def _margin(env, budget, spacing):
    """
    Wanted-to-interferer ratio the receiver actually sees, in dB.

    Wanted is one spacing away; the nearest simultaneous transmitter is (N-1) spacings
    away, where N is the slot count. Below the demodulator's capture threshold, both are
    lost.
    """
    from ..manet.core import CONFIG
    n = CONFIG.slots_per_frame
    wanted = env.path_loss_db(spacing, budget.freq_hz)
    interferer = env.path_loss_db(spacing * (n - 1), budget.freq_hz)
    return interferer - wanted
