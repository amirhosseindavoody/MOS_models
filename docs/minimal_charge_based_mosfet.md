# Minimal Charge-Based MOSFET Compact Model

This document describes a minimal charge-based compact model for an n-channel MOSFET suitable for circuit simulation and teaching. The model focuses on charge partitioning and a consistent IV/charge formulation using the gradual channel approximation (GCA) and a simple gate-charge partition (Dutton partition).

**Assumptions**
- Long-channel device; velocity saturation and short-channel effects ignored.
- Uniform channel doping and gradual channel approximation (GCA) valid.
- Temperature effects, mobility degradation, and series resistances omitted for simplicity.
- Body effect included via body bias in threshold voltage.

## Notation and parameters
- `Vg`, `Vd`, `Vs`, `Vb` : gate, drain, source, body voltages.
- `Vgs = Vg - Vs`, `Vds = Vd - Vs`, `Vbs = Vb - Vs`.
- `Cox` : gate oxide capacitance per unit area.
- `W`, `L` : channel width and length.
- `mu` : carrier mobility (effective electron mobility).
- `Vth0` : threshold voltage at `Vbs = 0`.
- `gamma` : body effect coefficient.
- `phiF` : Fermi potential (absolute value) used in body effect.
- `kappa` : gate coupling coefficient (often ~1 for ideal oxide), here kappa = Cox/(Cox + Cdep) when depletion considered.

## Threshold with body effect

The threshold voltage including body effect is given by:

$$
V_{th} = V_{th0} + \gamma\left(\sqrt{2\phi_F - V_{bs}} - \sqrt{2\phi_F}\right)
$$

For a minimal model we can fix $\phi_F$ and $\gamma$ or set body effect to zero.

## Inversion charge per unit area (Qinv)

We use a simple expression valid above threshold (charge-sheet approximation). For $V_{gs} > V_{th}$:

$$
Q_{inv}(x) = -C_{ox}\left(V_{gs} - V_{th} - V(x)\right)
$$

Here $V(x)$ is the local channel potential measured from the source ($V(0)=0$, $V(L)=V_{ds}$). The negative sign indicates electron charge.

## Drain current (GCA)

Using the gradual channel approximation, the local drift current is

$$
i = \mu W Q_{inv}(x)\frac{dV}{dx}
$$

The drain current is constant along the channel, so integrate from $x=0$ to $L$:

$$
I_D = \mu W\int_{0}^{L} Q_{inv}(x)\frac{dV}{dx}\,dx
= \frac{\mu W}{L}\int_{0}^{V_{ds}} Q_{inv}(V)\,dV
$$

Substitute $Q_{inv}(V)=-C_{ox}(V_{gs}-V_{th}-V)$ and integrate. In the linear region ($V_{ds}\le V_{gs}-V_{th}$):

$$
I_D = \mu C_{ox}\frac{W}{L}\left[(V_{gs}-V_{th})V_{ds}-\frac{V_{ds}^2}{2}\right]
$$

In saturation (pinch-off) where $V_{ds}\ge V_{gs}-V_{th}$ we set $V_{ds}=V_{sat}=V_{gs}-V_{th}$ in the integral and obtain the familiar square-law:

$$
I_{D,sat}=\frac{\mu C_{ox}}{2}\frac{W}{L}(V_{gs}-V_{th})^2
$$

This is the classical square-law GCA result.

## Gate charge partition (Dutton partition)

The total gate charge (integrated over channel length) is

$$
Q_g = -W\int_{0}^{L} Q_{inv}(x)\,dx = -W C_{ox}\int_{0}^{L}\left(V_{gs}-V_{th}-V(x)\right)\,dx
$$

Assuming a linear potential drop along the channel (simple approximation) and changing integration variable to $V$ using $dx = L\,dV/V_{ds}$,

$$
Q_g = -W C_{ox} L \frac{1}{V_{ds}}\int_{0}^{V_{ds}}\left(V_{gs}-V_{th}-V\right)\,dV
$$

In the non-saturated case ($V_{ds}\le V_{gs}-V_{th}$) the integral yields:

$$
Q_g = -W C_{ox} L \left[(V_{gs}-V_{th})-\frac{V_{ds}}{2}\right]
$$

Dutton partition splits $Q_g$ into gate-to-source $Q_{gs}$ and gate-to-drain $Q_{gd}$ according to a partition factor $\alpha$:

$$
\begin{aligned}
Q_{gs} &= -W C_{ox} L\left[(V_{gs}-V_{th})-\alpha V_{ds}\right],\\
Q_{gd} &= -W C_{ox} L\left[V_{ds}\left(\alpha - \tfrac{1}{2}\right)\right].
\end{aligned}
$$

Dutton suggests $\alpha=\tfrac{1}{2}$ for a symmetric partition in the linear region. Near saturation, $\alpha$ shifts so that $Q_{gd}$ reduces as the channel pinches off near the drain.

For a minimal implementation we therefore compute a total inversion charge (per device) as:

$$
Q_{inv,total} = -W C_{ox} L \begin{cases}
 (V_{gs}-V_{th}-\dfrac{V_{ds}}{2}), & V_{ds} \le V_{gs}-V_{th}, \\
 (V_{gs}-V_{th})/2, & V_{ds} > V_{gs}-V_{th}.
\end{cases}
$$

Then we partition using a constant fraction $\beta$ (default $\beta=1/2$):

$$
Q_{gs} = \beta\,Q_{inv,total},\qquad Q_{gd} = (1-\beta)\,Q_{inv,total}.
$$

## Small-signal capacitances

Small-signal capacitances are defined by derivatives of terminal charges with respect to terminal voltages:

$$
C_{ij} = \frac{\partial Q_i}{\partial V_j}
$$

In the minimal model, the dominant gate capacitance magnitude is approximately

$$
C_{ox}W L
$$

and the gate-to-source and gate-to-drain capacitances can be approximated by differentiating the partitioned charges:

$$
C_{gs} \approx -\frac{\partial Q_{gs}}{\partial V_{gs}},\qquad C_{gd} \approx -\frac{\partial Q_{gd}}{\partial V_{gd}}.
$$

Using the simple (linear-region) expressions, $C_{gs}$ and $C_{gd}$ are roughly equal when $\beta=1/2$.

## Limitations and extensions

- No velocity saturation, channel-length modulation, or mobility degradation included.
- Body-effect and depletion capacitance treated minimally; adding `Cdep` produces a more accurate kappa.
- For subthreshold operation a different charge expression (exponential) is required.

## Summary

This minimal charge-based model yields classical square-law IVs and a continuous gate-charge partition useful for circuit-level charge-consistent modeling. The accompanying Python implementation (`src/mosfet_model.py`) implements these equations, returns terminal currents and charges, and provides small-signal capacitances by numerical differentiation for robustness.
# Minimal Charge-Based MOSFET Compact Model

This document describes a minimal charge-based compact model for an n-channel MOSFET suitable for circuit simulation and teaching. The model focuses on charge partitioning and a consistent IV/charge formulation using the gradual channel approximation (GCA) and a simple gate-charge partition (Dutton partition).

**Assumptions**
- Long-channel device; velocity saturation and short-channel effects ignored.
- Uniform channel doping and gradual channel approximation (GCA) valid.
- Temperature effects, mobility degradation, and series resistances omitted for simplicity.
- Body effect included via body bias in threshold voltage.

## Notation and parameters
- `Vg`, `Vd`, `Vs`, `Vb` : gate, drain, source, body voltages.
- `Vgs = Vg - Vs`, `Vds = Vd - Vs`, `Vbs = Vb - Vs`.
- `Cox` : gate oxide capacitance per unit area.
- `W`, `L` : channel width and length.
- `mu` : carrier mobility (effective electron mobility).
- `Vth0` : threshold voltage at `Vbs = 0`.
- `gamma` : body effect coefficient.
- `phiF` : Fermi potential (absolute value) used in body effect.
- `kappa` : gate coupling coefficient (often ~1 for ideal oxide), here kappa = Cox/(Cox + Cdep) when depletion considered.

## Threshold with body effect

The threshold voltage including body effect is given by:

$$
V_{th} = V_{th0} + \gamma\left(\sqrt{2\phi_F - V_{bs}} - \sqrt{2\phi_F}\right)
$$

For a minimal model we can fix $\phi_F$ and $\gamma$ or set body effect to zero.

## Inversion charge per unit area (Qinv)

We use a simple expression valid above threshold (charge-sheet approximation). For $V_{gs} > V_{th}$:

$$
Q_{inv}(x) = -C_{ox}\left(V_{gs} - V_{th} - V(x)\right)
$$

Here $V(x)$ is the local channel potential measured from the source ($V(0)=0$, $V(L)=V_{ds}$). The negative sign indicates electron charge.

## Drain current (GCA)

Using the gradual channel approximation, the local drift current is

$$
i = \mu W Q_{inv}(x)\frac{dV}{dx}
$$

The drain current is constant along the channel, so integrate from $x=0$ to $L$:

$$
I_D = \mu W\int_{0}^{L} Q_{inv}(x)\frac{dV}{dx}\,dx
= \frac{\mu W}{L}\int_{0}^{V_{ds}} Q_{inv}(V)\,dV
$$

Substitute $Q_{inv}(V)=-C_{ox}(V_{gs}-V_{th}-V)$ and integrate. In the linear region ($V_{ds}\le V_{gs}-V_{th}$):

$$
I_D = \mu C_{ox}\frac{W}{L}\left[(V_{gs}-V_{th})V_{ds}-\frac{V_{ds}^2}{2}\right]
$$

In saturation (pinch-off) where $V_{ds}\ge V_{gs}-V_{th}$ we set $V_{ds}=V_{sat}=V_{gs}-V_{th}$ in the integral and obtain the familiar square-law:

$$
I_{D,sat}=\frac{\mu C_{ox}}{2}\frac{W}{L}(V_{gs}-V_{th})^2
$$

This is the classical square-law GCA result.

## Gate charge partition (Dutton partition)

The total gate charge (integrated over channel length) is

$$
Q_g = -W\int_{0}^{L} Q_{inv}(x)\,dx = -W C_{ox}\int_{0}^{L}\left(V_{gs}-V_{th}-V(x)\right)\,dx
$$

Assuming a linear potential drop along the channel (simple approximation) and changing integration variable to $V$ using $dx = L\,dV/V_{ds}$,

$$
Q_g = -W C_{ox} L \frac{1}{V_{ds}}\int_{0}^{V_{ds}}\left(V_{gs}-V_{th}-V\right)\,dV
$$

In the non-saturated case ($V_{ds}\le V_{gs}-V_{th}$) the integral yields:

$$
Q_g = -W C_{ox} L \left[(V_{gs}-V_{th})-\frac{V_{ds}}{2}\right]
$$

Dutton partition splits $Q_g$ into gate-to-source $Q_{gs}$ and gate-to-drain $Q_{gd}$ according to a partition factor $\alpha$:

$$
\begin{aligned}
Q_{gs} &= -W C_{ox} L\left[(V_{gs}-V_{th})-\alpha V_{ds}\right],\\
Q_{gd} &= -W C_{ox} L\left[V_{ds}\left(\alpha - \tfrac{1}{2}\right)\right].
\end{aligned}
$$

Dutton suggests $\alpha=\tfrac{1}{2}$ for a symmetric partition in the linear region. Near saturation, $\alpha$ shifts so that $Q_{gd}$ reduces as the channel pinches off near the drain.

For a minimal implementation we therefore compute a total inversion charge (per device) as:

$$
Q_{inv,total} = -W C_{ox} L \begin{cases}
 (V_{gs}-V_{th}-\dfrac{V_{ds}}{2}), & V_{ds} \le V_{gs}-V_{th}, \\
 (V_{gs}-V_{th})/2, & V_{ds} > V_{gs}-V_{th}.
\end{cases}
$$

Then we partition using a constant fraction $\beta$ (default $\beta=1/2$):

$$
Q_{gs} = \beta\,Q_{inv,total},\qquad Q_{gd} = (1-\beta)\,Q_{inv,total}.
$$

## Small-signal capacitances

Small-signal capacitances are defined by derivatives of terminal charges with respect to terminal voltages:

$$
C_{ij} = \frac{\partial Q_i}{\partial V_j}
$$

In the minimal model, the dominant gate capacitance magnitude is approximately

$$
C_{ox}W L
$$

and the gate-to-source and gate-to-drain capacitances can be approximated by differentiating the partitioned charges:

$$
C_{gs} \approx -\frac{\partial Q_{gs}}{\partial V_{gs}},\qquad C_{gd} \approx -\frac{\partial Q_{gd}}{\partial V_{gd}}.
$$

Using the simple (linear-region) expressions, $C_{gs}$ and $C_{gd}$ are roughly equal when $\beta=1/2$.

## Limitations and extensions

- No velocity saturation, channel-length modulation, or mobility degradation included.
- Body-effect and depletion capacitance treated minimally; adding `Cdep` produces a more accurate kappa.
- For subthreshold operation a different charge expression (exponential) is required.

## Summary

This minimal charge-based model yields classical square-law IVs and a continuous gate-charge partition useful for circuit-level charge-consistent modeling. The accompanying Python implementation (`src/mosfet_model.py`) implements these equations, returns terminal currents and charges, and provides small-signal capacitances by numerical differentiation for robustness.
