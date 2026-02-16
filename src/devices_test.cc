// test_devices.cc
// Unit tests for device implementations

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>

#include "device.h"
#include "stamp.h"

using namespace minispice;

class DeviceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx = CtxCreate(4);
    ASSERT_NE(ctx, nullptr);
  }

  void TearDown() override {
    if (ctx) {
      CtxFree(ctx);
      ctx = nullptr;
    }
  }

  StampContext* ctx = nullptr;
};

// ============================================================================
// Resistor Tests
// ============================================================================

TEST_F(DeviceTest, ResistorCreation) {
  Device* r = CreateResistor("R1", 0, 1, 1000.0);
  ASSERT_NE(r, nullptr);
  EXPECT_STREQ(r->name, "R1");
  EXPECT_EQ(r->nodes[0], 0);
  EXPECT_EQ(r->nodes[1], 1);
  EXPECT_EQ(r->extra_var, -1);
  DeviceFree(r);
}

TEST_F(DeviceTest, ResistorStamp) {
  Device* r = CreateResistor("R1", 0, 1, 1000.0);
  ASSERT_NE(r, nullptr);

  IterationState it = {0, nullptr, 1e-9, 1e-6};
  r->vt->StampNonlinear(r, ctx, &it);

  double matrix[16] = {0};
  CtxAssembleDense(ctx, matrix);

  // g = 1/1000 = 0.001
  double g = 0.001;
  EXPECT_DOUBLE_EQ(matrix[0 * 4 + 0], g);   // A[0][0] = +g
  EXPECT_DOUBLE_EQ(matrix[1 * 4 + 1], g);   // A[1][1] = +g
  EXPECT_DOUBLE_EQ(matrix[0 * 4 + 1], -g);  // A[0][1] = -g
  EXPECT_DOUBLE_EQ(matrix[1 * 4 + 0], -g);  // A[1][0] = -g

  DeviceFree(r);
}

TEST_F(DeviceTest, ResistorWithGroundNode) {
  // Node -1 represents ground (excluded from stamping)
  Device* r = CreateResistor("R1", 0, -1, 1000.0);
  ASSERT_NE(r, nullptr);

  IterationState it = {0, nullptr, 1e-9, 1e-6};
  r->vt->StampNonlinear(r, ctx, &it);

  double matrix[16] = {0};
  CtxAssembleDense(ctx, matrix);

  // Only A[0][0] should be set
  double g = 0.001;
  EXPECT_DOUBLE_EQ(matrix[0 * 4 + 0], g);
  EXPECT_DOUBLE_EQ(matrix[0 * 4 + 1], 0.0);
  EXPECT_DOUBLE_EQ(matrix[1 * 4 + 0], 0.0);

  DeviceFree(r);
}

// ============================================================================
// Current Source Tests
// ============================================================================

TEST_F(DeviceTest, CurrentSourceCreation) {
  Device* i = CreateCurrentSource("I1", 0, 1, 0.001);
  ASSERT_NE(i, nullptr);
  EXPECT_STREQ(i->name, "I1");
  DeviceFree(i);
}

TEST_F(DeviceTest, CurrentSourceStamp) {
  Device* i = CreateCurrentSource("I1", 0, 1, 0.001);
  ASSERT_NE(i, nullptr);

  IterationState it = {0, nullptr, 1e-9, 1e-6};
  i->vt->StampNonlinear(i, ctx, &it);

  // Current source only affects RHS
  size_t count;
  CtxGetTriplets(ctx, &count);
  EXPECT_EQ(count, 0u);  // No matrix entries

  double* z = CtxGetZ(ctx);
  EXPECT_DOUBLE_EQ(z[0], -0.001);  // z[n1] -= I
  EXPECT_DOUBLE_EQ(z[1], +0.001);  // z[n2] += I

  DeviceFree(i);
}

// ============================================================================
// Voltage Source Tests
// ============================================================================

TEST_F(DeviceTest, VoltageSourceCreation) {
  Device* v = CreateVoltageSource("V1", 0, 1, 5.0);
  ASSERT_NE(v, nullptr);
  EXPECT_STREQ(v->name, "V1");
  EXPECT_EQ(v->extra_var,
            -2);  // Should request extra variable for branch current
  DeviceFree(v);
}

TEST_F(DeviceTest, VoltageSourceInit) {
  Device* v = CreateVoltageSource("V1", 0, 1, 5.0);
  ASSERT_NE(v, nullptr);

  // Init should request extra variable
  v->vt->Init(v, nullptr);
  EXPECT_EQ(v->extra_var, -2);  // Marker for needs allocation

  DeviceFree(v);
}

TEST_F(DeviceTest, VoltageSourceStamp) {
  // Create a fresh context with 3 variables (2 nodes + 1 extra)
  CtxFree(ctx);
  ctx = CtxCreate(3);

  Device* v = CreateVoltageSource("V1", 0, 1, 5.0);
  ASSERT_NE(v, nullptr);

  // Simulate finalization: allocate extra var
  v->extra_var = 2;  // k = 2

  IterationState it = {0, nullptr, 1e-9, 1e-6};
  v->vt->StampNonlinear(v, ctx, &it);

  double matrix[9] = {0};
  CtxAssembleDense(ctx, matrix);

  // Voltage source stamp with n1=0, n2=1, k=2:
  // A[0][2] = +1, A[1][2] = -1
  // A[2][0] = +1, A[2][1] = -1
  EXPECT_DOUBLE_EQ(matrix[0 * 3 + 2], 1.0);   // A[0][k]
  EXPECT_DOUBLE_EQ(matrix[1 * 3 + 2], -1.0);  // A[1][k]
  EXPECT_DOUBLE_EQ(matrix[2 * 3 + 0], 1.0);   // A[k][0]
  EXPECT_DOUBLE_EQ(matrix[2 * 3 + 1], -1.0);  // A[k][1]

  double* z = CtxGetZ(ctx);
  EXPECT_DOUBLE_EQ(z[2], 5.0);  // z[k] = V

  DeviceFree(v);
}

// ============================================================================
// Capacitor Tests
// ============================================================================

TEST_F(DeviceTest, CapacitorDCStamp) {
  Device* c = CreateCapacitor("C1", 0, 1, 1e-6);
  ASSERT_NE(c, nullptr);

  // Init to allocate state
  c->vt->Init(c, nullptr);

  IterationState it = {0, nullptr, 1e-9, 1e-6};
  c->vt->StampNonlinear(c, ctx, &it);

  // In DC, capacitor is open circuit - no stamp
  size_t count;
  CtxGetTriplets(ctx, &count);
  EXPECT_EQ(count, 0u);

  DeviceFree(c);
}

// ============================================================================
// Inductor Tests
// ============================================================================

TEST_F(DeviceTest, InductorDCStamp) {
  // Create context with extra variable for inductor
  CtxFree(ctx);
  ctx = CtxCreate(3);

  Device* l = CreateInductor("L1", 0, 1, 1e-3);
  ASSERT_NE(l, nullptr);

  // Simulate finalization
  l->vt->Init(l, nullptr);
  l->extra_var = 2;

  IterationState it = {0, nullptr, 1e-9, 1e-6};
  l->vt->StampNonlinear(l, ctx, &it);

  double matrix[9] = {0};
  CtxAssembleDense(ctx, matrix);

  // In DC, inductor is short circuit (like V=0 voltage source)
  EXPECT_DOUBLE_EQ(matrix[0 * 3 + 2], 1.0);   // A[0][k]
  EXPECT_DOUBLE_EQ(matrix[1 * 3 + 2], -1.0);  // A[1][k]
  EXPECT_DOUBLE_EQ(matrix[2 * 3 + 0], 1.0);   // A[k][0]
  EXPECT_DOUBLE_EQ(matrix[2 * 3 + 1], -1.0);  // A[k][1]

  DeviceFree(l);
}

// ============================================================================
// Diode Tests
// ============================================================================

TEST_F(DeviceTest, DiodeCreation) {
  Device* d = CreateDiode("D1", 0, 1, 1e-14, 1.0);
  ASSERT_NE(d, nullptr);
  EXPECT_STREQ(d->name, "D1");
  DeviceFree(d);
}

TEST_F(DeviceTest, DiodeStampAtZeroBias) {
  Device* d = CreateDiode("D1", 0, 1, 1e-14, 1.0);
  ASSERT_NE(d, nullptr);

  double x[4] = {0.0, 0.0, 0.0, 0.0};  // Zero bias
  IterationState it = {0, x, 1e-9, 1e-6};
  d->vt->StampNonlinear(d, ctx, &it);

  // At Vd=0: g_eq = Is/(nVt) * exp(0) = Is/Vt, I_eq = 0 - g_eq * 0 = 0
  double matrix[16] = {0};
  CtxAssembleDense(ctx, matrix);

  // Conductance should be very small but non-zero
  EXPECT_GT(matrix[0 * 4 + 0], 0.0);
  EXPECT_LT(matrix[0 * 4 + 0], 1e-9);  // Very small conductance at zero bias

  DeviceFree(d);
}

TEST_F(DeviceTest, DiodeStampForwardBias) {
  Device* d = CreateDiode("D1", 0, 1, 1e-14, 1.0);
  ASSERT_NE(d, nullptr);

  double x[4] = {0.6, 0.0, 0.0, 0.0};  // Forward bias ~0.6V
  IterationState it = {0, x, 1e-9, 1e-6};
  d->vt->StampNonlinear(d, ctx, &it);

  double matrix[16] = {0};
  CtxAssembleDense(ctx, matrix);

  // Conductance should be significant at forward bias
  double g = matrix[0 * 4 + 0];
  EXPECT_GT(g, 0.001);  // Should be in mS range at 0.6V

  // Verify symmetry
  EXPECT_DOUBLE_EQ(matrix[0 * 4 + 0], matrix[1 * 4 + 1]);
  EXPECT_DOUBLE_EQ(matrix[0 * 4 + 1], matrix[1 * 4 + 0]);
  EXPECT_DOUBLE_EQ(matrix[0 * 4 + 1], -g);

  DeviceFree(d);
}
