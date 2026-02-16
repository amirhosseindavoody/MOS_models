// Circuit representation and DC analysis implementation
//
// Implements the Circuit API including node management, device registration,
// and DC analysis using Newton-Raphson iteration.
//

#include "circuit.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "device.h"

namespace minispice {

namespace {
// =============================================================================
// Internal Utilities
// =============================================================================

static bool IsGroundName(const char* name) {
  if (!name) return false;
  if (strcmp(name, "0") == 0) return true;
  if (strcasecmp(name, "gnd") == 0) return true;
  if (strcasecmp(name, "ground") == 0) return true;
  return false;
}

// Simple dense linear solver using Gaussian elimination with partial pivoting
static int solve_dense_system(int n, double* A, double* b, double* x) {
  // A is row-major n x n, b and x are length n
  // Uses in-place elimination on copies

  // Work arrays - copy A and b
  double* M = (double*)malloc(n * n * sizeof(double));
  double* rhs = (double*)malloc(n * sizeof(double));
  if (!M || !rhs) {
    free(M);
    free(rhs);
    return -1;
  }

  memcpy(M, A, n * n * sizeof(double));
  memcpy(rhs, b, n * sizeof(double));

  // Gaussian elimination with partial pivoting
  for (int k = 0; k < n; k++) {
    // Find pivot
    int p = k;
    double maxv = fabs(M[k * n + k]);
    for (int i = k + 1; i < n; i++) {
      double v = fabs(M[i * n + k]);
      if (v > maxv) {
        maxv = v;
        p = i;
      }
    }

    if (maxv < 1e-15) {
      // Singular matrix
      free(M);
      free(rhs);
      return -2;
    }

    // Swap rows if needed
    if (p != k) {
      for (int j = 0; j < n; j++) {
        double tmp = M[k * n + j];
        M[k * n + j] = M[p * n + j];
        M[p * n + j] = tmp;
      }
      double tmp = rhs[k];
      rhs[k] = rhs[p];
      rhs[p] = tmp;
    }

    // Eliminate below pivot
    double pivot = M[k * n + k];
    for (int i = k + 1; i < n; i++) {
      double factor = M[i * n + k] / pivot;
      for (int j = k; j < n; j++) {
        M[i * n + j] -= factor * M[k * n + j];
      }
      rhs[i] -= factor * rhs[k];
    }
  }

  // Back substitution
  for (int i = n - 1; i >= 0; i--) {
    double sum = rhs[i];
    for (int j = i + 1; j < n; j++) {
      sum -= M[i * n + j] * x[j];
    }
    x[i] = sum / M[i * n + i];
  }

  free(M);
  free(rhs);
  return 0;
}

}  // namespace

// ============================================================================
// Circuit API Implementation
// ============================================================================

Circuit* circuit_create(void) {
  Circuit* c = (Circuit*)calloc(1, sizeof(Circuit));
  if (!c) return nullptr;

  // Initial capacity for nodes
  c->nodes_capacity = 16;
  c->nodes = (Node*)calloc(c->nodes_capacity, sizeof(Node));
  if (!c->nodes) {
    free(c);
    return nullptr;
  }

  // Add ground node at index 0
  strcpy(c->nodes[0].name, "0");
  c->nodes[0].var_index = -1;  // Ground is not a variable
  c->num_nodes = 1;

  c->devices = nullptr;
  c->num_devices = 0;
  c->num_vars = 0;
  c->num_extra_vars = 0;
  c->finalized = 0;

  return c;
}

void circuit_free(Circuit* c) {
  if (!c) return;

  // Free all devices
  Device* d = c->devices;
  while (d) {
    Device* next = d->next;
    DeviceFree(d);
    d = next;
  }

  free(c->nodes);
  free(c);
}

int CircuitAddNode(Circuit* c, const char* name) {
  if (c == nullptr || name == nullptr) {
    return -1;
  }

  // Cannot add nodes after finalization
  if (c->finalized) {
    return -1;
  }

  // Check for ground
  if (IsGroundName(name)) {
    return 0;  // Ground is always index 0
  }

  // Check if node already exists
  for (int i = 0; i < c->num_nodes; i++) {
    if (strcmp(c->nodes[i].name, name) == 0) {
      return i;
    }
  }

  // Need to add new node - check capacity
  if (c->num_nodes >= c->nodes_capacity) {
    int new_cap = c->nodes_capacity * 2;
    Node* new_nodes = (Node*)realloc(c->nodes, new_cap * sizeof(Node));
    if (!new_nodes) return -1;
    c->nodes = new_nodes;
    c->nodes_capacity = new_cap;
  }

  // Add the node
  int idx = c->num_nodes;
  strncpy(c->nodes[idx].name, name, kMaxNodeNameLen - 1);
  c->nodes[idx].name[kMaxNodeNameLen - 1] = '\0';
  c->nodes[idx].var_index = -1;  // Will be assigned during finalize
  c->num_nodes++;

  return idx;
}

int CircuitGetNode(Circuit* c, const char* name) {
  if (!c || !name) return -1;

  if (IsGroundName(name)) {
    return 0;
  }

  for (int i = 0; i < c->num_nodes; i++) {
    if (strcmp(c->nodes[i].name, name) == 0) {
      return i;
    }
  }

  return -1;
}

int CircuitGetVarIndex(Circuit* c, int node_index) {
  if (!c) return -1;
  if (node_index < 0 || node_index >= c->num_nodes) return -1;

  return c->nodes[node_index].var_index;
}

Device* CircuitAddDevice(Circuit* c, Device* d) {
  if (!c || !d) return nullptr;
  if (c->finalized) return nullptr;

  // Add to front of linked list
  d->next = c->devices;
  c->devices = d;
  c->num_devices++;

  return d;
}

int CircuitFinalize(Circuit* c) {
  if (!c) return -1;
  if (c->finalized) return 0;  // Already finalized

  // Initial pass to assign variable indices to nodes and node variables
  int var_idx = 0;
  for (int i = 0; i < c->num_nodes; i++) {
    if (i == 0) {
      c->nodes[i].var_index = -1;  // Ground
    } else {
      c->nodes[i].var_index = var_idx++;
    }
  }

  c->num_vars = var_idx;
  if (c->num_vars <= 0) {
    return -1;
  }

  c->num_extra_vars = 0;

  // Second pass to initialize devices and count extra variables needed
  for (Device* d = c->devices; d; d = d->next) {
    // Initialize device (may set extra_var to -2 to request allocation)
    if (d->vt && d->vt->Init) {
      d->vt->Init(d, c);
    }

    // Allocate extra variable if requested
    if (d->extra_var == -2) {
      d->extra_var = c->num_vars + c->num_extra_vars;
      c->num_extra_vars++;
    }
  }

  c->num_vars += c->num_extra_vars;

  c->finalized = 1;
  return 0;
}

int CircuitDcAnalysis(Circuit* c, double* x, int max_iter, double tol_abs,
                      double tol_rel) {
  if (!c || !x) return -1;
  if (!c->finalized) return -1;

  int n = c->num_vars;
  if (n == 0) return -1;

  // Allocate working arrays
  double* A = (double*)calloc(n * n, sizeof(double));
  double* z = (double*)calloc(n, sizeof(double));
  double* x_new = (double*)calloc(n, sizeof(double));
  double* delta = (double*)calloc(n, sizeof(double));

  if (!A || !z || !x_new || !delta) {
    free(A);
    free(z);
    free(x_new);
    free(delta);
    return -1;
  }

  // Initialize solution guess to zero
  memset(x, 0, n * sizeof(double));

  StampContext* ctx = CtxCreate(n);
  if (!ctx) {
    free(A);
    free(z);
    free(x_new);
    free(delta);
    return -1;
  }

  IterationState it;
  it.tol_abs = tol_abs;
  it.tol_rel = tol_rel;

  int iter;
  for (iter = 0; iter < max_iter; iter++) {
    it.iter = iter;
    it.x_current = x;

    // Reset and stamp
    CtxReset(ctx);

    for (Device* d = c->devices; d; d = d->next) {
      if (d->vt && d->vt->StampNonlinear) {
        d->vt->StampNonlinear(d, ctx, &it);
      }
    }

    // Assemble matrix
    memset(A, 0, n * n * sizeof(double));
    CtxAssembleDense(ctx, A);

    // Get RHS
    double* rhs = CtxGetZ(ctx);
    memcpy(z, rhs, n * sizeof(double));

    // Solve A * x_new = z
    int solve_result = solve_dense_system(n, A, z, x_new);
    if (solve_result != 0) {
      fprintf(stderr, "DC analysis: solver failed at iteration %d\n", iter);
      CtxFree(ctx);
      free(A);
      free(z);
      free(x_new);
      free(delta);
      return -1;
    }

    // Check convergence
    double max_delta = 0.0;
    for (int i = 0; i < n; i++) {
      delta[i] = x_new[i] - x[i];
      double abs_delta = fabs(delta[i]);
      if (abs_delta > max_delta) max_delta = abs_delta;
    }

    // Update solution
    memcpy(x, x_new, n * sizeof(double));

    // For linear circuits, first iteration gives exact solution
    // Check if converged
    bool converged = true;
    for (int i = 0; i < n; i++) {
      double threshold = tol_abs + tol_rel * fabs(x[i]);
      if (fabs(delta[i]) > threshold) {
        converged = false;
        break;
      }
    }

    if (converged || iter == 0) {
      // For linear circuits, converge after first iteration
      iter++;
      break;
    }
  }

  CtxFree(ctx);
  free(A);
  free(z);
  free(x_new);
  free(delta);

  return iter;
}

void CircuitPrintSummary(Circuit* c) {
  if (!c) return;

  printf("Circuit Summary:\n");
  printf("  Nodes: %d (including ground)\n", c->num_nodes);
  printf("  Devices: %d\n", c->num_devices);
  printf("  MNA Variables: %d\n", c->num_vars);
  printf("  Extra Variables: %d\n", c->num_extra_vars);
  printf("  Finalized: %s\n", c->finalized ? "yes" : "no");
}

void CircuitPrintSolution(Circuit* c, double* x) {
  if (!c || !x) return;
  if (!c->finalized) return;

  printf("DC Solution:\n");

  // Print node voltages
  for (int i = 0; i < c->num_nodes; i++) {
    if (i == 0) {
      printf("  V(%s) = 0.000000 V  [ground]\n", c->nodes[i].name);
    } else {
      int vi = c->nodes[i].var_index;
      if (vi >= 0 && vi < c->num_vars) {
        printf("  V(%s) = %.6f V\n", c->nodes[i].name, x[vi]);
      }
    }
  }

  // Print extra variable values (branch currents)
  int extra_idx = c->num_vars - c->num_extra_vars;
  for (Device* d = c->devices; d; d = d->next) {
    if (d->extra_var >= 0) {
      printf("  I(%s) = %.6f A\n", d->name, x[d->extra_var]);
    }
  }
}
}  // namespace minispice