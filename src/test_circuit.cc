// test_circuit.cc
// Unit tests for Circuit API and DC analysis

#include <gtest/gtest.h>

#include <cmath>

#include "circuit.h"
#include "parser.h"

using namespace minispice;

class CircuitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    c = circuit_create();
    ASSERT_NE(c, nullptr);
  }

  void TearDown() override {
    if (c) {
      circuit_free(c);
      c = nullptr;
    }
  }

  Circuit* c = nullptr;
};

// ============================================================================
// Circuit Creation Tests
// ============================================================================

TEST_F(CircuitTest, CreateEmpty) {
  EXPECT_EQ(c->num_nodes, 1);  // Ground node
  EXPECT_EQ(c->num_devices, 0);
  EXPECT_EQ(c->finalized, 0);
}

TEST_F(CircuitTest, AddNodes) {
  int n1 = circuit_add_node(c, "1");
  int n2 = circuit_add_node(c, "2");
  int n3 = circuit_add_node(c, "out");

  EXPECT_EQ(n1, 1);
  EXPECT_EQ(n2, 2);
  EXPECT_EQ(n3, 3);
  EXPECT_EQ(c->num_nodes, 4);  // including ground
}

TEST_F(CircuitTest, GroundNodeHandling) {
  int g1 = circuit_add_node(c, "0");
  int g2 = circuit_add_node(c, "gnd");
  int g3 = circuit_add_node(c, "GND");
  int g4 = circuit_add_node(c, "ground");

  EXPECT_EQ(g1, 0);
  EXPECT_EQ(g2, 0);
  EXPECT_EQ(g3, 0);
  EXPECT_EQ(g4, 0);
}

TEST_F(CircuitTest, DuplicateNodes) {
  int n1 = circuit_add_node(c, "out");
  int n2 = circuit_add_node(c, "out");  // Same name

  EXPECT_EQ(n1, n2);
  EXPECT_EQ(c->num_nodes, 2);  // Only one node added
}

TEST_F(CircuitTest, AddDevices) {
  int n1 = circuit_add_node(c, "1");
  int n2 = circuit_add_node(c, "2");

  Device* r = create_resistor("R1", n1, n2, 1000.0);
  Device* added = circuit_add_device(c, r);

  EXPECT_EQ(added, r);
  EXPECT_EQ(c->num_devices, 1);
}

TEST_F(CircuitTest, Finalization) {
  int n1 = circuit_add_node(c, "1");
  int n2 = circuit_add_node(c, "2");

  Device* r = create_resistor("R1", n1, n2, 1000.0);
  circuit_add_device(c, r);

  int result = circuit_finalize(c);
  EXPECT_EQ(result, 0);
  EXPECT_EQ(c->finalized, 1);
  EXPECT_EQ(c->num_vars, 2);  // Two non-ground nodes
}

TEST_F(CircuitTest, VoltageSourceExtraVar) {
  int n1 = circuit_add_node(c, "1");

  Device* v =
      create_voltage_source("V1", n1, 0, 5.0);  // 0 is ground node index
  circuit_add_device(c, v);

  circuit_finalize(c);

  EXPECT_EQ(c->num_vars, 2);  // 1 node voltage + 1 extra var
  EXPECT_EQ(c->num_extra_vars, 1);
}

// ============================================================================
// DC Analysis Tests
// ============================================================================

TEST_F(CircuitTest, SimpleVoltageDivider) {
  // Circuit: V1(5V) -- R1(1k) -- node1 -- R2(1k) -- GND
  //
  // Expected: V(node1) = 2.5V

  int n1 = circuit_add_node(c, "1");         // between R1 and R2
  int n_vpos = circuit_add_node(c, "vpos");  // positive terminal of V1

  // Note: device nodes are node indices, will be converted to var indices
  Device* v1 = create_voltage_source("V1", n_vpos, 0, 5.0);
  Device* r1 = create_resistor("R1", n_vpos, n1, 1000.0);
  Device* r2 = create_resistor("R2", n1, 0, 1000.0);

  circuit_add_device(c, v1);
  circuit_add_device(c, r1);
  circuit_add_device(c, r2);

  circuit_finalize(c);

  // Convert device nodes to var indices
  for (Device* d = c->devices; d; d = d->next) {
    for (int i = 0; i < 4; i++) {
      int node_idx = d->nodes[i];
      if (node_idx >= 0 && node_idx < c->num_nodes) {
        d->nodes[i] = c->nodes[node_idx].var_index;
      }
    }
  }

  double x[4] = {0};
  int iters = circuit_dc_analysis(c, x, 100, 1e-9, 1e-6);

  EXPECT_GT(iters, 0);

  // Find var index for node1
  int v_n1 = c->nodes[n1].var_index;
  int v_vpos = c->nodes[n_vpos].var_index;

  EXPECT_NEAR(x[v_vpos], 5.0, 1e-6);  // V+ should be 5V
  EXPECT_NEAR(x[v_n1], 2.5, 1e-6);    // Divider output should be 2.5V
}

TEST_F(CircuitTest, CurrentSourceWithResistor) {
  // Circuit: I1(1mA) --> node1 -- R1(1k) --> GND
  //
  // Expected: V(node1) = I * R = 1mA * 1k = 1V

  int n1 = circuit_add_node(c, "1");

  Device* i1 = create_current_source("I1", 0, n1, 0.001);  // Current into n1
  Device* r1 = create_resistor("R1", n1, 0, 1000.0);

  circuit_add_device(c, i1);
  circuit_add_device(c, r1);

  circuit_finalize(c);

  // Convert device nodes to var indices
  for (Device* d = c->devices; d; d = d->next) {
    for (int i = 0; i < 4; i++) {
      int node_idx = d->nodes[i];
      if (node_idx >= 0 && node_idx < c->num_nodes) {
        d->nodes[i] = c->nodes[node_idx].var_index;
      }
    }
  }

  double x[4] = {0};
  int iters = circuit_dc_analysis(c, x, 100, 1e-9, 1e-6);

  EXPECT_GT(iters, 0);

  int v_n1 = c->nodes[n1].var_index;
  EXPECT_NEAR(x[v_n1], 1.0, 1e-6);
}

// ============================================================================
// Parser Tests
// ============================================================================

TEST(ParserTest, SimpleResistorCircuit) {
  const char* netlist = R"(
* Simple voltage divider
V1 vpos 0 5
R1 vpos out 1k
R2 out 0 1k
)";

  Circuit* c = parse_netlist_string(netlist);
  ASSERT_NE(c, nullptr);

  EXPECT_EQ(c->num_devices, 3);  // V1, R1, R2
  EXPECT_TRUE(c->finalized);

  double* x = (double*)calloc(c->num_vars, sizeof(double));
  ASSERT_NE(x, nullptr);

  int iters = circuit_dc_analysis(c, x, 100, 1e-9, 1e-6);
  EXPECT_GT(iters, 0);

  // Find output node voltage
  int out_node_idx = circuit_get_node(c, "out");
  EXPECT_GE(out_node_idx, 0);

  if (out_node_idx >= 0) {
    int out_var = c->nodes[out_node_idx].var_index;
    EXPECT_NEAR(x[out_var], 2.5, 0.01);
  }

  free(x);
  circuit_free(c);
}

TEST(ParserTest, CurrentSourceCircuit) {
  const char* netlist = R"(
I1 0 out 1m
R1 out 0 2k
)";

  Circuit* c = parse_netlist_string(netlist);
  ASSERT_NE(c, nullptr);

  double* x = (double*)calloc(c->num_vars, sizeof(double));
  ASSERT_NE(x, nullptr);

  int iters = circuit_dc_analysis(c, x, 100, 1e-9, 1e-6);
  EXPECT_GT(iters, 0);

  int out_node_idx = circuit_get_node(c, "out");
  if (out_node_idx >= 0) {
    int out_var = c->nodes[out_node_idx].var_index;
    EXPECT_NEAR(x[out_var], 2.0, 0.01);  // 1mA * 2k = 2V
  }

  free(x);
  circuit_free(c);
}

TEST(ParserTest, ValueParsing) {
  const char* netlist = R"(
V1 1 0 1.5
R1 1 0 4.7k
R2 1 0 100m
)";

  Circuit* c = parse_netlist_string(netlist);
  ASSERT_NE(c, nullptr);

  // Verify circuit was created (detailed value checks would need access to
  // params)
  EXPECT_EQ(c->num_devices, 3);

  circuit_free(c);
}

TEST(ParserTest, Comments) {
  const char* netlist = R"(
* This is a SPICE comment
# This is also a comment
// C++ style comment
V1 1 0 5
R1 1 0 1k
)";

  Circuit* c = parse_netlist_string(netlist);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->num_devices, 2);

  circuit_free(c);
}

TEST(ParserTest, InductorDC) {
  // In DC, inductor is short circuit
  const char* netlist = R"(
V1 1 0 10
L1 1 out 1m
R1 out 0 1k
)";

  Circuit* c = parse_netlist_string(netlist);
  ASSERT_NE(c, nullptr);

  double* x = (double*)calloc(c->num_vars, sizeof(double));
  ASSERT_NE(x, nullptr);

  int iters = circuit_dc_analysis(c, x, 100, 1e-9, 1e-6);
  EXPECT_GT(iters, 0);

  // In DC, inductor is short so V(out) = V(1) = 10V
  int out_node = circuit_get_node(c, "out");
  int node_1 = circuit_get_node(c, "1");

  if (out_node >= 0 && node_1 >= 0) {
    int out_var = c->nodes[out_node].var_index;
    int n1_var = c->nodes[node_1].var_index;
    EXPECT_NEAR(x[out_var], x[n1_var], 0.01);
    EXPECT_NEAR(x[out_var], 10.0, 0.01);
  }

  free(x);
  circuit_free(c);
}

TEST(ParserTest, CapacitorDC) {
  // In DC, capacitor is open circuit
  const char* netlist = R"(
V1 1 0 10
R1 1 out 1k
C1 out 0 1u
)";

  Circuit* c = parse_netlist_string(netlist);
  ASSERT_NE(c, nullptr);

  double* x = (double*)calloc(c->num_vars, sizeof(double));
  ASSERT_NE(x, nullptr);

  int iters = circuit_dc_analysis(c, x, 100, 1e-9, 1e-6);
  EXPECT_GT(iters, 0);

  // Capacitor is open, no current through R1, so V(out) = V(1) = 10V
  int out_node = circuit_get_node(c, "out");
  if (out_node >= 0) {
    int out_var = c->nodes[out_node].var_index;
    EXPECT_NEAR(x[out_var], 10.0, 0.01);
  }

  free(x);
  circuit_free(c);
}
