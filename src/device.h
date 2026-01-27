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
  Device* next;            // Next device in circuit's linked list
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

// ----------------------------------------------------------------------------
// C-style aliases (snake_case) to match tests/docs
// ----------------------------------------------------------------------------
inline Device* create_resistor(const char* name, int n1, int n2,
                               double resistance) {
  return CreateResistor(name, n1, n2, resistance);
}
inline Device* create_current_source(const char* name, int n1, int n2,
                                     double current) {
  return CreateCurrentSource(name, n1, n2, current);
}
inline Device* create_voltage_source(const char* name, int n1, int n2,
                                     double voltage) {
  return CreateVoltageSource(name, n1, n2, voltage);
}
inline Device* create_capacitor(const char* name, int n1, int n2,
                                double capacitance) {
  return CreateCapacitor(name, n1, n2, capacitance);
}
inline Device* create_inductor(const char* name, int n1, int n2,
                               double inductance) {
  return CreateInductor(name, n1, n2, inductance);
}
inline Device* create_diode(const char* name, int n_anode, int n_cathode,
                            double I_s, double n) {
  return CreateDiode(name, n_anode, n_cathode, I_s, n);
}
inline void device_free(Device* d) { DeviceFree(d); }

}  // namespace minispice

#endif  // MINI_SPICE_DEVICE_H_
