"""OQ-0028 power-delta query: when several relays carry ONE payload into one slot,
how far apart do they arrive? Combining only matters where they arrive close."""
import sys, math, statistics, collections
sys.path.insert(0,'/Users/willsheard/Documents/Development/MANET-VHF')
from sim.manet import radio as R
from sim.manet.core import CONFIG, VOICE
from sim.manet.geometry import within_one_hop_m, horizon_m
from sim.manet.mobility import Static
from sim.manet.radio import Channel, ENVIRONMENTS, LinkBudget
from sim.manet.terrain import Ridge
from sim.manet.world import Simulation
from sim.scenarios.atlas import SHOULDER, PAIR, ROW, scatter, centre_of

W,B = ENVIRONMENTS["woodland"], LinkBudget()
one, hz = within_one_hop_m(W,B), horizon_m(W,B)

def run(name, pos, terrain=None, talker=0, slots=3000):
    sim = Simulation(Static(pos), W, B, talker=talker, terrain=terrain)
    deltas=[]; crowd=collections.Counter(); solo=0; multi=0
    real = sim.channel.decode
    def decode(rx_pos, txs, positions, _r=real):
        got=_r(rx_pos, txs, positions)
        # group audible copies by what is actually on the wire
        by=collections.defaultdict(list)
        for idx,payload in txs:
            pdu = payload[0] if isinstance(payload,tuple) else payload
            if pdu.type != VOICE: continue
            p = sim.channel.rx_dbm_between(rx_pos, positions[idx])
            if p >= B.sensitivity_dbm:
                by[R._payload_key(payload if isinstance(payload,tuple) else (payload,None))].append(p)
        for k,ps in by.items():
            nonlocal_solo = len(ps)
            crowd[len(ps)] += 1
            if len(ps) >= 2:
                ps.sort(reverse=True)
                deltas.append(ps[0]-ps[1])
        return got
    sim.channel.decode = decode
    k = CONFIG.beacon_interval_slots*4
    sim.run(k); sim.run(slots-k, voice_from=k)
    return name, deltas, crowd

# The atlas lays radios out on an exact lattice, which makes distances match to the metre
# between symmetric pairs and reports near-zero power deltas that are a property of the
# arithmetic rather than of the radio. People do not stand on a grid. Jitter breaks the
# symmetry without changing the topology: +/- 25 m is a couple of paces.
def jitter(pos, seed=1):
    h = 0x811C9DC5
    out = []
    for i,(x,y) in enumerate(pos):
        for v in (i, seed):
            h = ((h ^ v) * 0x01000193) & 0xFFFFFFFF; h ^= h >> 13
        dx = ((h & 0xFFFF) / 65535.0 - 0.5) * 50.0
        h = (h * 0x9E3779B1) & 0xFFFFFFFF
        dy = ((h & 0xFFFF) / 65535.0 - 0.5) * 50.0
        out.append((x + dx, y + dy))
    return out


CASES = [
 ("seven groups of six",      [(g*one*1.05+SHOULDER*(i%3), SHOULDER*(i//3)) for g in range(7) for i in range(6)], None, 0),
 ("eight groups over a ridge",[(g*one*0.9+PAIR*(i%2), ROW*(i//2)) for g in range(8) for i in range(4)],
                              Ridge(crest_x=one*3.6, height_m=80.0, width_m=400.0), 0),
 ("40 spread over ground",    scatter(40, one*2.2, seed=3), None, 0),
 ("100 over 38 km",           scatter(100, one*8.6, seed=11), None, None),
 ("twelve in a line",         [(i*hz*0.55, 0.0) for i in range(12)], None, 0),
 ("twelve together",          [(SHOULDER*(i%4), SHOULDER*(i//4)) for i in range(12)], None, 0),
]
print(f"{'scenario':<28}{'co-slot copies':>15}{'>=2 copies':>12}"
      f"{'within 1 dB':>13}{'within 3 dB':>13}{'within 6 dB':>13}{'median':>9}")
for name,pos,terr,t in CASES:
    pos = jitter(pos)
    tk = centre_of(pos) if t is None else t
    _,d,c = run(name, pos, terr, tk)
    tot = sum(c.values()); multi = sum(v for k,v in c.items() if k>=2)
    if not d:
        print(f"{name:<28}{tot:>15}{multi:>12}{'—':>13}{'—':>13}{'—':>13}{'—':>9}")
        continue
    w1=sum(1 for x in d if x<=1.0); w3=sum(1 for x in d if x<=3.0); w6=sum(1 for x in d if x<=6.0)
    print(f"{name:<28}{tot:>15}{multi:>12}"
          f"{w1/len(d)*100:>12.1f}%{w3/len(d)*100:>12.1f}%{w6/len(d)*100:>12.1f}%"
          f"{statistics.median(d):>8.1f}")
