// Device vtable interface for polymorphic device stamping
// This header defines the Device structure and vtable interface that
// allows different device types to contribute to the MNA system through
// a common polymorphic interface.

#ifndef MINI_SPICE_DEVICE_H_
#define MINI_SPICE_DEVICE_H_

#include "stamp.h"

namespace minispice {

// Device vtable - polymorphic interface for all device types
struct DeviceVTable {
  // Initialize the device after circuit setup
  void (*Init)(Device* d, Circuit* c);

  // Stamp contributions for DC/nonlinear analysis
  void (*StampNonlinear)(Device* d, StampContext* ctx, IterationState* it);

  // Stamp contributions for transient analysis
  void (*StampTransient)(Device* d, StampContext* ctx, TimeStepState* ts);

  // Update device state after a converged time step
  void (*UpdateState)(Device* d, double* x, TimeStepState* ts);

  // Free device-specific memory
  void (*Free)(Device* d);
};

// Generic device structure
struct Device {
  const DeviceVTable* vt;  // Pointer to device's vtable
  char name[32];           // Device name (e.g., "R1", "V1")
  int nodes[4];            // Terminal variable indices (-1 for unused)
  void* params;            // Device-specific parameters
  void* state;             // Device-specific state (history, etc.)
  int extra_var;           // Extra variable index (for V-sources, L)
                  // Note: extra_var is -1 if not used, or the index of the
                  // extra variable in the MNA system if used
                  // extra_var is set to -2 during initialization to request
                  // allocation of an extra variable by circuit_finalize
  Device* next;  // Next device in circuit's linked list
};

// ============================================================================
// Device Factory Functions
// ============================================================================

// Create a resistor device
Device* CreateResistor(const char* name, int n1, int n2, double resistance);

// Create a DC current source
Device* CreateCurrentSource(const char* name, int n1, int n2, double current);

// Create a DC voltage source
Device* CreateVoltageSource(const char* name, int n1, int n2, double voltage);

// Create a capacitor
Device* CreateCapacitor(const char* name, int n1, int n2, double capacitance);

// Create an inductor
Device* CreateInductor(const char* name, int n1, int n2, double inductance);

// Create a Shockley diode
Device* CreateDiode(const char* name, int n_anode, int n_cathode, double I_s,
                    double n);

// ============================================================================
// Device Utility Functions
// ============================================================================

// Free a device and its associated memory
void DeviceFree(Device* d);

}  // namespace minispice

#endif  // MINI_SPICE_DEVICE_H_
