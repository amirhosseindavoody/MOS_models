"""Minimal charge-based MOSFET model.

Provides IV calculation, gate charge partition (Dutton-like), and small-signal
capacitances via numerical differentiation.
"""
from dataclasses import dataclass
import math
from typing import Tuple


@dataclass
class MOSFETParams:
    W: float = 1e-4  # channel width (m)
    L: float = 1e-6  # channel length (m)
    mu: float = 0.05  # mobility (m^2/Vs) ~ 500 cm2/Vs -> 0.05 m2/Vs
    Cox: float = 2.3e-2  # oxide capacitance per area (F/m^2) example
    Vth0: float = 0.5  # threshold at Vbs=0 (V)
    gamma: float = 0.0  # body effect coeff (minimal)
    phiF: float = 0.3  # Fermi potential (V)
    beta_partition: float = 0.5  # Dutton symmetric partition


def threshold(Vbs: float, p: MOSFETParams) -> float:
    if p.gamma == 0.0:
        return p.Vth0
    # simple body effect model
    return p.Vth0 + p.gamma * (math.sqrt(max(0.0, 2 * p.phiF - Vbs)) - math.sqrt(2 * p.phiF))


def drain_current(Vg: float, Vd: float, Vs: float, Vb: float, p: MOSFETParams) -> float:
    """Return drain current Id (A) using square-law GCA.

    Voltages absolute; computes Vgs and Vds and applies linear/sat formulas.
    """
    Vgs = Vg - Vs
    Vds = Vd - Vs
    Vbs = Vb - Vs
    Vth = threshold(Vbs, p)
    if Vgs <= Vth:
        return 0.0
    Vov = Vgs - Vth
    if Vds <= Vov:
        Id = p.mu * p.Cox * (p.W / p.L) * (Vov * Vds - 0.5 * Vds * Vds)
    else:
        Id = 0.5 * p.mu * p.Cox * (p.W / p.L) * (Vov * Vov)
    return Id


def total_gate_charge(Vg: float, Vd: float, Vs: float, Vb: float, p: MOSFETParams) -> float:
    """Return total gate charge Qg (C). Negative for electron inversion charge."""
    Vgs = Vg - Vs
    Vds = Vd - Vs
    Vbs = Vb - Vs
    Vth = threshold(Vbs, p)
    if Vgs <= Vth:
        return 0.0
    Vov = Vgs - Vth
    # effective Vds for charge integration
    Vds_eff = min(Vds, Vov)
    Qinv_per_area = p.Cox * (Vov - 0.5 * Vds_eff)
    Qinv_total = -Qinv_per_area * p.W * p.L
    return Qinv_total


def partition_gate_charge(Vg: float, Vd: float, Vs: float, Vb: float, p: MOSFETParams) -> Tuple[float, float, float]:
    """Return (Qgs, Qgd, Qgb) charges (C). Qgb is gate-to-bulk; in this minimal model it's zero.

    Qgs + Qgd + Qgb = Qg (total gate charge) by sign convention.
    """
    Qg = total_gate_charge(Vg, Vd, Vs, Vb, p)
    beta = p.beta_partition
    Qgs = beta * Qg
    Qgd = (1.0 - beta) * Qg
    Qgb = 0.0
    return Qgs, Qgd, Qgb


def numeric_capacitances(func, x0: float, h: float = 1e-6) -> float:
    # simple central difference for scalar function of one variable
    return (func(x0 + h) - func(x0 - h)) / (2 * h)


def small_signal_caps(Vg: float, Vd: float, Vs: float, Vb: float, p: MOSFETParams, h: float = 1e-6):
    """Return dictionary of small-signal capacitances (F): Cgs, Cgd, Cgb, Cgg approx.

    Uses numeric differentiation of partitioned charges.
    """
    def Qgs_wrt_Vg(v):
        return partition_gate_charge(v, Vd, Vs, Vb, p)[0]

    def Qgd_wrt_Vg(v):
        return partition_gate_charge(Vg, Vd, Vs, Vb, p)[1] if False else partition_gate_charge(Vg, Vd, Vs, Vb, p)[1]

    Cgs = -numeric_capacitances(lambda vg: partition_gate_charge(vg, Vd, Vs, Vb, p)[0], Vg, h)
    Cgd = -numeric_capacitances(lambda vd: partition_gate_charge(Vg, vd, Vs, Vb, p)[1], Vd, h)
    Cgb = -numeric_capacitances(lambda vb: partition_gate_charge(Vg, Vd, Vs, vb, p)[2], Vb, h)
    Cgg = Cgs + Cgd + Cgb
    return dict(Cgs=Cgs, Cgd=Cgd, Cgb=Cgb, Cgg=Cgg)


if __name__ == "__main__":
    # simple demo
    p = MOSFETParams()
    Vg = 1.5
    Vs = 0.0
    Vd = 1.0
    Vb = 0.0
    print("Id =", drain_current(Vg, Vd, Vs, Vb, p))
    print("Qg =", total_gate_charge(Vg, Vd, Vs, Vb, p))
    print("Qgs,Qgd,Qgb =", partition_gate_charge(Vg, Vd, Vs, Vb, p))
    print("Caps =", small_signal_caps(Vg, Vd, Vs, Vb, p))
