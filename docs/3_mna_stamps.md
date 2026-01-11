# MNA Stamps for mini-spice Components

This document outlines the "stamps" used to build the Modified Nodal Analysis (MNA) matrix (`A`) and the right-hand side vector (`z`) for each circuit component supported in mini-spice. The system of equations to be solved is `A * x = z`, where `x` is the vector of unknown node voltages and branch currents.

In the following descriptions:
-   `n1` and `n2` refer to the nodes the component is connected to.
-   `A[i][j]` is the element in the `i`-th row and `j`-th column of the MNA matrix.
-   `z[i]` is the `i`-th element of the RHS vector.

## Sign conventions

| Current direction    | Jacobian  | RHS       |
| :--------            | :-------: | :-------: |
|Current out of Node   | +         | -         |
|Current into the Node | -         | +         |


## 1. Resistor (R)

A resistor is a linear, memoryless component.

-   **Connections**: `n1`, `n2`
-   **Value**: Resistance `R`, Conductance `g = 1/R`
-   **MNA Stamp**: The resistor adds its conductance value to the `A` matrix.

```
A[n1][n1] += g
A[n2][n2] += g
A[n1][n2] -= g
A[n2][n1] -= g
```

This stamp is the same for both DC and transient analysis.

### Equations

- **Conductance:** $g = \dfrac{1}{R}$. There is no $G_{eq}$ or $I_{eq}$ for a linear resistor because it is time-invariant and memoryless.

---

## 2. DC Current Source (I)

A DC current source injects a constant current into the circuit.

-   **Connections**: `n1` (positive, current flows out of), `n2` (negative, current flows into)
-   **Value**: Current `I`
-   **MNA Stamp**: The source adds its current value to the `z` vector.

```
z[n1] -= I
z[n2] += I
```

This stamp is the same for both DC and transient analysis.

### Equations

- **Current source:** The DC current source injects a fixed current $I$ and does not produce a $G_{eq}$ term. The contribution to the RHS is simply
    $$
    z[n1] \mathrel{-}= I,\qquad z[n2] \mathrel{+}= I.
    $$ 
    There is no derivation for $I_{eq}$ because the source value is given and time-invariant for DC.

---

## 3. DC Voltage Source (V)

A DC voltage source maintains a constant voltage between its terminals. Its inclusion requires an additional variable in the `x` vector for the current flowing through the source, `i_v`.

-   **Connections**: `n1` (positive), `n2` (negative)
-   **Value**: Voltage `V`
-   **Extra Variable**: Let `k` be the index of the new variable `i_v`.
-   **MNA Stamp**:

```
// Entries to handle the new current variable
A[n1][k] += 1
A[n2][k] -= 1

// Entries for the voltage constraint equation: v(n1) - v(n2) = V
A[k][n1] += 1
A[k][n2] -= 1
z[k]    += V
```

This stamp is the same for both DC and transient analysis.

### Equations

- **Voltage source constraint:** The voltage source enforces
    $$
    v(n1)-v(n2)=V.
    $$ 
    Introducing the extra variable $i_v$ (the source current) produces the pair of MNA rows/cols shown above. There is no $G_{eq}$ or $I_{eq}$ for an ideal DC voltage source in the MNA formulation; instead the constraint is enforced via the additional KCL/KVL rows.

---

## 4. Capacitor (C)

A capacitor is a dynamic element whose current depends on the rate of change of voltage.

-   **Connections**: `n1`, `n2`
-   **Value**: Capacitance `C`

### DC Analysis

In DC analysis, a capacitor is an open circuit. It has no effect on the MNA matrix.

### Transient Analysis

For transient analysis, the capacitor is converted into a DC equivalent model at each time step using the selected **integration method**. The model consists of a conductance `G_eq` in parallel with a current source `I_eq`.

#### General Formulation

Using integration coefficients from the `IntegrationMethod` interface:

-   **Equivalent Conductance**: `G_eq = α₀ · C / h`
-   **History Current**: `I_eq = (α₁ · C / h) · v_{n-1} + (α₂ · C / h) · v_{n-2} + ...`

For Trapezoidal rule, also include the previous current: `I_eq += i_{n-1}`

#### Integration Method Coefficients for Capacitor

| Method | α₀ | α₁ | α₂ | History includes i_{n-1}? |
|--------|-----|-----|-----|---------------------------|
| **Backward Euler** | 1 | 1 | 0 | No |
| **Trapezoidal** | 2 | 2 | 0 | Yes |
| **Gear (BDF2)** | 3/2 | 2 | -1/2 | No |

#### Backward Euler (Default)

-   **Equivalent Conductance**: `G_eq = C / h`
-   **Equivalent Current**: `I_eq = (C / h) · (v_{n-1}(n1) - v_{n-1}(n2))`

##### Derivation (Backward Euler)
The capacitor's behavior is described by $i(t) = C \frac{dv(t)}{dt}$. Using the Backward Euler approximation for the derivative at time $t_n$:
$$ \frac{dv(t_n)}{dt} \approx \frac{v(t_n) - v(t_{n-1})}{h} $$
Substituting this into the capacitor equation gives the current at time $t_n$:
$$ i_n = \frac{C}{h} (v_n - v_{n-1}) $$
Letting $v_n$ be the voltage across the capacitor, $v_n = v_n(n1) - v_n(n2)$, we can rearrange the equation:
$$ i_n = \underbrace{\left(\frac{C}{h}\right)}_{G_{eq}} (v_n(n1) - v_n(n2)) - \underbrace{\left(\frac{C}{h}\right)(v_{n-1}(n1) - v_{n-1}(n2))}_{I_{eq}} $$

#### Trapezoidal Rule

-   **Equivalent Conductance**: `G_eq = 2C / h`
-   **Equivalent Current**: `I_eq = (2C / h) · v_{n-1} + i_{n-1}`

##### Derivation (Trapezoidal)
The trapezoidal rule uses:
$$ i_n + i_{n-1} = \frac{2C}{h}(v_n - v_{n-1}) $$
Rearranging:
$$ i_n = \underbrace{\frac{2C}{h}}_{G_{eq}} v_n - \underbrace{\left(\frac{2C}{h} v_{n-1} + i_{n-1}\right)}_{I_{eq}} $$

-   **MNA Stamp**:

```
// Conductance part
A[n1][n1] += G_eq
A[n2][n2] += G_eq
A[n1][n2] -= G_eq
A[n2][n1] -= G_eq

// Current source part
z[n1] -= I_eq
z[n2] += I_eq
```


## 5. Inductor (L)

An inductor is a dynamic element whose voltage depends on the rate of change of current. It requires an additional variable for the inductor current, `i_L`.

-   **Connections**: `n1`, `n2`
-   **Value**: Inductance `L`
-   **Extra Variable**: Let `k` be the index of the new variable `i_L`.

### DC Analysis

In DC analysis, an inductor is a short circuit. The stamp enforces `v(n1) - v(n2) = 0`.

-   **MNA Stamp**:

```
A[n1][k] += 1
A[n2][k] -= 1
A[k][n1] += 1
A[k][n2] -= 1
```

### Transient Analysis

Using the selected **integration method**, the inductor is modeled as a resistance `R_eq` in series with a voltage source `V_eq`.

#### General Formulation

Using integration coefficients from the `IntegrationMethod` interface:

-   **Equivalent Resistance**: `R_eq = β₀ · L / h`
-   **History Voltage**: `V_eq = (β₁ · L / h) · i_{L,n-1} + (β₂ · L / h) · i_{L,n-2}`

For Trapezoidal rule, also include the previous voltage: `V_eq += v_{n-1}`

#### Integration Method Coefficients for Inductor

| Method | β₀ | β₁ | β₂ | History includes v_{n-1}? |
|--------|-----|-----|-----|---------------------------|
| **Backward Euler** | 1 | 1 | 0 | No |
| **Trapezoidal** | 2 | 2 | 0 | Yes |
| **Gear (BDF2)** | 3/2 | 2 | -1/2 | No |

#### Backward Euler (Default)

-   **Equivalent Resistance**: `R_eq = L / h`
-   **Equivalent Voltage Source**: `V_eq = (L / h) · i_L_{n-1}`

##### Derivation (Backward Euler)
The inductor's behavior is described by $v(t) = L \frac{di(t)}{dt}$. Using the Backward Euler approximation:
$$
\frac{di_L(t_n)}{dt} \approx \frac{i_L(t_n) - i_L(t_{n-1})}{h}
$$
Substituting:
$$
v_n = L \frac{i_{L,n} - i_{L,n-1}}{h}
$$
Rearranging into MNA form:
$$
v_n(n1) - v_n(n2) - \underbrace{\left(\frac{L}{h}\right)}_{R_{eq}} i_{L,n} = - \underbrace{\left(\frac{L}{h}\right) i_{L, n-1}}_{V_{eq}}
$$

#### Trapezoidal Rule

-   **Equivalent Resistance**: `R_eq = 2L / h`
-   **Equivalent Voltage Source**: `V_eq = (2L / h) · i_{L,n-1} + v_{n-1}`

-   **MNA Stamp**:

```
A[n1][k] += 1
A[n2][k] -= 1
A[k][n1] += 1
A[k][n2] -= 1
A[k][k]  -= R_eq  // from equivalent resistance
z[k]     -= V_eq  // from history voltage
```

## 6. Shockley Diode

A diode is a non-linear component. Its model is linearized at each iteration of the Newton-Raphson algorithm. The linearized model consists of a conductance `g_eq` in parallel with a current source `I_eq`.

-   **Connections**: `n1` (anode), `n2` (cathode)
-   **Model**: Shockley diode equation: $I_d = f(V_d) = I_s (e^{V_d / (n V_t)} - 1)$
-   **Linearization at iteration `k`**:
    -   `V_d^k = v^k(n1) - v^k(n2)`
    -   `g_eq = dI_d / dV_d` evaluated at `V_d^k`
    -   `I_eq = I_d(V_d^k) - g_eq * V_d^k`

#### Derivation
For the Newton-Raphson method, we linearize the non-linear function $I_d = f(V_d)$ around the operating point of the previous iteration, $V_d^k$. Using a first-order Taylor expansion:
$$ I_d^{k+1} \approx f(V_d^k) + f'(V_d^k)(V_d^{k+1} - V_d^k) $$
Let the derivative $f'(V_d^k)$ be the equivalent conductance $g_{eq}$.
$$ I_d^{k+1} \approx I_d^k + g_{eq}(V_d^{k+1} - V_d^k) $$
Rearranging this gives the equation for a linearized equivalent circuit:
$$ I_d^{k+1} = g_{eq}V_d^{k+1} + \underbrace{(I_d^k - g_{eq}V_d^k)}_{I_{eq}} $$
This shows the diode is modeled as a conductance $g_{eq}$ in parallel with a current source $I_{eq}$ for the $(k+1)$-th iteration. The value of $I_{eq}$ is calculated using values from the $k$-th iteration.

- **MNA Stamp** (for the Jacobian and RHS):

```
// Jacobian part (conductance)
A[n1][n1] += g_eq
A[n2][n2] += g_eq
A[n1][n2] -= g_eq
A[n2][n1] -= g_eq

// RHS part (current source)
z[n1] -= I_eq
z[n2] += I_eq
```

## 7. MOSFET

A MOSFET is a non-linear component with four terminals: drain (d), gate (g), source (s), and bulk (b). It is also linearized for the Newton-Raphson solver.

### DC Analysis

The linearized model includes the drain-source current `I_ds` and its derivatives: transconductance (`gm`), output conductance (`gds`), and body-effect transconductance (`gmbs`).

-   **Derivatives at iteration `k`**:
    -   `gm = dI_ds / dV_gs`
    -   `gds = dI_ds / dV_ds`
    -   `gmbs = dI_ds / dV_bs`
-   **Equivalent RHS Current**: `I_eq = I_ds^k - gm*v_gs^k - gds*v_ds^k - gmbs*v_bs^k`

#### Derivation
The MOSFET drain current $I_{ds} = f(V_{gs}, V_{ds}, V_{bs})$ is a non-linear function of its terminal voltages. We linearize it using a multi-variable Taylor expansion around the voltages from the previous iteration ($V_{gs}^k, V_{ds}^k, V_{bs}^k$):
$$
I_{ds}^{k+1} \approx I_{ds}^k + \frac{\partial I_{ds}}{\partial V_{gs}}\bigg|_k (V_{gs}^{k+1} - V_{gs}^k) + \frac{\partial I_{ds}}{\partial V_{ds}}\bigg|_k (V_{ds}^{k+1} - V_{ds}^k) + \frac{\partial I_{ds}}{\partial V_{bs}}\bigg|_k (V_{bs}^{k+1} - V_{bs}^k) 
$$
Substituting the small-signal parameters $g_m, g_{ds}, g_{mbs}$:
$$
I_{ds}^{k+1} \approx I_{ds}^k + g_m(V_{gs}^{k+1} - V_{gs}^k) + g_{ds}(V_{ds}^{k+1} - V_{ds}^k) + g_{mbs}(V_{bs}^{k+1} - V_{bs}^k)
$$
The MNA formulation requires separating terms for the current iteration $(k+1)$ on the left side (Jacobian) and previous iteration $(k)$ on the right side (RHS).
$$
I_{ds}^{k+1} - g_m V_{gs}^{k+1} - g_{ds} V_{ds}^{k+1} - g_{mbs} V_{bs}^{k+1} = \underbrace{I_{ds}^k - g_m V_{gs}^k - g_{ds} V_{ds}^k - g_{mbs} V_{bs}^k}_{I_{eq}}
$$
The terms on the left form the Jacobian stamp, while the terms on the right are collected into the equivalent current source $I_{eq}$ for the RHS vector.

- **MNA Stamp** (for the Jacobian and RHS):

```
// Jacobian (conductance) stamp
A[d][d] += gds
A[s][s] += gds + gm + gmbs
A[d][s] -= gds + gm + gmbs
A[s][d] -= gds

A[d][g] += gm
A[s][g] -= gm

A[d][b] += gmbs
A[s][b] -= gmbs

// RHS (current) stamp
z[d] -= I_eq
z[s] += I_eq
```

### Transient Analysis

For transient analysis, the MOSFET's non-linear capacitances (`Cgs`, `Cgd`, etc.) must be included. Each capacitance is treated like the linear capacitor: it is converted to a DC equivalent model (conductance + current source) at each time step, and its contribution is added to the MNA system. The values of these capacitances and their derivatives `dQ/dV` are updated at each Newton-Raphson iteration.
