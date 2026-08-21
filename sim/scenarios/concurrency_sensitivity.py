"""
How much of the barrage result survives if identical co-slot copies only PARTLY stop
jamming each other?  Monkeypatched, so no tracked file is touched.

  identical_capture_db = 0   copies never jam          (ADR-0011's model)
  identical_capture_db = 10  copies jam exactly like different payloads (pre-ADR-0011)
"""
import math, sys; sys.path.insert(0,'.')
from sim.manet.core import CONFIG
from sim.manet.mobility import Static
from sim.manet.radio import ENVIRONMENTS, LinkBudget, Channel, _dbm_to_mw, _payload_key
from sim.manet.terrain import Ridge
from sim.manet.world import Simulation

def patched_decode(margin_db):
    def decode(self, rx_pos, transmissions, positions):
        if not transmissions:
            return None
        powers = [(self.rx_dbm_between(rx_pos, positions[i]), i, p) for i, p in transmissions]
        powers.sort(key=lambda t: -t[0])
        best_dbm, best_idx, best_payload = powers[0]
        if best_dbm < self.budget.sensitivity_dbm:
            return None
        if len(powers) > 1:
            mw = 0.0
            for dbm, _i, pay in powers[1:]:
                if _payload_key(pay) == _payload_key(best_payload):
                    # An identical copy only counts as interference when it is within
                    # `margin_db` of the wanted one — i.e. when capture cannot separate them.
                    if (best_dbm - dbm) < margin_db:
                        mw += _dbm_to_mw(dbm)
                else:
                    mw += _dbm_to_mw(dbm)
            if mw > 0.0 and (best_dbm - 10.0*math.log10(mw)) < self.budget.capture_db:
                return None
        return (best_idx, best_payload, best_dbm)
    return decode

ridge = Ridge(crest_x=1500.0, height_m=80.0, width_m=400.0)
pos = []
for c in (300.0, 1500.0, 2700.0):
    for i in range(4): pos.append((c+(i-2)*62.5, (i%2)*40.0))
orig = Channel.decode
print("identical copies must be this far apart in dB before capture separates them:\n")
print(f"{'margin':>7} {'own':>7} {'hilltop':>9} {'far':>7} {'slots/hop':>10} {'verdict'}")
for margin in (0, 1, 3, 6, 10):
    Channel.decode = patched_decode(margin)
    Simulation.BARRAGE_RELAY = True
    sim = Simulation(Static(pos), ENVIRONMENTS["woodland"], LinkBudget(), talker=0, terrain=ridge)
    s = CONFIG.beacon_interval_slots*4
    sim.run(s); sim.run(6000-s, voice_from=s)
    d = sim.delivery(sim.nodes[0].addr)
    g = [sum(d.get(i,0) for i in r)/len(list(r))*100 for r in (range(1,4), range(4,8), range(8,12))]
    dh = sim.delay_hist; n = sum(dh.values()) or 1
    hop = 1 + sum(k*v for k,v in dh.items())/n
    v = "as ADR-0011" if margin == 0 else ("collapses" if g[2] < 80 else "holds")
    print(f"{margin:>5} dB {g[0]:>6.1f}% {g[1]:>8.1f}% {g[2]:>6.1f}% {hop:>9.2f}   {v}")
Channel.decode = orig
