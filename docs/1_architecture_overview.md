# Mini-SPICE Architecture Overview

## Executive Summary

Mini-SPICE is an educational circuit simulator designed to perform DC and transient analyses on circuits containing linear and nonlinear components. The simulator is structured around the **Modified Nodal Analysis (MNA)** formulation, which systematically converts circuit topology and device physics into a system of linear or nonlinear algebraic equations. The architecture emphasizes simplicity, testability, and clear separation of concerns between circuit topology, device modeling, equation assembly, and numerical solving.

---

## 1. High-Level Architecture

```
┌─────────────────────────────────────────────────────────┐
│                 User Interface / Netlist                │
│                   (SPICE-like format)                   │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│            Netlist Parser & Circuit Builder             │
│  • Parse component definitions and connections          │
│  • Build in-memory circuit representation               │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│                  Circuit Representation                 │
│  • Node list and indices                                │
│  • Device list with connectivity                        │
│  • Extra variable allocation (V-sources, inductors)     │
└────────────────────┬────────────────────────────────────┘
                     │
        ┌────────────┴────────────┬───────────────┐
        │                         │               │
┌───────▼────────┐    ┌──────────▼──────┐  ┌─────▼──────┐
│  DC Analysis   │    │ Transient       │  │  Future:   │
│  (NR Newton-   │    │ Analysis        │  │  • AC      │
│   Raphson)     │    │ • Time stepping │  │  • Noise   │
│  • Linear &    │    │ • Integration   │  │  • etc.    │
│    Nonlinear   │    │ • NR iterations │  │            │
└────┬───────────┘    └────────┬────────┘  └────────────┘
     │                         │
     └────────────┬────────────┘
                  │
       ┌──────────▼──────────┐
       │  Equation Assembly  │
       │   (MNA + Devices)   │
       │  • StampContext     │
       │  • Triplet (COO)    │
       │  • Matrix assembly  │
       └──────────┬──────────┘
                  │
       ┌──────────▼──────────┐
       │  Linear/Nonlinear   │
       │       Solver        │
       │  • CSR/Dense matrix │
       │  • Direct solve     │
       │  • NR iterations    │
       └──────────┬──────────┘
                  │
       ┌──────────▼──────────┐
       │   Output Results    │
       │  • Node voltages    │
       │  • Device currents  │
       │  • Time series data │
       └─────────────────────┘
```

---

## 2. Core Concepts

### 2.1 Modified Nodal Analysis (MNA)

MNA is the mathematical foundation of the simulator. It formulates circuit equations as:

$$A \cdot x = z$$

where:
- **$A$** is the system matrix (Jacobian of KCL residual)
- **$x$** is the unknown vector: node voltages + extra branch currents
- **$z$** is the right-hand side: source contributions + history terms

**Key principles:**
- Each circuit node gets one row/column in the system
- Extra variables (inductor currents, voltage source branch currents) get additional rows/columns
- Devices contribute local derivatives (conductances, transconductances) to the global Jacobian
- The MNA formulation is agnostic to device physics — each device type is responsible only for its local contribution

See [mna_stamps.md](mna_stamps.md) and [a_matrix_relationship.md](a_matrix_relationship.md) for detailed mathematical formulations.

### 2.2 Device Stamping

**Stamping** is the process by which each device injects its contribution into the MNA system. Instead of devices directly modifying solver structures, they use a **StampContext** to accumulate:
- **Triplets** (row, col, value) → collected and assembled into the global matrix later
- **RHS contributions** → updates to the `z` vector

This **delayed assembly** approach provides:
- **Decoupling:** Devices don't know about global matrix layout or solver internals
- **Locality:** Each device only computes its local currents and sensitivities
- **Efficiency:** Convert triplets to CSR/dense once per assembly, avoiding random writes

See [stamp_api_design.md](stamp_api_design.md) for the complete API and vtable design.

### 2.3 Device Polymorphism via Vtable

Each device type implements a **virtual function table (vtable)** with six hooks:

| Hook | Purpose |
|------|---------|
| `init()` | One-time initialization; allocate extra variables |
| `stamp_nonlinear()` | Jacobian and RHS for NR iterations (used for all DC analysis) |
| `stamp_transient()` | Time-discretized stamping (reactive devices) |
| `update_state()` | Store history after successful time-step |
| `free()` | Cleanup and memory deallocation |

The simulator kernel loops over all devices, calling the appropriate hook for the current analysis mode. This allows new device types to be added without modifying the core simulator.

---

## 3. Analysis Modes

### 3.1 DC Analysis

**Goal:** Solve the DC operating point of the circuit using Newton–Raphson iteration. This unified approach handles both linear and nonlinear elements.

**Why Newton–Raphson for all DC analysis:**
- For linear circuits, NR converges in one iteration (the Jacobian is constant)
- For nonlinear circuits, NR iterates to the solution
- Single implementation handles both cases, avoiding code duplication

**Flow:**
1. Initialize x (e.g., all zeros or educated guess)
2. **Newton–Raphson loop** (iterate until convergence):
   - Reset `StampContext`
   - For each device: call `stamp_nonlinear()` with current x guess
   - Assemble Jacobian `A` and residual RHS based on device physics
   - Solve $A \cdot \Delta x = -F(x)$ for the Newton step
   - Update $x \gets x + \Delta x$
   - Check convergence: $||\Delta x|| < \epsilon$
3. When converged, output DC operating point

**Supported devices:** Resistor, DC voltage/current sources (linear devices, converge in 1 NR iteration), Diode, MOSFET (nonlinear devices, multiple NR iterations)

**Key concepts:**
- **Jacobian assembly:** Each device contributes $\frac{\partial I}{\partial V}$ (conductance for linear devices, small-signal conductance for nonlinear devices)
- **For linear devices:** The Jacobian is constant, and NR solves the problem exactly in one iteration
- **For nonlinear devices:** The Jacobian changes with voltage, and NR iterates to convergence
- **RHS:** Includes source contributions and history/bias terms needed for linearization accuracy

### 3.2 Transient Analysis

**Goal:** Compute circuit response over time using implicit time-stepping.

**Numerical integration:** Backward Euler (simple) or Trapezoidal (more accurate)
- Converts differential equations into algebraic constraints
- Time discretization: $\frac{dv}{dt} \to \frac{v_n - v_{n-1}}{h}$
- Reactive elements become equivalent resistors + history current sources

**Flow:**
1. Initialize time step and previous state ($x_{n-1}$)
2. **For each time step:**
   - Set up time-step state: `TimeStepState = {t, h, x_prev}`
   - Initialize x guess (e.g., $x_{n-1}$)
   - **NR iteration loop** (nonlinear transient):
     - Reset `StampContext`
     - For each device: call `stamp_transient(ctx, ts)`
     - Assemble `A(x)` and `z(x)`
     - Solve for $\Delta x$; update x
     - Check convergence
   - When converged, for each device: call `update_state(x, ts)` to store history
   - Advance time: $t \gets t + h$; $x_{n-1} \gets x_n$
   - Output time-series point

**Supported devices:** All DC devices + Capacitor, Inductor + nonlinear devices

**Key differences from DC analysis:**
- Reactive devices (capacitor, inductor) generate equivalent conductances and history currents based on time-step size
- State must be updated after each successful time-step to store history (previous voltages, currents)
- History terms couple the current step to previous steps via Backward Euler discretization

---

## 4. Component Architecture

### 4.1 Circuit Representation

**Data structure:**
- **Node map:** Node names → global indices (0 = ground, 1..N = user nodes)
- **Device list:** Ordered list of all components with connectivity
- **Variable index space:**
  - Indices 0 to N_nodes–1: node voltages
  - Indices N_nodes to N_vars–1: extra variables (inductor currents, V-source branch currents)

**Responsibilities:**
- Maintain consistent node indexing
- Allocate extra variable indices during `finalize()`
- Provide device list to simulator kernel

### 4.2 Device Models

Each supported device type implements the vtable interface. Devices contribute to the global system via `StampContext`:

#### Linear DC Devices

| Device | Stamp Type | MNA Contribution |
|--------|----------|------------------|
| **Resistor** | Conductance | $G = 1/R$ into 2×2 stamp |
| **DC Voltage Source** | Extra variable + constraint | Enforces $V_{n1} - V_{n2} = V$ via extra row/col |
| **DC Current Source** | RHS | Adds $\pm I$ to z vector |

#### Nonlinear Devices

| Device | Model | Jacobian | RHS |
|--------|-------|----------|-----|
| **Diode** | Shockley exponential | $g_{eq} = I_s \exp(V_d/(nV_t)) / (nV_t)$ | $I_{eq} = I_d - g_{eq} V_d$ |
| **MOSFET** | Square-law (Level 1) | Transconductance $g_m$, output conductance $g_{ds}$ | $I_{eq} = I_{ds} - g_m V_{gs} - g_{ds} V_{ds}$ |

See [mna_stamps.md](mna_stamps.md) for all device stamps and [minimal_charge_based_mosfet.md](minimal_charge_based_mosfet.md) for MOSFET charge model details.

#### Reactive Devices (Transient)

| Device | Equivalent Model (Backward Euler) | Effect |
|--------|----------------------------------|--------|
| **Capacitor** | Conductance $G_{eq} = C/h$ + history current | Stores voltage history |
| **Inductor** | Resistance $R_{eq} = L/h$ + history voltage | Stores current history; requires extra variable |

### 4.3 StampContext & Assembly

**Purpose:** Collect device contributions without imposing matrix layout requirements.

**Interface:**
- `ctx_add_A(ctx, row, col, val)` — Append triplet
- `ctx_add_z(ctx, idx, val)` — Add RHS
- `ctx_alloc_extra_var()` — Allocate variable index for device use
- `ctx_assemble_matrix()` — Convert triplets (COO) to solver format (CSR or dense)

**Semantics:**
- Multiple calls to `ctx_add_A` for the same (row, col) **accumulate**
- Sign conventions must be respected (current injections vs. extractions)
- Assembly is **non-destructive** unless explicitly reset

See [stamp_api_design.md](stamp_api_design.md) for complete API semantics.

### 4.4 Solvers

The simulator uses pluggable solver interfaces for flexibility:

**Linear Solver:**
- Direct: LAPACK (dense), CSR-based (sparse)
- Input: Matrix $A$ (CSR), vector $z$
- Output: Solution $x$

**Nonlinear Solver (Newton–Raphson):**
- Wrapper around linear solver
- Manages iteration loop and convergence checking
- Computes NR step: $\Delta x = A^{-1} \cdot (-F(x))$

---

## 5. Workflow Examples

### 5.1 DC Analysis: Resistive Divider

```
Circuit: Vdd=10V -> R1=1k -> R2=1k -> GND
```

**Steps:**
1. Parse netlist → 3 nodes (Vdd, mid, GND) + 2 resistors + 1 voltage source
2. Finalize circuit → Node indices: GND=0, mid=1, Vdd=2; 1 extra var (V-source current)
3. Initialize NR: $x = [0, 5, 10, 0]$ (guess)
4. **NR Iteration 1:**
   - Resistor 1: `stamp_nonlinear()` → $g = 0.001$, constant Jacobian contribution
   - Resistor 2: `stamp_nonlinear()` → $g = 0.001$, constant Jacobian contribution
   - Voltage source: `stamp_nonlinear()` → enforce $V_{dd} = 10$
   - Assemble: $A$ matrix, $z$
   - Solve for $\Delta x$ (should be zero for linear circuit)
   - Check convergence: $||\Delta x|| < \epsilon$ → **CONVERGED in 1 iteration**
5. Output: $V_{mid} = 5V$, source current = 10mA

**Note:** Linear circuits converge in 1 NR iteration because the Jacobian is constant.

### 5.2 DC Analysis: Diode Circuit

```
Circuit: Vdd=5V -> R=1k -> Diode -> GND
```

**Steps:**
1. Parse and finalize: Nodes (Vdd, anode, GND); 1 resistor, 1 diode, 1 V-source
2. Initialize NR: $x = [0, 0.6, 5]$ (guess diode anode ~0.6V, Vdd=5V)
3. **NR Iteration 1:**
   - Resistor: `stamp_nonlinear()` → conductance $g = 0.001$ (constant)
   - Diode: `stamp_nonlinear()` → compute $I_d$ and $g_{eq}$ for current x guess
     - If $V_d = 0.6V$, then $I_d \approx I_s e^{0.6/(nV_t)}$
     - Linearized conductance: $g_{eq} = dI_d/dV_d = (I_s/(nV_t))e^{0.6/(nV_t)}$
   - Voltage source: enforce $V_{dd} = 5V$
   - Assemble Jacobian and RHS
   - Solve for $\Delta x$
   - Check convergence: $||\Delta x|| < \epsilon$
4. **NR Iteration 2, 3, ...:** Continue until converged (typically 3-5 iterations for diodes)
5. Output: Node voltages, diode current, power dissipation

**Note:** Nonlinear devices require multiple NR iterations because their Jacobian (conductance) changes with voltage.

### 5.3 Transient Analysis: RC Step Response

```
Circuit: Vsrc (PWL: 0→5V at t=0) -> R -> C -> GND
```

**Steps:**
1. Parse and finalize: Nodes (source, junction, GND); capacitor uses no extra var
2. Initialize: $x = [0, 0, 0]$, $h = 1\mu s$, $t = 0$, $x_{prev} = [0, 0, 0]$
3. **Time-step loop (t = 0, 1, 2, ... T_end):**
   - Set time-step state: $h$, $t$, $x_{prev}$
   - Initialize NR: $x = x_{prev}$
   - **NR iteration loop:**
     - Resistor: `stamp_nonlinear()` → adds conductance
     - Capacitor: `stamp_nonlinear()` → adds $G_{eq} = C/h$ and history RHS $I_{eq} = G_{eq}(v_{prev})$
     - Voltage source: enforces $V_{src}(t)$ (time-varying if PWL, SIN, etc.)
     - Assemble and solve for $\Delta x$
     - Check convergence: $||\Delta x|| < \epsilon$
   - When converged:
     - Capacitor: `update_state()` → stores $v_{prev,new} = x_{capacitor}$
     - Advance $t \gets t + h$; $x_{prev} \gets x$
     - Record $(t, x)$ for output
4. Plot voltage rise curve: exponential charging with time constant $RC \approx 1 \mu s$

---

## 6. Key Design Principles

### 6.1 Separation of Concerns

- **Devices** know only their physics; they stamp to a context
- **StampContext** knows only how to collect triplets; it's solver-agnostic
- **Solver** knows matrix format and linear algebra; it's device-agnostic
- **Simulator kernel** orchestrates the flow without hardcoding device types

**Benefit:** Easy to add new devices, solvers, or analyses without touching other layers.

### 6.2 Locality

Each device contributes only local currents and derivatives. The global system is built bottom-up by summing local contributions. This enables:
- **Parallelization:** Thread-local stamps can be merged later
- **Modularity:** Device implementation is independent of circuit size
- **Testability:** Unit-test each device in isolation

### 6.3 Determinism and Reproducibility

- Triplet-based assembly produces deterministic results (no floating-point ordering ambiguity if COO → CSR is stable)
- Clear sign conventions and RHS semantics
- Verbose logging of assembled matrix for debugging

### 6.4 C-style API for Portability

- Pure C function pointers (vtable) avoid C++ overhead and language-specific ABI issues
- Structs with opaque pointers allow C++ implementation (RAII, STL) under the hood
- API remains stable across different implementation languages

---

## 7. Development Roadmap (Phases)

### Phase 1: Core DC Linear Simulator
- Netlist parser (R, V, I, L, C cards)
- Circuit representation and node indexing
- MNA matrix assembly (linear devices only)
- Linear solver interface + LAPACK backend
- Unit tests for simple resistive circuits

### Phase 2: Nonlinear DC Analysis
- Newton–Raphson iteration framework
- Diode model (Shockley equation)
- MOSFET Level 1 model (square-law)
- Jacobian assembly from nonlinear devices
- DC operating point tests

### Phase 3: Transient Analysis
- Time-stepping loop and Backward Euler integration
- Capacitor and inductor transient models
- State management and history tracking
- Time-varying sources (PWL, SIN, PULSE)
- Transient response tests (RC, RL, RLC)

### Phase 4: Refinement & Extensions
- Improved netlist parser (.MODEL, .PARAM, etc.)
- Additional solvers (sparse direct, iterative)
- Diode charge model (junction capacitance)
- MOSFET charge-based model (see [minimal_charge_based_mosfet.md](minimal_charge_based_mosfet.md))
- AC analysis, noise analysis (future)

---

## 8. Testing Strategy

### Unit Tests
- **Device stamps:** Verify each device's triplets and RHS for known inputs
- **StampContext:** Test accumulation, assembly, sign conventions
- **Solver interface:** Linear system solve for small known circuits

### Integration Tests
- **DC linear:** Simple resistive dividers, Thevenin equivalents
- **DC nonlinear:** Diode I-V curve, MOSFET transfer curve
- **Transient:** RC charging/discharging, LC oscillation, RLC damping

### Regression Tests
- Benchmark circuits with known results (e.g., SPICE comparison)
- Performance profiling for large circuits

---

## 9. Related Documentation

- **[mna_stamps.md](mna_stamps.md)** — Detailed MNA matrix and RHS stamps for each device type
- **[a_matrix_relationship.md](a_matrix_relationship.md)** — Mathematical relationship between device Jacobians and global MNA matrix
- **[stamp_api_design.md](stamp_api_design.md)** — Complete C-style API for stamping, context, and vtable design
- **[minimal_charge_based_mosfet.md](minimal_charge_based_mosfet.md)** — Detailed MOSFET model equations and charge partitioning
- **[development_plan.md](development_plan.md)** — Project goals and development phases

---

## 10. Glossary

| Term | Definition |
|------|-----------|
| **MNA** | Modified Nodal Analysis; formulation of circuit equations as $A \cdot x = z$ |
| **Stamp** | Contribution of a device to the MNA matrix and RHS |
| **StampContext** | Arena for collecting triplets and RHS updates; assembly happens after all devices stamp |
| **Triplet** | (row, col, value) tuple; accumulated and converted to CSR/dense format |
| **Vtable** | Virtual function table; C-style polymorphism for device types |
| **Jacobian** | Matrix of partial derivatives; for MNA, $J_{ij} = \partial I_i / \partial x_j$ |
| **Backward Euler** | Implicit time-integration method; simple and stable for stiff systems |
| **Newton–Raphson** | Iterative method to solve $F(x)=0$; requires Jacobian |
| **Extra variable** | Additional unknown (e.g., inductor current, voltage source branch current) |
| **Convergence** | NR iteration stops when $\||\Delta x\|| < \epsilon$; transient completes when both NR and time-stepping converge |

---

**Document Version:** 1.0  
**Last Updated:** December 2025  
**Author:** Mini-SPICE Development Team
