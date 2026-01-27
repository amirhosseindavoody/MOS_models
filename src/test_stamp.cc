// test_stamp.cc
// Unit tests for StampContext API

#include <gtest/gtest.h>

#include <cstring>

#include "stamp.h"

using namespace minispice;

class StampContextTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx = ctx_create(4);
    ASSERT_NE(ctx, nullptr);
  }

  void TearDown() override {
    if (ctx) {
      ctx_free(ctx);
      ctx = nullptr;
    }
  }

  StampContext* ctx = nullptr;
};

TEST_F(StampContextTest, CreateAndFree) {
  // Test basic creation (already done in SetUp)
  EXPECT_EQ(ctx_get_num_vars(ctx), 4);
}

TEST_F(StampContextTest, CreateWithZeroVars) {
  StampContext* bad = ctx_create(0);
  EXPECT_EQ(bad, nullptr);
}

TEST_F(StampContextTest, CreateWithNegativeVars) {
  StampContext* bad = ctx_create(-5);
  EXPECT_EQ(bad, nullptr);
}

TEST_F(StampContextTest, AddTriplets) {
  ctx_add_A(ctx, 0, 0, 1.0);
  ctx_add_A(ctx, 0, 1, 2.0);
  ctx_add_A(ctx, 1, 0, 3.0);
  ctx_add_A(ctx, 1, 1, 4.0);

  size_t count;
  const Triplet* triplets = ctx_get_triplets(ctx, &count);
  EXPECT_EQ(count, 4u);
  EXPECT_NE(triplets, nullptr);
}

TEST_F(StampContextTest, AddRHS) {
  ctx_add_z(ctx, 0, 1.5);
  ctx_add_z(ctx, 1, 2.5);
  ctx_add_z(ctx, 2, 3.5);

  double* z = ctx_get_z(ctx);
  EXPECT_NE(z, nullptr);
  EXPECT_DOUBLE_EQ(z[0], 1.5);
  EXPECT_DOUBLE_EQ(z[1], 2.5);
  EXPECT_DOUBLE_EQ(z[2], 3.5);
  EXPECT_DOUBLE_EQ(z[3], 0.0);  // Unchanged
}

TEST_F(StampContextTest, AccumulateSameEntry) {
  ctx_add_A(ctx, 0, 0, 1.0);
  ctx_add_A(ctx, 0, 0, 2.0);
  ctx_add_A(ctx, 0, 0, 3.0);

  // Three separate triplets, but will sum to 6.0 when assembled
  size_t count;
  const Triplet* triplets = ctx_get_triplets(ctx, &count);
  EXPECT_EQ(count, 3u);

  double matrix[16] = {0};
  ctx_assemble_dense(ctx, matrix);
  EXPECT_DOUBLE_EQ(matrix[0], 6.0);  // A[0][0] = 1.0 + 2.0 + 3.0
}

TEST_F(StampContextTest, AccumulateRHS) {
  ctx_add_z(ctx, 0, 1.0);
  ctx_add_z(ctx, 0, 2.0);
  ctx_add_z(ctx, 0, 3.0);

  double* z = ctx_get_z(ctx);
  EXPECT_DOUBLE_EQ(z[0], 6.0);
}

TEST_F(StampContextTest, Reset) {
  ctx_add_A(ctx, 0, 0, 5.0);
  ctx_add_z(ctx, 0, 10.0);

  ctx_reset(ctx);

  size_t count;
  ctx_get_triplets(ctx, &count);
  EXPECT_EQ(count, 0u);

  double* z = ctx_get_z(ctx);
  EXPECT_DOUBLE_EQ(z[0], 0.0);
}

TEST_F(StampContextTest, AssembleDense) {
  // Build a 2x2 system
  // [ 2, -1 ]
  // [-1,  2 ]
  ctx_add_A(ctx, 0, 0, 2.0);
  ctx_add_A(ctx, 0, 1, -1.0);
  ctx_add_A(ctx, 1, 0, -1.0);
  ctx_add_A(ctx, 1, 1, 2.0);

  double matrix[16] = {0};
  ctx_assemble_dense(ctx, matrix);

  // Row-major order: matrix[row * 4 + col]
  EXPECT_DOUBLE_EQ(matrix[0 * 4 + 0], 2.0);
  EXPECT_DOUBLE_EQ(matrix[0 * 4 + 1], -1.0);
  EXPECT_DOUBLE_EQ(matrix[1 * 4 + 0], -1.0);
  EXPECT_DOUBLE_EQ(matrix[1 * 4 + 1], 2.0);
}

TEST_F(StampContextTest, IgnoreOutOfBounds) {
  // These should be silently ignored
  ctx_add_A(ctx, -1, 0, 1.0);
  ctx_add_A(ctx, 0, -1, 1.0);
  ctx_add_A(ctx, 4, 0, 1.0);
  ctx_add_A(ctx, 0, 4, 1.0);
  ctx_add_z(ctx, -1, 1.0);
  ctx_add_z(ctx, 4, 1.0);

  size_t count;
  ctx_get_triplets(ctx, &count);
  EXPECT_EQ(count, 0u);
}

TEST_F(StampContextTest, IgnoreZeroValues) {
  ctx_add_A(ctx, 0, 0, 0.0);

  size_t count;
  ctx_get_triplets(ctx, &count);
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
