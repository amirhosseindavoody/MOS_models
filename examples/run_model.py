"""Example: run MOSFET model, print IVs and charges, and plot IV curve if matplotlib present."""
import sys
import os
# ensure project root is on sys.path so `src` package can be imported when running as a script
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from src.mosfet_model import MOSFETParams, drain_current, total_gate_charge, partition_gate_charge, small_signal_caps
import numpy as np


def demo():
    p = MOSFETParams()
    Vs = 0.0
    Vb = 0.0
    Vg = 1.2
    Vds_list = np.linspace(0.0, 1.2, 13)
    Ids = [drain_current(Vg, v, Vs, Vb, p) for v in Vds_list]
    print("Vds (V)\tId (A)")
    for v, i in zip(Vds_list, Ids):
        print(f"{v:.3f}\t{i:.6e}")
    print('\nSample charges at Vd=Vg/2:')
    Vd = Vg / 2
    print('Qg =', total_gate_charge(Vg, Vd, Vs, Vb, p))
    print('Qgs,Qgd,Qgb =', partition_gate_charge(Vg, Vd, Vs, Vb, p))
    print('Caps =', small_signal_caps(Vg, Vd, Vs, Vb, p))


if __name__ == '__main__':
    demo()
