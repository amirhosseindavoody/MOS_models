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

// Add a node to the circuit.
// Returns node index, or -1 on error.
// Note: node index is not the same as variable index. Variable indices are
//       assigned during finalization and only apply to non-ground nodes.
// Note: Node names must be unique (except for ground which can be "0", "gnd",
//       "GND", "ground").
int CircuitAddNode(Circuit* c, const char* name);

// Get node index by name. Returns -1 if not found.
int CircuitGetNode(Circuit* c, const char* name);

// Get variable index for a given node index. Returns -1 if invalid node index
int CircuitGetVarIndex(Circuit* c, int node_index);

// Add a device to the circuit. Returns the device pointer on success, nullptr
// on error.
Device* CircuitAddDevice(Circuit* c, Device* d);

// Finalize the circuit: assign variable indices and prepare for analysis
// Returns 0 on success, -1 on error
int CircuitFinalize(Circuit* c);

// PascalCase aliases for consumers expecting that style
inline Circuit* CircuitCreate() { return circuit_create(); }
inline void CircuitFree(Circuit* c) { circuit_free(c); }

// =============================================================================
// Analysis Functions
// =============================================================================

// Perform DC analysis using Newton-Raphson iteration
int CircuitDcAnalysis(Circuit* c, double* x, int max_iter, double tol_abs,
                      double tol_rel);

// Print circuit summary (number of nodes, devices, variables, etc.)
void CircuitPrintSummary(Circuit* c);

// Print solution vector (node voltages and extra variables)
void CircuitPrintSolution(Circuit* c, double* x);

}  // namespace minispice

#endif  // MINI_SPICE_CIRCUIT_H_
