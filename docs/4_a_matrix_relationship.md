# Relation Between MNA Matrix `A` and Resistive/Reactive Jacobian Elements

This document explains how the global MNA matrix `A` is assembled from device-level Jacobian contributions for resistive and reactive elements, and how time-discretized reactive elements (Backward Euler) map to conductance and RHS history terms.

## 1. Global matrix `A` definition

- The MNA system of equations is written as
  $$A x = z,$$
  where `x` contains node voltages and extra branch currents. Equivalently, the MNA matrix `A` is the Jacobian of the KCL residual vector `F(x)`:
  $$
  F(x)=0 \qquad\Longrightarrow\quad A=\frac{\partial F}{\partial x}.
  $$ 
- Each device contributes local currents $I_i^{(\text{device})}(x)$ to the KCL residual at node `i`, so the global residual is
  $$F_i(x)=\sum_{\text{devices}} I_i^{(\text{device})}(x) - z_i,$$
  and the MNA matrix entries are
  $$A_{ij}=\frac{\partial F_i}{\partial x_j}=\sum_{\text{devices}}\frac{\partial I_i^{(\text{device})}}{\partial x_j}.$$

## 2. Resistive elements

- A resistor between `n1` and `n2` with resistance $R$ contributes a conductance $g=1/R$ and updates `A` with the standard 2×2 conductance stamp:
  $$
  \begin{aligned}
  A[n1,n1] &\mathrel{+}= g, & A[n2,n2] &\mathrel{+}= g,\\
  A[n1,n2] &\mathrel{-}= g, & A[n2,n1] &\mathrel{-}= g.
  \end{aligned}
  $$

- In Jacobian terms, these are the partial derivatives of the currents injected at the nodes with respect to the node voltages, e.g. $\partial I_{n1}/\partial v_{n1}=+g$ and $\partial I_{n1}/\partial v_{n2}=-g$.

## 3. Reactive elements (time-domain)

Reactive elements are converted to equivalent DC stamps using a numerical integrator. With Backward Euler (BE) at timestep $h$:

- **Capacitor** $C$ between `n1` and `n2`:
  - Equivalent conductance:
    $$
    G_{eq}=\frac{C}{h}.
    $$ 
  - History current (RHS):
    $$
    I_{eq}=G_{eq}\left[v_{n-1}(n1)-v_{n-1}(n2)\right].
    $$
  - `G_eq` is stamped into `A` using the same 2×2 pattern as a resistor; `I_eq` is added to the RHS `z` with the sign convention described in the texts (e.g., `CtxAddZ(n1, -I_eq); CtxAddZ(n2, +I_eq);`).

- **Inductor** $L$ between `n1` and `n2` (with extra current variable index `k`):
  - Equivalent series resistance:
    $$
    R_{eq}=\frac{L}{h}.
    $$ 
  - History voltage (RHS):
    $$
    V_{eq}=R_{eq}\,i_{L,n-1}.
    $$ 
  - Stamp (using extra variable `k`):
    $$
    \begin{aligned}
    &A[n1,k]\mathrel{+}=1,\quad A[n2,k]\mathrel{-}=1,\\
    &A[k,n1]\mathrel{+}=1,\quad A[k,n2]\mathrel{-}=1,\\
    &A[k,k]\mathrel{-}=R_{eq},\quad z[k]\mathrel{-}=V_{eq}.
    \end{aligned}
    $$

## 4. Nonlinear devices and local Jacobians

- Any nonlinear device contributes partial derivatives of its terminal currents with respect to terminal voltages to the global Jacobian. If a device injects local currents $I_i^{(d)}(x)$ at node `i`, then the contribution to the Jacobian is:
  $$
  A_{ij}\mathrel{+}=\frac{\partial I_i^{(d)}}{\partial x_j}.
  $$ 

- Example — diode (Shockley):
  $$
  I_d(V_d)=I_s\left(e^{V_d/(nV_t)}-1\right),\qquad g_{eq}=\frac{dI_d}{dV_d}=\frac{I_s}{nV_t}e^{V_d/(nV_t)}.
  $$ 
  Stamp `g_eq` into the same 2×2 conductance pattern; place the history/current bias into `z` as
  $$
  I_{eq}=I_d(V_d^k)-g_{eq}V_d^k.
  $$ 

## 5. Compact formula

- If the residual at node `i` is
  $$
  F_i(x)=\sum_{\text{devices}} I_i^{(\text{device})}(x) - z_i,
  $$
  then
  $$
  A_{ij}=\frac{\partial F_i}{\partial x_j}=\sum_{\text{devices}}\frac{\partial I_i^{(\text{device})}}{\partial x_j}.
  $$ 

## 6. Practical notes

- Build `A` by summing local device derivatives into a `StampContext` (COO triplets) and convert to CSR/dense once per assembly.
- Respect the RHS sign convention: for a current source from `n1` to `n2` call `CtxAddZ(n1_idx, -I); CtxAddZ(n2_idx, +I);`.
- For inductors remember to allocate an extra current variable and stamp its row/column accordingly.

---

If you want, I can also produce a compact lookup table mapping each device type to the exact `CtxAddA` / `CtxAddZ` calls required for implementation. Would you like that? 
