# Stamp & MNA API Design for mini-spice (C/C++)

This design document defines the C/C++ API and data structures for device stamping, MNA assembly, solver interfaces, and transient control used by mini-spice.

**Goals**
- Provide a minimal, testable C API that maps cleanly to a C++ implementation.
- Keep devices responsible for local calculations only; they push contributions into a StampContext.
- Make stamping deterministic and easy to unit-test.

**File:** [2_stamp_api_design.md](2_stamp_api_design.md)

**Key Concepts**
- **Stamp:** device contributions to the MNA matrix `A` and RHS `z` for the current analysis step/iteration.
- **StampContext:** arena used during assembly; devices append triplets and RHS updates to it.
- **Variable indexing:** global indexing of node voltages plus extra branch variables (voltage-source currents, inductor currents).
- **Device lifecycle hooks:** init, stamp_nonlinear (for all DC analysis), stamp_transient (for transient), update_state, free.
- **Unified DC approach:** Newton-Raphson iteration is used for both linear and nonlinear DC circuits; linear devices have constant Jacobians and converge in 1 iteration.

**Top-level Data Types**
- `Circuit` - holds node map, device list, and `num_vars`.
- `Device` - polymorphic device with a vtable of stamping hooks.
- `StampContext` - collects triplets and RHS updates during assembly.
- `Triplet` - (row, col, value) used to build sparse matrix.
- `Matrix` - assembled CSR (or dense for small circuits).
- `Solver` - pluggable linear solver interface.
- `IterationState`, `TimeStepState` - context for NR iterations and time-stepping.

**C-style API Prototypes**

```c
// core typedefs
typedef struct Circuit Circuit;
typedef struct Device Device;
typedef struct StampContext StampContext;
typedef struct IterationState IterationState;
typedef struct TimeStepState TimeStepState;

// triplet used while assembling
typedef struct {
  int row;
  int col;
  double val;
} Triplet;

// StampContext helpers
StampContext *ctx_create(int num_vars);
void ctx_free(StampContext *ctx);
void ctx_reset(StampContext *ctx); // clear collected triplets and z
void ctx_add_A(StampContext *ctx, int row, int col, double val);
void ctx_add_z(StampContext *ctx, int idx, double val);

// during initial circuit setup devices request extra variable indices
int ctx_alloc_extra_var(StampContext *ctx);

// convert triplets -> dense matrix for solver use (see src/stamp.h)
// In the implementation the helper exposed is:
//   void ctx_assemble_dense(StampContext *ctx, double* matrix);
// which assembles into a preallocated row-major dense matrix.
void ctx_assemble_dense(StampContext *ctx, double* matrix);
```

## StampContext: role & semantics

`StampContext` is the central assembly arena used during one MNA assembly pass (one DC build, one NR iteration, or one transient timestep). Its responsibilities and expected behavior are:

- **Collect contributions:** devices append local contributions as triplets via `ctx_add_A` and push RHS updates with `ctx_add_z`. The context accumulates these into an internal COO-style list (triplets) and an RHS vector sized to `num_vars`.
- **Delayed assembly:** `ctx_add_A` does not immediately modify a global matrix layout or solver data structure — it simply records the (row, col, value) triple. After all devices have stamped, the caller converts the triplets to the solver format by calling `ctx_assemble_matrix` (COO -> CSR/dense).
- **Non-destructive until assemble:** calling `ctx_assemble_matrix` should not invalidate the logical contents of the `StampContext` unless explicitly documented; callers may `ctx_reset` to clear contents for the next assembly.
- **Atomic accumulation:** repeated calls to `ctx_add_A` / `ctx_add_z` for the same indices accumulate (i.e., they add values). Devices should freely call these helpers — the context handles summation.
- **Indices are global variable indices:** `row` and `col` passed to `ctx_add_A`, and the `idx` passed to `ctx_add_z`, MUST be global variable indices in the range `[0, num_vars-1]`. For node voltages the Circuit provides these indices to devices (e.g., via `Device->nodes[]` mapped to variable indices). Ground is typically assigned a fixed index (commonly 0) and is handled consistently by the circuit (devices may skip stamping contributions to ground if using a reduced indexing scheme).
- **Sign conventions:** `ctx_add_z` follows the MNA RHS sign convention used throughout this document: currents injected into the node are added/subtracted according to the device's stamp definition. For example, a current source from `n1` to `n2` with value `I` should call `ctx_add_z(n1_idx, -I); ctx_add_z(n2_idx, +I);` to produce the correct sign in the assembled RHS. Device implementations are responsible for applying the sign conventions shown in the device stamps in this doc.
- **Thread-safety / parallel stamping:** `StampContext` implementations are not required to be thread-safe. If you parallelize stamping, use thread-local `StampContext` buffers per worker and merge their triplet lists into the main context prior to assembly.
- **Performance note:** collecting contributions as triplets avoids many random writes into sparse solver memory. After stamping, convert to CSR once per assembly and hand the CSR to the solver.

### Helper semantics — `ctx_add_A` and `ctx_add_z`

- `void ctx_add_A(StampContext *ctx, int row, int col, double val)`
  - Appends (row, col, val) to the internal triplet list. The caller must ensure `0 <= row < ctx->num_vars` and `0 <= col < ctx->num_vars`.
  - Values are additive: multiple calls for the same (row,col) are allowed and will be summed when the matrix is assembled.
  - Use this for both Jacobian/J matrix entries (during NR) and for linear conductance stamps (during DC/transient).

- `void ctx_add_z(StampContext *ctx, int idx, double val)`
  - Adds `val` to the RHS accumulator at index `idx`. The caller must ensure `0 <= idx < ctx->num_vars`.
  - This function is used to add source/current contributions and history current terms (e.g., capacitor I_eq) into the RHS vector.

### Minimal assembly usage example

```c
ctx_reset(ctx); // clear previous triplets and RHS
for each device in circuit {
    device->vt->stamp_dc(device, ctx); // or stamp_nonlinear / stamp_transient
}
Matrix *A = ctx_assemble_matrix(ctx); // produces CSR or dense matrix
solver->solve(solver, A, ctx->z, x);
```

Devices should never assume the solver's internal matrix layout — they only interact with `StampContext` via `ctx_add_A` and `ctx_add_z`.


## Device vtable: Role and Design

The **vtable (virtual function table)** is a C-style mechanism for achieving **polymorphism** without using C++. It allows the simulator to work with many different device types (resistors, capacitors, transistors, diodes, etc.) through a single generic `Device` interface, with each device type providing its own implementation of stamping, initialization, and state management.

### Why vtable?

- **Polymorphic behavior:** Different devices implement the same interface but with completely different physics. A resistor's stamp is purely ohmic; a diode's is exponential; a MOSFET's involves multiple coupled equations. The vtable lets the core simulator loop call `device->vt->stamp_dc(device, ctx)` without knowing the specific device type.
- **Extensibility:** New device types (e.g., BJT, JFET, voltage-controlled sources) can be added by defining a new vtable and device struct without modifying the simulator kernel.
- **C-compatible:** This design uses plain C function pointers rather than C++ virtual methods, making it portable and suitable for embedding in other tools.

### The Device vtable interface

Note: the canonical vtable as implemented in the headers uses PascalCase symbols. See `src/device.h`. The actual C++ header defines the vtable with these exact names and signatures:

```c
/* Device vtable as declared in src/device.h (exact symbol names) */
struct DeviceVTable {
  void (*Init)(Device* d, Circuit* c);
  void (*StampNonlinear)(Device* d, StampContext* ctx, IterationState* it);
  void (*StampTransient)(Device* d, StampContext* ctx, TimeStepState* ts);
  void (*UpdateState)(Device* d, double* x, TimeStepState* ts);
  void (*Free)(Device* d);
};

struct Device {
  const DeviceVTable* vt;
  int nodes[4];
  void* params;
  void* state;
  int extra_var;
};
```

This document retains the C-style helper names in examples (e.g., `ctx_add_A`, `ctx_add_z`), but the device vtable symbols in the implementation are PascalCase (as above).

### Vtable methods explained

Each function pointer in the vtable defines a lifecycle hook:

- **`Init(Device *d, Circuit *c)`**
  - Called once after the device is added to the circuit but before any analysis begins.
  - Allows the device to initialize internal state, validate parameters, and allocate extra variables if needed (e.g., voltage source branch current, inductor current).
  - Example: a voltage source calls `ctx_alloc_extra_var()` to obtain its extra variable index for the branch current.

- **`StampNonlinear(Device *d, StampContext *ctx)`**
  - Called during DC analysis to push the device's contributions to the MNA matrix and RHS. For nonlinear devices this is used inside Newton–Raphson iterations.
  - Used for linear devices (resistor, linear current/voltage sources) and for DC biasing of nonlinear devices.
  - Example: a resistor with value R and nodes n1, n2 stamps the conductance 1/R into the matrix.

- **`StampNonlinear(Device *d, StampContext *ctx, IterationState *it)`**
  - Called during each Newton–Raphson iteration of a DC nonlinear analysis or transient solve.
  - Allows the device to compute its operating point for the current guess vector (stored in `it->x_current`) and stamp the Jacobian (linearized conductance) and nonlinear RHS contribution.
  - Example: a diode computes its exponential current and small-signal conductance around the current voltage estimate.

- **`StampTransient(Device *d, StampContext *ctx, TimeStepState *ts)`**
  - Called during each transient time-step to stamp contributions that include time-derivative or history terms.
  - Used for reactive devices (capacitors, inductors) and implicit time-stepping schemes (Backward Euler, trapezoidal).
  - Example: a capacitor stamps an equivalent conductance and current source based on the previous step's voltage and the time-step size.

- **`UpdateState(Device *d, double *x, TimeStepState *ts)`**
  - Called after a successful transient time-step solution to allow the device to update its internal state with the new solution vector `x`.
  - Used to store history needed for the next time-step (previous voltages, currents, flux linkages).
  - Example: a capacitor stores `v_prev = x[n1] - x[n2]` for the next step; an inductor stores `i_prev`.

- **`Free(Device *d)`**
  - Called when the device is destroyed (circuit cleanup) to deallocate any device-specific memory (params, state).

### Calling convention in the simulator kernel

The simulator kernel typically loops over all devices and calls the appropriate vtable method:

```c
// DC analysis (linear or NR)
for (Device *d = circuit->devices; d != NULL; d = d->next) {
    d->vt->StampNonlinear(d, ctx);
}

// NR iteration during transient
for (Device *d = circuit->devices; d != NULL; d = d->next) {
    d->vt->StampNonlinear(d, ctx, iteration_state);
}

// After converged transient step
for (Device *d = circuit->devices; d != NULL; d = d->next) {
    d->vt->UpdateState(d, x, time_step_state);
}
```

### Device implementation example (pseudocode)

A resistor implementation provides:

```c
struct ResistorParams {
  double R; // resistance in ohms
};

struct ResistorState {
  // resistor is memoryless; no state needed
};

static void resistor_init(Device *d, Circuit *c) {
  // nothing to do; R is stored in params
}

static void resistor_stamp_dc(Device *d, StampContext *ctx) {
  ResistorParams *p = (ResistorParams *)d->params;
  double g = 1.0 / p->R;
  int n1 = d->nodes[0], n2 = d->nodes[1];
  ctx_add_A(ctx, n1, n1, +g);
  ctx_add_A(ctx, n2, n2, +g);
  ctx_add_A(ctx, n1, n2, -g);
  ctx_add_A(ctx, n2, n1, -g);
}

static void resistor_stamp_nonlinear(Device *d, StampContext *ctx, IterationState *it) {
  // identical to DC for linear device
  resistor_stamp_dc(d, ctx);
}

static void resistor_stamp_transient(Device *d, StampContext *ctx, TimeStepState *ts) {
  // identical to DC for memoryless device
  resistor_stamp_dc(d, ctx);
}

static void resistor_update_state(Device *d, double *x, TimeStepState *ts) {
  // nothing to update; resistor is memoryless
}

static void resistor_free(Device *d) {
  free(d->params);
  free(d->state);
  free(d);
}

static const DeviceVTable resistor_vt = {
  .init = resistor_init,
  .stamp_dc = resistor_stamp_dc,
  .stamp_nonlinear = resistor_stamp_nonlinear,
  .stamp_transient = resistor_stamp_transient,
  .update_state = resistor_update_state,
  .free = resistor_free,
};
```

### Design benefits

- **Separation of concerns:** Each device type concentrates its physics logic in one module; the simulator kernel remains generic and agnostic.
- **Easy testing:** Device implementations can be unit-tested in isolation by creating a minimal circuit and calling their vtable methods directly.
- **Flexible device types:** Linear, nonlinear, and reactive devices coexist; each provides only the stamping behavior it needs.
- **Memory efficiency:** Devices store only the state they require (resistor state is empty; capacitor stores voltage history; etc.).

**Device vtable (C-style)**

```c
typedef struct DeviceVTable {
  void (*init)(Device *d, Circuit *c);
  void (*stamp_nonlinear)(Device *d, StampContext *ctx, IterationState *it); // used for all DC analysis
  void (*stamp_transient)(Device *d, StampContext *ctx, TimeStepState *ts);    // used for transient analysis
  void (*update_state)(Device *d, double *x, TimeStepState *ts);
  void (*free)(Device *d);
} DeviceVTable;

struct Device {
  const DeviceVTable *vt;
  int nodes[4]; // generic terminal indices: use -1 for unused terminals
  void *params; // device-specific parameters
  void *state;  // device-specific state (history, prev voltages, prev currents)
  int extra_var; // index of extra variable if allocated
};
```

**Circuit API (high-level)**

```c
Circuit *circuit_create();
int circuit_add_node(Circuit *c, const char *name); // returns node index
Device *circuit_add_device(Circuit *c, Device *d); // register device
void circuit_finalize(Circuit *c); // assigns node indices and extra vars
```

**Solver interface**

The simulator supports multiple linear solver backends:

```c
typedef enum {
    SOLVER_DIRECT_DENSE,    // LAPACK (dgesv) - for small circuits
    SOLVER_DIRECT_SPARSE,   // KLU, UMFPACK, SuperLU - for medium circuits
    SOLVER_ITERATIVE        // GMRES, BiCGSTAB - for large circuits
} SolverType;

typedef struct Solver Solver;
struct Solver {
  SolverType type;
  // Core solve: factor and solve in one call
  int (*solve)(Solver *s, Matrix *A, double *rhs, double *x); // returns 0 on success
  
  // Optional: separate factorization (for NR where Jacobian reuse is possible)
  int (*factor)(Solver *s, Matrix *A);              // factor matrix A
  int (*solve_factored)(Solver *s, double *rhs, double *x);  // solve with existing factor
  
  // Cleanup
  void (*free)(Solver *s);
};

// Factory functions for different backends
Solver *solver_create_dense();                              // LAPACK backend
Solver *solver_create_sparse_direct();                      // KLU/UMFPACK backend  
Solver *solver_create_iterative(double tol, int max_iter);  // GMRES/BiCGSTAB
```

**Iterative solver notes:**
- Iterative solvers require good preconditioners for circuit matrices
- Common preconditioners: ILU(0), diagonal, block-diagonal
- May fail to converge for ill-conditioned matrices; fall back to direct solver

**Iteration & Time-step state**

```c
struct IterationState {
  int iter;
  double *x_current; // current NR guess (length = num_vars)
  double tol_rel;
  double tol_abs;
};

struct TimeStepState {
  double t;
  double h; // timestep
  double *x_prev;   // previous step solution
  double *x_prev2;  // second previous step (for multi-step methods like Gear)
  const IntegrationMethod *im;  // current integration method
};
```

**Integration Method Interface**

The `IntegrationMethod` abstraction allows the simulator to support different time-discretization schemes. Devices query integration coefficients via this interface:

```c
typedef struct IntegrationMethod IntegrationMethod;
struct IntegrationMethod {
    const char *name;           // "backward_euler", "trapezoidal", "gear2"
    int order;                  // 1 for BE, 2 for Trap/Gear2
    
    // Capacitor discretization: i = alpha0 * C/h * v_n - I_history
    double alpha0;              // coefficient for current voltage
    double alpha1;              // coefficient for v_{n-1}
    double alpha2;              // coefficient for v_{n-2} (multi-step only)
    
    // Inductor discretization: v = beta0 * L/h * i_n - V_history
    double beta0;               // coefficient for current current
    double beta1;               // coefficient for i_{n-1}
    double beta2;               // coefficient for i_{n-2} (multi-step only)
    
    int required_history;       // 1 for BE/Trap, 2 for Gear
};

// Predefined integration methods
extern const IntegrationMethod BACKWARD_EULER;   // alpha0=1, alpha1=1
extern const IntegrationMethod TRAPEZOIDAL;      // alpha0=2, alpha1=2 (+ current history)
extern const IntegrationMethod GEAR2;            // alpha0=3/2, alpha1=2, alpha2=-1/2
```

**Stamping examples (pseudocode)**

- Resistor (R between n1,n2):
  - g = 1.0 / R
  - ctx_add_A(ctx,n1,n1,+g); ctx_add_A(ctx,n2,n2,+g);
  - ctx_add_A(ctx,n1,n2,-g); ctx_add_A(ctx,n2,n1,-g);

- Current source (I from n1->n2):
  - ctx_add_z(ctx,n1, -I);
  - ctx_add_z(ctx,n2, +I);

- Voltage source (V between n1,n2):
  - During init: d->extra_var = ctx_alloc_extra_var(ctx)
  - k = d->extra_var
  - ctx_add_A(ctx,n1,k,+1); ctx_add_A(ctx,n2,k,-1);
  - ctx_add_A(ctx,k,n1,+1); ctx_add_A(ctx,k,n2,-1);
  - ctx_add_z(ctx,k, V);

- Capacitor (C between n1,n2) - **using integration method**:
  - Get coefficients from ts->im
  - G_eq = ts->im->alpha0 * C / h
  - I_eq = (ts->im->alpha1 * C / h) * v_prev + (ts->im->alpha2 * C / h) * v_prev2
  - For Trapezoidal: also add i_prev to history term
  - stamp like resistor with G_eq and add RHS contributions using I_eq

- Inductor (L between n1,n2) - **using integration method**:
  - Get coefficients from ts->im
  - R_eq = ts->im->beta0 * L / h
  - V_eq = (ts->im->beta1 * L / h) * i_prev + (ts->im->beta2 * L / h) * i_prev2
  - stamp KCL/KVL rows and A[k][k] -= R_eq; z[k] -= V_eq

- Diode (nonlinear) - NR iteration:
  - Vd = x[n1] - x[n2]
  - Id = Is*(exp(Vd/(nVt)) - 1)
  - g_eq = (Is/(nVt))*exp(Vd/(nVt))
  - I_eq = Id - g_eq*Vd
  - stamp conductance g_eq and RHS I_eq

- MOSFET (d,g,s,b) - NR iteration:
  - evaluate I_ds and partial derivatives gm,gds,gmbs
  - form Jacobian entries per the MNA stamp
  - I_eq = I_ds - gm*Vgs - gds*Vds - gmbs*Vbs
  - add I_eq to RHS (z[d] -= I_eq; z[s] += I_eq)

**Assembly and solution flows**

- DC analysis (Newton-Raphson, unified for linear and nonlinear):
  1. initialize x (e.g., zeros)
  2. loop until converge:
     - ctx_reset
    - for each device: StampNonlinear(device, ctx, it)
     - assemble A, z (Jacobian and residual RHS)
     - solve for delta_x (A * delta_x = -F)
     - update x
  3. Note: linear circuits converge in 1 iteration; nonlinear circuits iterate until convergence

- Transient (Backward Euler with Newton-Raphson inner loop):
  1. initialize TimeStepState with x_prev
  2. for each time step:
     - ts.h = h; ts.t = t
     - initialize x guess (x_prev)
     - iterate NR (Newton-Raphson loop):
       - ctx_reset
       - for each device: stamp_transient(device, ctx, ts)
       - assemble A, z (includes reactive devices' equivalent conductance + history terms)
       - solve for delta_x
       - update x
       - check convergence
     - on converge: for each device call update_state(device, x, ts) to store history
     - x_prev = x; advance time

**Testing matrix & validation**
- Unit tests for each device stamp (resistor, cap, inductor, diode, mosfet) using small harness that creates a StampContext and checks collected triplets and z.
- Integration tests: DC resistive divider, DC source network, diode DC bias, MOSFET small-signal, RC transient step response, RL transient.

**Notes & Implementation recommendations**
- Build triplets per device and convert to CSR once per assembly to reduce random writes.
- Keep ground node index fixed at 0.
- Use double precision and provide diagnostic verbose mode to dump assembled triplets.
- Consider using a small C++ wrapper (RAII for memory) but keep C API stable for portability.

---

If you want, I can now:
- Add corresponding header files (`include/stamp.h`, `include/device.h`) and a small C++ implementation skeleton.
- Implement a minimal working prototype for DC linear solve (parser + resistor + V/I sources + CSR assembly + solver).

Which next step should I take? 
