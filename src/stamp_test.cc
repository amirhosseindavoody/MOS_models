// test_stamp.cc
// Unit tests for StampContext API

#include "stamp.h"

#include <gtest/gtest.h>

#include <cstring>

using namespace minispice;

class StampContextTest : public ::testing::Test {
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

TEST_F(StampContextTest, CreateAndFree) {
  // Test basic creation (already done in SetUp)
  EXPECT_EQ(CtxGetNumVars(ctx), 4);
}

TEST_F(StampContextTest, CreateWithZeroVars) {
  StampContext* bad = CtxCreate(0);
  EXPECT_EQ(bad, nullptr);
}

TEST_F(StampContextTest, CreateWithNegativeVars) {
  StampContext* bad = CtxCreate(-5);
  EXPECT_EQ(bad, nullptr);
}

TEST_F(StampContextTest, AddTriplets) {
  CtxAddA(ctx, 0, 0, 1.0);
  CtxAddA(ctx, 0, 1, 2.0);
  CtxAddA(ctx, 1, 0, 3.0);
  CtxAddA(ctx, 1, 1, 4.0);

  size_t count;
  const Triplet* triplets = CtxGetTriplets(ctx, &count);
  EXPECT_EQ(count, 4u);
  EXPECT_NE(triplets, nullptr);
}

TEST_F(StampContextTest, AddRHS) {
  CtxAddZ(ctx, 0, 1.5);
  CtxAddZ(ctx, 1, 2.5);
  CtxAddZ(ctx, 2, 3.5);

  double* z = CtxGetZ(ctx);
  EXPECT_NE(z, nullptr);
  EXPECT_DOUBLE_EQ(z[0], 1.5);
  EXPECT_DOUBLE_EQ(z[1], 2.5);
  EXPECT_DOUBLE_EQ(z[2], 3.5);
  EXPECT_DOUBLE_EQ(z[3], 0.0);  // Unchanged
}

TEST_F(StampContextTest, AccumulateSameEntry) {
  CtxAddA(ctx, 0, 0, 1.0);
  CtxAddA(ctx, 0, 0, 2.0);
  CtxAddA(ctx, 0, 0, 3.0);

  // Three separate triplets, but will sum to 6.0 when assembled
  size_t count;
  const Triplet* triplets = CtxGetTriplets(ctx, &count);
  EXPECT_EQ(count, 3u);

  double matrix[16] = {0};
  CtxAssembleDense(ctx, matrix);
  EXPECT_DOUBLE_EQ(matrix[0], 6.0);  // A[0][0] = 1.0 + 2.0 + 3.0
}

TEST_F(StampContextTest, AccumulateRHS) {
  CtxAddZ(ctx, 0, 1.0);
  CtxAddZ(ctx, 0, 2.0);
  CtxAddZ(ctx, 0, 3.0);

  double* z = CtxGetZ(ctx);
  EXPECT_DOUBLE_EQ(z[0], 6.0);
}

TEST_F(StampContextTest, Reset) {
  CtxAddA(ctx, 0, 0, 5.0);
  CtxAddZ(ctx, 0, 10.0);

  CtxReset(ctx);

  size_t count;
  CtxGetTriplets(ctx, &count);
  EXPECT_EQ(count, 0u);

  double* z = CtxGetZ(ctx);
  EXPECT_DOUBLE_EQ(z[0], 0.0);
}

TEST_F(StampContextTest, AssembleDense) {
  // Build a 2x2 system
  // [ 2, -1 ]
  // [-1,  2 ]
  CtxAddA(ctx, 0, 0, 2.0);
  CtxAddA(ctx, 0, 1, -1.0);
  CtxAddA(ctx, 1, 0, -1.0);
  CtxAddA(ctx, 1, 1, 2.0);

  double matrix[16] = {0};
  CtxAssembleDense(ctx, matrix);

  // Row-major order: matrix[row * 4 + col]
  EXPECT_DOUBLE_EQ(matrix[0 * 4 + 0], 2.0);
  EXPECT_DOUBLE_EQ(matrix[0 * 4 + 1], -1.0);
  EXPECT_DOUBLE_EQ(matrix[1 * 4 + 0], -1.0);
  EXPECT_DOUBLE_EQ(matrix[1 * 4 + 1], 2.0);
}

TEST_F(StampContextTest, IgnoreOutOfBounds) {
  // These should be silently ignored
  CtxAddA(ctx, -1, 0, 1.0);
  CtxAddA(ctx, 0, -1, 1.0);
  CtxAddA(ctx, 4, 0, 1.0);
  CtxAddA(ctx, 0, 4, 1.0);
  CtxAddZ(ctx, -1, 1.0);
  CtxAddZ(ctx, 4, 1.0);

  size_t count;
  CtxGetTriplets(ctx, &count);
  EXPECT_EQ(count, 0u);
}

TEST_F(StampContextTest, IgnoreZeroValues) {
  CtxAddA(ctx, 0, 0, 0.0);

  size_t count;
  CtxGetTriplets(ctx, &count);
  EXPECT_EQ(count, 0u);  // Zero entries should be skipped
}

// Test integration methods are defined correctly
TEST(IntegrationMethodTest, BackwardEulerCoefficients) {
  EXPECT_STREQ(BACKWARD_EULER.name, "backward_euler");
  EXPECT_EQ(BACKWARD_EULER.order, 1);
  EXPECT_DOUBLE_EQ(BACKWARD_EULER.alpha0, 1.0);
  EXPECT_DOUBLE_EQ(BACKWARD_EULER.alpha1, 1.0);
  EXPECT_DOUBLE_EQ(BACKWARD_EULER.alpha2, 0.0);
}

TEST(IntegrationMethodTest, TrapezoidalCoefficients) {
  EXPECT_STREQ(TRAPEZOIDAL.name, "trapezoidal");
  EXPECT_EQ(TRAPEZOIDAL.order, 2);
  EXPECT_DOUBLE_EQ(TRAPEZOIDAL.alpha0, 2.0);
  EXPECT_DOUBLE_EQ(TRAPEZOIDAL.alpha1, 2.0);
}

TEST(IntegrationMethodTest, Gear2Coefficients) {
  EXPECT_STREQ(GEAR2.name, "gear2");
  EXPECT_EQ(GEAR2.order, 2);
  EXPECT_DOUBLE_EQ(GEAR2.alpha0, 1.5);
  EXPECT_DOUBLE_EQ(GEAR2.alpha1, 2.0);
  EXPECT_DOUBLE_EQ(GEAR2.alpha2, -0.5);
  EXPECT_EQ(GEAR2.required_history, 2);
}
