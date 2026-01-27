// circuit.h
// Circuit representation and management

#ifndef MINI_SPICE_CIRCUIT_H_
#define MINI_SPICE_CIRCUIT_H_

#include "device.h"
#include "stamp.h"

namespace minispice {

// Maximum length for node names
constexpr int kMaxNodeNameLen = 64;

// Node information
struct Node {
  char name[kMaxNodeNameLen];  // Node name (e.g., "1", "out", "gnd")
  int var_index;               // Variable index in MNA system (-1 for ground)
};

// Circuit structure
struct Circuit {
  Node* nodes;         // Array of nodes (index 0 = ground)
  int num_nodes;       // Number of nodes including ground
  int nodes_capacity;  // Allocated capacity for nodes array

  Device* devices;  // Linked list of devices
  int num_devices;  // Number of devices

  int num_vars;        // Total MNA variables (node voltages + extra)
  int num_extra_vars;  // Number of extra variables (V-sources, inductors)
  int finalized;       // 1 if circuit is finalized, 0 otherwise
};

// Circuit Creation and Management

// C-style API (snake_case) - primary implementation
Circuit* circuit_create();
void circuit_free(Circuit* c);
int circuit_add_node(Circuit* c, const char* name);
int circuit_get_node(Circuit* c, const char* name);
int circuit_get_var_index(Circuit* c, int node_index);
Device* circuit_add_device(Circuit* c, Device* d);
int circuit_finalize(Circuit* c);

// PascalCase aliases for consumers expecting that style
inline Circuit* CircuitCreate() { return circuit_create(); }
inline void CircuitFree(Circuit* c) { circuit_free(c); }
inline int CircuitAddNode(Circuit* c, const char* name) {
  return circuit_add_node(c, name);
}
inline int CircuitGetNode(Circuit* c, const char* name) {
  return circuit_get_node(c, name);
}
inline int CircuitGetVarIndex(Circuit* c, int node_index) {
  return circuit_get_var_index(c, node_index);
}
inline Device* CircuitAddDevice(Circuit* c, Device* d) {
  return circuit_add_device(c, d);
}
inline int CircuitFinalize(Circuit* c) { return circuit_finalize(c); }

// =============================================================================
// Analysis Functions
// =============================================================================

// Perform DC analysis using Newton-Raphson iteration
int circuit_dc_analysis(Circuit* c, double* x, int max_iter, double tol_abs,
                        double tol_rel);

// Print circuit summary
void circuit_print_summary(Circuit* c);

// Print solution
void circuit_print_solution(Circuit* c, double* x);

// PascalCase aliases
inline int CircuitDcAnalysis(Circuit* c, double* x, int max_iter,
                             double tol_abs, double tol_rel) {
  return circuit_dc_analysis(c, x, max_iter, tol_abs, tol_rel);
}
inline void CircuitPrintSummary(Circuit* c) { circuit_print_summary(c); }
inline void CircuitPrintSolution(Circuit* c, double* x) {
  circuit_print_solution(c, x);
}

}  // namespace minispice

#endif  // MINI_SPICE_CIRCUIT_H_
