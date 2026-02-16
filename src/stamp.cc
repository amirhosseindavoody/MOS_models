// StampContext implementation for MNA assembly
// Implements the StampContext API defined in stamp.h. The context collects
// triplets (COO format) during device stamping and can assemble them into
// a dense matrix for solving.

#include "stamp.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace minispice {

// ============================================================================
// Internal representation of StampContext
// ============================================================================

struct StampContext {
  // Number of variables (including extra vars for voltage sources, inductors,
  // etc.)
  int num_vars_;

  // Number of extra variables allocated (for tracking)
  int num_extra_allocated_;

  // Internal storage for triplets and RHS vector
  std::vector<Triplet> triplets_;

  // RHS vector (length = num_vars_)
  std::vector<double> z_;
};

// ============================================================================
// Predefined Integration Methods
// ============================================================================

const IntegrationMethod kBackwardEuler = {.name = "backward_euler",
                                          .order = 1,
                                          .alpha0 = 1.0,
                                          .alpha1 = 1.0,
                                          .alpha2 = 0.0,
                                          .beta0 = 1.0,
                                          .beta1 = 1.0,
                                          .beta2 = 0.0,
                                          .required_history = 1};

const IntegrationMethod kTrapezoidal = {.name = "trapezoidal",
                                        .order = 2,
                                        .alpha0 = 2.0,
                                        .alpha1 = 2.0,
                                        .alpha2 = 0.0,
                                        .beta0 = 2.0,
                                        .beta1 = 2.0,
                                        .beta2 = 0.0,
                                        .required_history = 1};

const IntegrationMethod kGear2 = {.name = "gear2",
                                  .order = 2,
                                  .alpha0 = 1.5,
                                  .alpha1 = 2.0,
                                  .alpha2 = -0.5,
                                  .beta0 = 1.5,
                                  .beta1 = 2.0,
                                  .beta2 = -0.5,
                                  .required_history = 2};

// ============================================================================
// StampContext API Implementation
// ============================================================================

StampContext* CtxCreate(int num_vars) {
  if (num_vars <= 0) {
    return nullptr;
  }

  StampContext* ctx = new (std::nothrow) StampContext;
  if (!ctx) {
    return nullptr;
  }

  ctx->num_vars_ = num_vars;
  ctx->num_extra_allocated_ = 0;
  ctx->triplets_.reserve(64);  // Initial capacity for typical small circuits
  ctx->z_.resize(num_vars, 0.0);

  return ctx;
}

void CtxFree(StampContext* ctx) {
  if (ctx) {
    delete ctx;
  }
}

void CtxReset(StampContext* ctx) {
  if (!ctx) return;

  ctx->triplets_.clear();
  std::fill(ctx->z_.begin(), ctx->z_.end(), 0.0);
}

void CtxAddA(StampContext* ctx, int row, int col, double val) {
  if (!ctx) return;
  if (row < 0 || row >= ctx->num_vars_) return;
  if (col < 0 || col >= ctx->num_vars_) return;
  if (val == 0.0) return;  // Skip zero entries for efficiency

  ctx->triplets_.push_back({row, col, val});
}

void CtxAddZ(StampContext* ctx, int idx, double val) {
  if (!ctx) return;
  if (idx < 0 || idx >= ctx->num_vars_) return;

  ctx->z_[idx] += val;
}

int CtxAllocExtraVar(StampContext* ctx) {
  if (!ctx) return -1;

  // This function is called during circuit setup to allocate extra variables.
  // The caller must ensure the context was created with enough initial capacity
  // or resize the context accordingly.
  int new_idx = ctx->num_vars_ + ctx->num_extra_allocated_;
  ctx->num_extra_allocated_++;

  // Resize z vector to accommodate new variable
  ctx->z_.resize(new_idx + 1, 0.0);
  ctx->num_vars_ = new_idx + 1;

  return new_idx;
}

int CtxGetNumVars(StampContext* ctx) {
  if (!ctx) return 0;
  return ctx->num_vars_;
}

const Triplet* CtxGetTriplets(StampContext* ctx, size_t* count) {
  if (!ctx) {
    if (count) *count = 0;
    return nullptr;
  }

  if (count) *count = ctx->triplets_.size();
  return ctx->triplets_.data();
}

double* CtxGetZ(StampContext* ctx) {
  if (!ctx) return nullptr;
  return ctx->z_.data();
}

void CtxAssembleDense(StampContext* ctx, double* matrix) {
  if (!ctx || !matrix) return;

  int n = ctx->num_vars_;

  // Zero the matrix
  std::memset(matrix, 0, n * n * sizeof(double));

  // Accumulate triplets into dense matrix (row-major order)
  for (const auto& t : ctx->triplets_) {
    matrix[t.row * n + t.col] += t.val;
  }
}

}  // namespace minispice
