
// StampContext and core data structures for MNA assembly
// This header defines the StampContext API for device stamping and the
// core data structures used throughout mini-spice.

#ifndef MINI_SPICE_STAMP_H_
#define MINI_SPICE_STAMP_H_

#include <stddef.h>

namespace minispice {

// Triplet structure for COO-style matrix assembly
// Used internally by StampContext to collect contributions before
// converting to a dense or CSR matrix format.
struct Triplet {
  int row;
  int col;
  double val;
};

// Forward declarations
struct StampContext;
struct Circuit;
struct Device;

// State passed to devices during Newton-Raphson iterations
struct IterationState {
  int iter;          /**< Current iteration number (0-based) */
  double* x_current; /**< Current solution guess (length = num_vars) */
  double tol_rel;    /**< Relative convergence tolerance */
  double tol_abs;    /**< Absolute convergence tolerance */
};

// Integration method coefficients for time discretization
// Devices use these coefficients to compute equivalent conductances and history
// terms during transient analysis.
struct IntegrationMethod {
  const char* name; /**< Method name (e.g., "backward_euler") */
  int order;        /**< Integration order (1 for BE, 2 for Trap/Gear) */

  /* Capacitor coefficients: i_n = (alpha0*C/h)*v_n - I_history */
  double alpha0; /**< Coefficient for current voltage */
  double alpha1; /**< Coefficient for v_{n-1} */
  double alpha2; /**< Coefficient for v_{n-2} (multi-step only) */

  /* Inductor coefficients: v_n = (beta0*L/h)*i_n - V_history */
  double beta0; /**< Coefficient for current current */
  double beta1; /**< Coefficient for i_{n-1} */
  double beta2; /**< Coefficient for i_{n-2} (multi-step only) */

  int required_history; /**< Number of history steps needed */
};

/**
 * @brief State passed to devices during transient time-stepping
 */
struct TimeStepState {
  double t;        /**< Current simulation time */
  double h;        /**< Time step size */
  double* x_prev;  /**< Solution from previous time step */
  double* x_prev2; /**< Solution from two steps ago (for multi-step) */
  const IntegrationMethod* im; /**< Current integration method */
};

/**
 * @brief Predefined integration methods
 */
extern const IntegrationMethod kBackwardEuler;
extern const IntegrationMethod kTrapezoidal;
extern const IntegrationMethod kGear2;
inline const IntegrationMethod& BACKWARD_EULER = kBackwardEuler;
inline const IntegrationMethod& TRAPEZOIDAL = kTrapezoidal;
inline const IntegrationMethod& GEAR2 = kGear2;

/* ============================================================================
 * StampContext API
 * ============================================================================
 */

/**
 * @brief Create a new StampContext for the given number of variables
 *
 * @param num_vars Total number of MNA variables (node voltages + extra vars)
 * @return Pointer to new StampContext, or nullptr on failure
 */
StampContext* CtxCreate(int num_vars);

/**
 * @brief Free a StampContext and all associated memory
 */
void CtxFree(StampContext* ctx);

/**
 * @brief Reset the StampContext for a new assembly pass
 *
 * Clears all accumulated triplets and resets the RHS vector to zero.
 * Call this before each new matrix assembly (each NR iteration or time step).
 */
void CtxReset(StampContext* ctx);

/**
 * @brief Add a value to the MNA matrix at position (row, col)
 *
 * Values are accumulated: multiple calls to the same (row, col) will sum.
 *
 * @param ctx The StampContext
 * @param row Row index (0 <= row < num_vars)
 * @param col Column index (0 <= col < num_vars)
 * @param val Value to add to A[row][col]
 */
void CtxAddA(StampContext* ctx, int row, int col, double val);

/**
 * @brief Add a value to the RHS vector at position idx
 *
 * Values are accumulated: multiple calls to the same idx will sum.
 *
 * @param ctx The StampContext
 * @param idx Index into RHS vector (0 <= idx < num_vars)
 * @param val Value to add to z[idx]
 */
void CtxAddZ(StampContext* ctx, int idx, double val);

/**
 * @brief Allocate an extra variable for branch currents
 *
 * Used by voltage sources and inductors that require extra MNA variables.
 *
 * @param ctx The StampContext
 * @return Index of the newly allocated variable
 */
int CtxAllocExtraVar(StampContext* ctx);

/**
 * @brief Get the number of variables
 */
int CtxGetNumVars(StampContext* ctx);

/**
 * @brief Get direct access to triplet array (for assembly)
 *
 * @param ctx The StampContext
 * @param count Output: number of triplets
 * @return Pointer to triplet array
 */
const Triplet* CtxGetTriplets(StampContext* ctx, size_t* count);

/**
 * @brief Get direct access to RHS vector
 *
 * @param ctx The StampContext
 * @return Pointer to RHS vector (length = num_vars)
 */
double* CtxGetZ(StampContext* ctx);

/**
 * @brief Assemble triplets into a dense matrix
 *
 * Converts accumulated triplets into a row-major dense matrix.
 *
 * @param ctx The StampContext
 * @param matrix Output: preallocated num_vars x num_vars matrix (row-major)
 */
void CtxAssembleDense(StampContext* ctx, double* matrix);

// ----------------------------------------------------------------------------
// C-style aliases (snake_case) for compatibility with tests and docs
// ----------------------------------------------------------------------------
inline StampContext* ctx_create(int num_vars) { return CtxCreate(num_vars); }
inline void ctx_free(StampContext* ctx) { CtxFree(ctx); }
inline void ctx_reset(StampContext* ctx) { CtxReset(ctx); }
inline void ctx_add_A(StampContext* ctx, int row, int col, double val) {
  CtxAddA(ctx, row, col, val);
}
inline void ctx_add_z(StampContext* ctx, int idx, double val) {
  CtxAddZ(ctx, idx, val);
}
inline int ctx_alloc_extra_var(StampContext* ctx) {
  return CtxAllocExtraVar(ctx);
}
inline int ctx_get_num_vars(StampContext* ctx) { return CtxGetNumVars(ctx); }
inline const Triplet* ctx_get_triplets(StampContext* ctx, size_t* count) {
  return CtxGetTriplets(ctx, count);
}
inline double* ctx_get_z(StampContext* ctx) { return CtxGetZ(ctx); }
inline void ctx_assemble_dense(StampContext* ctx, double* matrix) {
  CtxAssembleDense(ctx, matrix);
}

}  // namespace minispice

#endif  // MINI_SPICE_STAMP_H_
