# Mini-SPICE Agent Instructions

This document provides instructions for AI coding assistants helping with the development of **mini-spice**, an educational C/C++ circuit simulator.

---

## Project Overview

**Mini-SPICE** is an educational circuit simulator designed to teach the fundamentals of SPICE-like circuit solvers. The project implements:

- **Modified Nodal Analysis (MNA)** for equation formulation
- **Newton-Raphson iteration** for both linear and nonlinear DC analysis
- **Transient analysis** with pluggable integration methods (Backward Euler, Trapezoidal, Gear/BDF2)
- **Device models** for linear (R, L, C, V, I) and nonlinear (Diode, MOSFET) components

### Educational Goals

The primary purpose is to provide a clear, readable implementation that demonstrates:
1. How circuit topology maps to matrix equations via MNA
2. How device physics contribute "stamps" to the global system
3. How Newton-Raphson iteration solves nonlinear systems
4. How time-discretization converts differential equations to algebraic ones

**Simplicity and clarity are prioritized over performance optimizations.**

---

## Repository Structure

```
mini-spice/
├── docs/                           # Design documentation
│   ├── 0_development_plan.md       # Development roadmap and phases
│   ├── 1_architecture_overview.md  # High-level architecture
│   ├── 2_stamp_api_design.md       # C-style API for device stamping
│   ├── 3_mna_stamps.md             # MNA matrix stamps for each device
│   ├── 4_a_matrix_relationship.md  # Math behind Jacobian and MNA
│   └── minimal_charge_based_mosfet.md  # MOSFET model derivation
├── src/                            # C/C++ source code
│   ├── CMakeLists.txt
│   └── dc_linear.cc                # Current working prototype
├── examples/                       # Example netlists and scripts
├── scripts/                        # Utility scripts
├── build/                          # CMake build directory
├── CMakeLists.txt                  # Root CMake configuration
├── pixi.toml                       # Pixi package manager config
└── README.md
```

---

## Core Concepts

### Modified Nodal Analysis (MNA)

The simulator solves the system `A · x = z` where:
- **A** = Jacobian matrix (device conductances and coupling terms)
- **x** = unknowns (node voltages + extra branch currents)
- **z** = RHS vector (source currents, history terms)

### Device Stamping

Each device contributes to the global MNA system via a **stamp**:
- Devices push triplets `(row, col, value)` to a `StampContext`
- The context accumulates contributions and assembles the matrix
- This decouples device physics from solver implementation

### Newton-Raphson Iteration

For **all DC analysis** (linear and nonlinear):
1. Linear circuits: NR converges in 1 iteration (constant Jacobian)
2. Nonlinear circuits: NR iterates until convergence

This unified approach avoids separate code paths for linear vs. nonlinear.

### Time Integration Methods

For transient analysis, reactive elements (C, L) use integration methods:
- **Backward Euler**: Simple, first-order, A-stable
- **Trapezoidal**: Second-order, may ring with discontinuities
- **Gear (BDF2)**: Second-order, good for stiff circuits

Devices query integration coefficients from `IntegrationMethod` interface.

---

## Coding Guidelines

### Language and Style

- **Primary language**: C++ (C++17 standard)
- **API style**: C-compatible interfaces (function pointers, opaque structs)
- **Build system**: CMake (3.30+)
- **Testing**: Google Test framework

### Code Style Preferences

```cpp
// Use descriptive names
double equivalent_conductance = capacitance / time_step;

// Prefer explicit over clever
for (int i = 0; i < num_devices; i++) {
    devices[i]->vt->stamp_nonlinear(devices[i], ctx, it);
}

// Document mathematical relationships
// i_n = (C/h) * v_n - (C/h) * v_{n-1}
// G_eq = C/h,  I_eq = G_eq * v_prev
double G_eq = C / h;
double I_eq = G_eq * v_prev;
```

### File Organization

- **Headers**: Place in `include/` directory (when created)
- **Implementation**: Place in `src/` directory
- **Documentation**: Keep design docs in `docs/`
- **Tests**: Co-locate with source or in `test/` directory

### Key Data Structures

Refer to `docs/2_stamp_api_design.md` for the complete API specification:

```cpp
// Triplet for matrix assembly
typedef struct {
    int row;
    int col;
    double val;
} Triplet;

// Device virtual table
typedef struct DeviceVTable {
    void (*init)(Device *d, Circuit *c);
    void (*stamp_nonlinear)(Device *d, StampContext *ctx, IterationState *it);
    void (*stamp_transient)(Device *d, StampContext *ctx, TimeStepState *ts);
    void (*update_state)(Device *d, double *x, TimeStepState *ts);
    void (*free)(Device *d);
} DeviceVTable;

// StampContext for collecting contributions
void ctx_add_A(StampContext *ctx, int row, int col, double val);
void ctx_add_z(StampContext *ctx, int idx, double val);
```

---

## Implementation Guidance

### When Adding a New Device

1. **Create device struct** with params and state
2. **Implement vtable functions** (`init`, `stamp_nonlinear`, `stamp_transient`, `update_state`, `free`)
3. **Use the stamp formulas** from `docs/3_mna_stamps.md`
4. **Write unit tests** that verify triplets and RHS contributions

Example flow for a capacitor:
```cpp
void capacitor_stamp_transient(Device *d, StampContext *ctx, TimeStepState *ts) {
    CapParams *p = (CapParams *)d->params;
    CapState *s = (CapState *)d->state;
    
    // Get integration coefficients
    double alpha0 = ts->im->alpha0;
    double alpha1 = ts->im->alpha1;
    double h = ts->h;
    
    // Calculate equivalent circuit
    double G_eq = alpha0 * p->C / h;
    double I_eq = (alpha1 * p->C / h) * s->v_prev;
    
    // Stamp conductance (like resistor)
    int n1 = d->nodes[0], n2 = d->nodes[1];
    ctx_add_A(ctx, n1, n1, +G_eq);
    ctx_add_A(ctx, n2, n2, +G_eq);
    ctx_add_A(ctx, n1, n2, -G_eq);
    ctx_add_A(ctx, n2, n1, -G_eq);
    
    // Stamp history current
    ctx_add_z(ctx, n1, -I_eq);
    ctx_add_z(ctx, n2, +I_eq);
}
```

### When Modifying the Solver

- The current prototype uses Gaussian elimination with partial pivoting
- Future improvements should add sparse solver support (KLU, UMFPACK)
- Solver abstraction should use the interface in `docs/2_stamp_api_design.md`

### When Working on Transient Analysis

1. Implement `TimeStepState` structure
2. Add `IntegrationMethod` with coefficients for BE, Trap, Gear
3. Modify reactive devices to use integration coefficients
4. Add `update_state()` calls after each converged time step

---

## Testing Strategy

### Unit Tests

Test each device's stamp in isolation:
```cpp
TEST(ResistorTest, StampsCorrectConductance) {
    StampContext *ctx = ctx_create(2);
    Device *r = create_resistor(0, 1, 1000.0);  // 1kΩ
    r->vt->stamp_dc(r, ctx);
    
    // Verify triplets contain g = 0.001
    EXPECT_TRIPLET(ctx, 0, 0, +0.001);
    EXPECT_TRIPLET(ctx, 1, 1, +0.001);
    EXPECT_TRIPLET(ctx, 0, 1, -0.001);
    EXPECT_TRIPLET(ctx, 1, 0, -0.001);
}
```

### Integration Tests

Verify complete circuits against known solutions:
- Resistive divider: Vout = Vin × R2/(R1+R2)
- RC transient: V(t) = V0 × (1 - e^(-t/RC))
- Diode I-V: Verify exponential behavior

---

## Development Phases

Follow the roadmap in `docs/0_development_plan.md`:

### Phase 1: Core DC Simulator (Current Focus)
- [x] Basic netlist parsing (R, V, I)
- [x] Dense matrix assembly
- [x] Gaussian elimination solver
- [ ] StampContext abstraction
- [ ] Device vtable interface
- [ ] Diode and MOSFET models

### Phase 2: Transient Analysis
- [ ] Time-stepping loop
- [ ] Backward Euler integration
- [ ] Capacitor and inductor transient models
- [ ] State history management

### Phase 3: Advanced Integration & Solvers
- [ ] Trapezoidal and Gear methods
- [ ] Sparse solver backends
- [ ] Adaptive time-stepping

### Phase 4: Advanced Models
- [ ] Charge-based MOSFET
- [ ] Junction capacitances
- [ ] Extended netlist syntax

---

## Sign Conventions

**Critical**: Use consistent sign conventions for MNA stamps:

| Current Direction    | Jacobian Entry | RHS Entry |
|---------------------|----------------|-----------|
| Current out of node | +              | -         |
| Current into node   | -              | +         |

For current source `I` flowing from `n1` to `n2`:
```cpp
ctx_add_z(ctx, n1, -I);  // Current leaves n1
ctx_add_z(ctx, n2, +I);  // Current enters n2
```

---

## Mathematical References

### Capacitor (Backward Euler)

$$i_n = \frac{C}{h}(v_n - v_{n-1})$$

Rearranging: $i_n = G_{eq} \cdot v_n - I_{eq}$

where $G_{eq} = C/h$ and $I_{eq} = (C/h) \cdot v_{n-1}$

### Diode (Newton-Raphson Linearization)

$$I_d = I_s(e^{V_d/(nV_t)} - 1)$$

Linearized: $I_d^{k+1} \approx g_{eq} \cdot V_d^{k+1} + I_{eq}$

where:
- $g_{eq} = \frac{I_s}{nV_t} e^{V_d^k/(nV_t)}$
- $I_{eq} = I_d^k - g_{eq} \cdot V_d^k$

---

## Common Pitfalls

1. **Ground node handling**: Ground (node 0) is excluded from the variable vector. Check `node_to_var[n] == -1` before stamping.

2. **Extra variables**: Voltage sources and inductors require extra variables for branch currents. Allocate during `init()`.

3. **Accumulation semantics**: Multiple `ctx_add_A()` calls for the same (row, col) must accumulate, not overwrite.

4. **Convergence criteria**: NR convergence should check both $||\Delta x||$ and $||F(x)||$.

5. **History initialization**: For transient analysis, properly initialize `x_prev` (usually from DC operating point).

---

## Build and Run

### Using Pixi (Recommended)

```bash
pixi run build
pixi run test
```

### Using CMake Directly

```bash
cmake --preset default
cmake --build build
./build/src/dc_linear examples/simple.net
```

---

## Documentation

Refer to these documents for detailed specifications:

| Document | Purpose |
|----------|---------|
| `docs/0_development_plan.md` | Project goals and roadmap |
| `docs/1_architecture_overview.md` | High-level system design |
| `docs/2_stamp_api_design.md` | C API specification |
| `docs/3_mna_stamps.md` | Device stamp formulas |
| `docs/4_a_matrix_relationship.md` | Jacobian mathematics |
| `docs/minimal_charge_based_mosfet.md` | MOSFET model details |

---

## Questions to Consider When Implementing

1. **Does this device need an extra variable?** (V-sources, inductors do)
2. **Is the device linear or nonlinear?** (Affects stamping approach)
3. **Does the device have memory?** (Reactive devices need state storage)
4. **What are the Jacobian entries?** (Partial derivatives of current w.r.t. voltage)
5. **What goes in the RHS?** (Source values, history terms, linearization constants)

---

*Last Updated: January 2026*
