/**
 * @file device.cc
 * @brief Device implementations for Resistor, Current Source, Voltage Source
 */

#include "device.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "circuit.h"

namespace minispice {

// ============================================================================
// Resistor Implementation
// ============================================================================

struct ResistorParams {
  double r;  // Resistance in ohms
};

static void ResistorInit(Device* d, Circuit* c) {
  (void)d;
  (void)c;
}

static void ResistorStampNonlinear(Device* d, StampContext* ctx,
                                   IterationState* it) {
  (void)it;

  ResistorParams* p = static_cast<ResistorParams*>(d->params);
  if (!p || p->r == 0.0) return;

  double g = 1.0 / p->r;
  int n1 = d->nodes[0];
  int n2 = d->nodes[1];

  if (n1 >= 0) CtxAddA(ctx, n1, n1, +g);
  if (n2 >= 0) CtxAddA(ctx, n2, n2, +g);
  if (n1 >= 0 && n2 >= 0) {
    CtxAddA(ctx, n1, n2, -g);
    CtxAddA(ctx, n2, n1, -g);
  }
}

static void ResistorStampTransient(Device* d, StampContext* ctx,
                                   TimeStepState* ts) {
  (void)ts;
  IterationState it = {0, nullptr, 0.0, 0.0};
  ResistorStampNonlinear(d, ctx, &it);
}

static void ResistorUpdateState(Device* d, double* x, TimeStepState* ts) {
  (void)d;
  (void)x;
  (void)ts;
}

static void ResistorFree(Device* d) {
  if (d) {
    free(d->params);
    free(d->state);
    free(d);
  }
}

static const DeviceVTable kResistorVTable = {
    .Init = ResistorInit,
    .StampNonlinear = ResistorStampNonlinear,
    .StampTransient = ResistorStampTransient,
    .UpdateState = ResistorUpdateState,
    .Free = ResistorFree};

// ============================================================================
// Current Source Implementation
// ============================================================================

struct CurrentSourceParams {
  double i;  // Current in amperes
};

static void CurrentSourceInit(Device* d, Circuit* c) {
  (void)d;
  (void)c;
}

static void CurrentSourceStampNonlinear(Device* d, StampContext* ctx,
                                        IterationState* it) {
  (void)it;

  CurrentSourceParams* p = static_cast<CurrentSourceParams*>(d->params);
  if (!p) return;

  double i = p->i;
  int n1 = d->nodes[0];
  int n2 = d->nodes[1];

  if (n1 >= 0) CtxAddZ(ctx, n1, -i);
  if (n2 >= 0) CtxAddZ(ctx, n2, +i);
}

static void CurrentSourceStampTransient(Device* d, StampContext* ctx,
                                        TimeStepState* ts) {
  (void)ts;
  IterationState it = {0, nullptr, 0.0, 0.0};
  CurrentSourceStampNonlinear(d, ctx, &it);
}

static void CurrentSourceUpdateState(Device* d, double* x, TimeStepState* ts) {
  (void)d;
  (void)x;
  (void)ts;
}

static void CurrentSourceFree(Device* d) {
  if (d) {
    free(d->params);
    free(d->state);
    free(d);
  }
}

static const DeviceVTable kCurrentSourceVTable = {
    .Init = CurrentSourceInit,
    .StampNonlinear = CurrentSourceStampNonlinear,
    .StampTransient = CurrentSourceStampTransient,
    .UpdateState = CurrentSourceUpdateState,
    .Free = CurrentSourceFree};

// ============================================================================
// Voltage Source Implementation
// ============================================================================

struct VoltageSourceParams {
  double v;  // Voltage in volts
};

static void VoltageSourceInit(Device* d, Circuit* c) {
  (void)c;
  d->extra_var = -2;
}

static void VoltageSourceStampNonlinear(Device* d, StampContext* ctx,
                                        IterationState* it) {
  (void)it;

  VoltageSourceParams* p = static_cast<VoltageSourceParams*>(d->params);
  if (!p) return;
  if (d->extra_var < 0) return;

  double v = p->v;
  int n1 = d->nodes[0];
  int n2 = d->nodes[1];
  int k = d->extra_var;

  if (n1 >= 0) {
    CtxAddA(ctx, n1, k, +1.0);
    CtxAddA(ctx, k, n1, +1.0);
  }
  if (n2 >= 0) {
    CtxAddA(ctx, n2, k, -1.0);
    CtxAddA(ctx, k, n2, -1.0);
  }
  CtxAddZ(ctx, k, v);
}

static void VoltageSourceStampTransient(Device* d, StampContext* ctx,
                                        TimeStepState* ts) {
  (void)ts;
  IterationState it = {0, nullptr, 0.0, 0.0};
  VoltageSourceStampNonlinear(d, ctx, &it);
}

static void VoltageSourceUpdateState(Device* d, double* x, TimeStepState* ts) {
  (void)d;
  (void)x;
  (void)ts;
}

static void VoltageSourceFree(Device* d) {
  if (d) {
    free(d->params);
    free(d->state);
    free(d);
  }
}

static const DeviceVTable kVoltageSourceVTable = {
    .Init = VoltageSourceInit,
    .StampNonlinear = VoltageSourceStampNonlinear,
    .StampTransient = VoltageSourceStampTransient,
    .UpdateState = VoltageSourceUpdateState,
    .Free = VoltageSourceFree};

// ============================================================================
// Capacitor Implementation
// ============================================================================

struct CapacitorParams {
  double c;  // Capacitance in farads
};

struct CapacitorState {
  double v_prev;
  double v_prev2;
  double i_prev;
};

static void CapacitorInit(Device* d, Circuit* c) {
  (void)c;
  d->state = calloc(1, sizeof(CapacitorState));
}

static void CapacitorStampNonlinear(Device* d, StampContext* ctx,
                                    IterationState* it) {
  (void)d;
  (void)ctx;
  (void)it;
}

static void CapacitorStampTransient(Device* d, StampContext* ctx,
                                    TimeStepState* ts) {
  CapacitorParams* p = static_cast<CapacitorParams*>(d->params);
  CapacitorState* s = static_cast<CapacitorState*>(d->state);
  if (!p || !ts || !ts->im) return;

  double c = p->c;
  double h = ts->h;
  const IntegrationMethod* im = ts->im;

  double g_eq = im->alpha0 * c / h;
  double i_eq = (im->alpha1 * c / h) * s->v_prev;
  if (im->required_history >= 2) {
    i_eq += (im->alpha2 * c / h) * s->v_prev2;
  }

  if (strcmp(im->name, "trapezoidal") == 0) {
    i_eq += s->i_prev;
  }

  int n1 = d->nodes[0];
  int n2 = d->nodes[1];

  if (n1 >= 0) CtxAddA(ctx, n1, n1, +g_eq);
  if (n2 >= 0) CtxAddA(ctx, n2, n2, +g_eq);
  if (n1 >= 0 && n2 >= 0) {
    CtxAddA(ctx, n1, n2, -g_eq);
    CtxAddA(ctx, n2, n1, -g_eq);
  }

  if (n1 >= 0) CtxAddZ(ctx, n1, -i_eq);
  if (n2 >= 0) CtxAddZ(ctx, n2, +i_eq);
}

static void CapacitorUpdateState(Device* d, double* x, TimeStepState* ts) {
  CapacitorState* s = static_cast<CapacitorState*>(d->state);
  CapacitorParams* p = static_cast<CapacitorParams*>(d->params);
  if (!s || !p || !ts) return;

  int n1 = d->nodes[0];
  int n2 = d->nodes[1];

  double v1 = (n1 >= 0) ? x[n1] : 0.0;
  double v2 = (n2 >= 0) ? x[n2] : 0.0;
  double v = v1 - v2;

  if (ts->im && strcmp(ts->im->name, "trapezoidal") == 0) {
    double c = p->c;
    double h = ts->h;
    s->i_prev = (2.0 * c / h) * (v - s->v_prev) - s->i_prev;
  }

  s->v_prev2 = s->v_prev;
  s->v_prev = v;
}

static void CapacitorFree(Device* d) {
  if (d) {
    free(d->params);
    free(d->state);
    free(d);
  }
}

static const DeviceVTable kCapacitorVTable = {
    .Init = CapacitorInit,
    .StampNonlinear = CapacitorStampNonlinear,
    .StampTransient = CapacitorStampTransient,
    .UpdateState = CapacitorUpdateState,
    .Free = CapacitorFree};

// ============================================================================
// Inductor Implementation
// ============================================================================

struct InductorParams {
  double l;  // Inductance in henries
};

struct InductorState {
  double i_prev;
  double i_prev2;
  double v_prev;
};

static void InductorInit(Device* d, Circuit* c) {
  (void)c;
  d->extra_var = -2;
  d->state = calloc(1, sizeof(InductorState));
}

static void InductorStampNonlinear(Device* d, StampContext* ctx,
                                   IterationState* it) {
  (void)it;
  if (d->extra_var < 0) return;

  int n1 = d->nodes[0];
  int n2 = d->nodes[1];
  int k = d->extra_var;

  if (n1 >= 0) {
    CtxAddA(ctx, n1, k, +1.0);
    CtxAddA(ctx, k, n1, +1.0);
  }
  if (n2 >= 0) {
    CtxAddA(ctx, n2, k, -1.0);
    CtxAddA(ctx, k, n2, -1.0);
  }
}

static void InductorStampTransient(Device* d, StampContext* ctx,
                                   TimeStepState* ts) {
  InductorParams* p = static_cast<InductorParams*>(d->params);
  InductorState* s = static_cast<InductorState*>(d->state);
  if (!p || !s || !ts || !ts->im) return;
  if (d->extra_var < 0) return;

  double l = p->l;
  double h = ts->h;
  const IntegrationMethod* im = ts->im;
  int k = d->extra_var;

  double r_eq = im->beta0 * l / h;
  double v_eq = (im->beta1 * l / h) * s->i_prev;
  if (im->required_history >= 2) {
    v_eq += (im->beta2 * l / h) * s->i_prev2;
  }

  if (strcmp(im->name, "trapezoidal") == 0) {
    v_eq += s->v_prev;
  }

  int n1 = d->nodes[0];
  int n2 = d->nodes[1];

  if (n1 >= 0) {
    CtxAddA(ctx, n1, k, +1.0);
    CtxAddA(ctx, k, n1, +1.0);
  }
  if (n2 >= 0) {
    CtxAddA(ctx, n2, k, -1.0);
    CtxAddA(ctx, k, n2, -1.0);
  }
  CtxAddA(ctx, k, k, -r_eq);
  CtxAddZ(ctx, k, -v_eq);
}

static void InductorUpdateState(Device* d, double* x, TimeStepState* ts) {
  InductorState* s = static_cast<InductorState*>(d->state);
  InductorParams* p = static_cast<InductorParams*>(d->params);
  if (!s || !p || !ts) return;
  if (d->extra_var < 0) return;

  int n1 = d->nodes[0];
  int n2 = d->nodes[1];
  int k = d->extra_var;

  double i = x[k];

  if (ts->im && strcmp(ts->im->name, "trapezoidal") == 0) {
    double v1 = (n1 >= 0) ? x[n1] : 0.0;
    double v2 = (n2 >= 0) ? x[n2] : 0.0;
    s->v_prev = v1 - v2;
  }

  s->i_prev2 = s->i_prev;
  s->i_prev = i;
}

static void InductorFree(Device* d) {
  if (d) {
    free(d->params);
    free(d->state);
    free(d);
  }
}

static const DeviceVTable kInductorVTable = {
    .Init = InductorInit,
    .StampNonlinear = InductorStampNonlinear,
    .StampTransient = InductorStampTransient,
    .UpdateState = InductorUpdateState,
    .Free = InductorFree};

// ============================================================================
// Diode Implementation
// ============================================================================

struct DiodeParams {
  double i_s;
  double n;
};

static constexpr double kVT = 0.025852;

static void DiodeInit(Device* d, Circuit* c) {
  (void)d;
  (void)c;
}

static void DiodeStampNonlinear(Device* d, StampContext* ctx,
                                IterationState* it) {
  DiodeParams* p = static_cast<DiodeParams*>(d->params);
  if (!p || !it || !it->x_current) return;

  int n_anode = d->nodes[0];
  int n_cathode = d->nodes[1];

  double v_anode = (n_anode >= 0) ? it->x_current[n_anode] : 0.0;
  double v_cathode = (n_cathode >= 0) ? it->x_current[n_cathode] : 0.0;
  double vd = v_anode - v_cathode;

  double vd_limit = 0.7;
  if (vd > vd_limit) vd = vd_limit;
  if (vd < -15.0 * p->n * kVT) vd = -15.0 * p->n * kVT;

  double n_vt = p->n * kVT;
  double exp_term = exp(vd / n_vt);

  double i_d = p->i_s * (exp_term - 1.0);
  double g_eq = (p->i_s / n_vt) * exp_term;

  if (g_eq < 1e-12) g_eq = 1e-12;

  double i_eq = i_d - g_eq * vd;

  if (n_anode >= 0) CtxAddA(ctx, n_anode, n_anode, +g_eq);
  if (n_cathode >= 0) CtxAddA(ctx, n_cathode, n_cathode, +g_eq);
  if (n_anode >= 0 && n_cathode >= 0) {
    CtxAddA(ctx, n_anode, n_cathode, -g_eq);
    CtxAddA(ctx, n_cathode, n_anode, -g_eq);
  }

  if (n_anode >= 0) CtxAddZ(ctx, n_anode, -i_eq);
  if (n_cathode >= 0) CtxAddZ(ctx, n_cathode, +i_eq);
}

static void DiodeStampTransient(Device* d, StampContext* ctx,
                                TimeStepState* ts) {
  IterationState it = {0, ts->x_prev, 1e-6, 1e-9};
  DiodeStampNonlinear(d, ctx, &it);
}

static void DiodeUpdateState(Device* d, double* x, TimeStepState* ts) {
  (void)d;
  (void)x;
  (void)ts;
}

static void DiodeFree(Device* d) {
  if (d) {
    free(d->params);
    free(d->state);
    free(d);
  }
}

static const DeviceVTable kDiodeVTable = {.Init = DiodeInit,
                                          .StampNonlinear = DiodeStampNonlinear,
                                          .StampTransient = DiodeStampTransient,
                                          .UpdateState = DiodeUpdateState,
                                          .Free = DiodeFree};

// ============================================================================
// Factory Functions
// ============================================================================

Device* CreateResistor(const char* name, int n1, int n2, double resistance) {
  Device* d = static_cast<Device*>(calloc(1, sizeof(Device)));
  if (!d) return nullptr;

  d->vt = &kResistorVTable;
  strncpy(d->name, name ? name : "R?", sizeof(d->name) - 1);
  d->nodes[0] = n1;
  d->nodes[1] = n2;
  d->nodes[2] = -1;
  d->nodes[3] = -1;
  d->extra_var = -1;

  ResistorParams* p =
      static_cast<ResistorParams*>(malloc(sizeof(ResistorParams)));
  if (p) {
    p->r = resistance;
    d->params = p;
  }

  return d;
}

Device* CreateCurrentSource(const char* name, int n1, int n2, double current) {
  Device* d = static_cast<Device*>(calloc(1, sizeof(Device)));
  if (!d) return nullptr;

  d->vt = &kCurrentSourceVTable;
  strncpy(d->name, name ? name : "I?", sizeof(d->name) - 1);
  d->nodes[0] = n1;
  d->nodes[1] = n2;
  d->nodes[2] = -1;
  d->nodes[3] = -1;
  d->extra_var = -1;

  CurrentSourceParams* p =
      static_cast<CurrentSourceParams*>(malloc(sizeof(CurrentSourceParams)));
  if (p) {
    p->i = current;
    d->params = p;
  }

  return d;
}

Device* CreateVoltageSource(const char* name, int n1, int n2, double voltage) {
  Device* d = static_cast<Device*>(calloc(1, sizeof(Device)));
  if (!d) return nullptr;

  d->vt = &kVoltageSourceVTable;
  strncpy(d->name, name ? name : "V?", sizeof(d->name) - 1);
  d->nodes[0] = n1;
  d->nodes[1] = n2;
  d->nodes[2] = -1;
  d->nodes[3] = -1;
  d->extra_var = -1;

  VoltageSourceParams* p =
      static_cast<VoltageSourceParams*>(malloc(sizeof(VoltageSourceParams)));
  if (p) {
    p->v = voltage;
    d->params = p;
  }

  return d;
}

Device* CreateCapacitor(const char* name, int n1, int n2, double capacitance) {
  Device* d = static_cast<Device*>(calloc(1, sizeof(Device)));
  if (!d) return nullptr;

  d->vt = &kCapacitorVTable;
  strncpy(d->name, name ? name : "C?", sizeof(d->name) - 1);
  d->nodes[0] = n1;
  d->nodes[1] = n2;
  d->nodes[2] = -1;
  d->nodes[3] = -1;
  d->extra_var = -1;

  CapacitorParams* p =
      static_cast<CapacitorParams*>(malloc(sizeof(CapacitorParams)));
  if (p) {
    p->c = capacitance;
    d->params = p;
  }

  return d;
}

Device* CreateInductor(const char* name, int n1, int n2, double inductance) {
  Device* d = static_cast<Device*>(calloc(1, sizeof(Device)));
  if (!d) return nullptr;

  d->vt = &kInductorVTable;
  strncpy(d->name, name ? name : "L?", sizeof(d->name) - 1);
  d->nodes[0] = n1;
  d->nodes[1] = n2;
  d->nodes[2] = -1;
  d->nodes[3] = -1;
  d->extra_var = -1;

  InductorParams* p =
      static_cast<InductorParams*>(malloc(sizeof(InductorParams)));
  if (p) {
    p->l = inductance;
    d->params = p;
  }

  return d;
}

Device* CreateDiode(const char* name, int n_anode, int n_cathode, double I_s,
                    double n) {
  Device* d = static_cast<Device*>(calloc(1, sizeof(Device)));
  if (!d) return nullptr;

  d->vt = &kDiodeVTable;
  strncpy(d->name, name ? name : "D?", sizeof(d->name) - 1);
  d->nodes[0] = n_anode;
  d->nodes[1] = n_cathode;
  d->nodes[2] = -1;
  d->nodes[3] = -1;
  d->extra_var = -1;

  DiodeParams* p = static_cast<DiodeParams*>(malloc(sizeof(DiodeParams)));
  if (p) {
    p->i_s = I_s;
    p->n = n;
    d->params = p;
  }

  return d;
}

void DeviceFree(Device* d) {
  if (d && d->vt && d->vt->Free) {
    d->vt->Free(d);
  }
}

}  // namespace minispice
