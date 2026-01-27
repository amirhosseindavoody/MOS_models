// Mini-SPICE circuit simulator main entry point
//
// Command-line interface for the mini-spice simulator.
//

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "circuit.h"
#include "parser.h"

namespace minispice {

static void print_usage(const char* prog) {
  printf("Usage: %s <netlist_file> [options]\n", prog);
  printf("\nOptions:\n");
  printf("  -h, --help     Show this help message\n");
  printf("  -v, --verbose  Verbose output\n");
  printf("  --max-iter N   Maximum NR iterations (default: 100)\n");
  printf("  --tol-abs T    Absolute tolerance (default: 1e-9)\n");
  printf("  --tol-rel T    Relative tolerance (default: 1e-6)\n");
}

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  const char* netlist_file = nullptr;
  bool verbose = false;
  int max_iter = 100;
  double tol_abs = 1e-9;
  double tol_rel = 1e-6;

  // Parse arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-v") == 0 ||
               strcmp(argv[i], "--verbose") == 0) {
      verbose = true;
    } else if (strcmp(argv[i], "--max-iter") == 0 && i + 1 < argc) {
      max_iter = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--tol-abs") == 0 && i + 1 < argc) {
      tol_abs = atof(argv[++i]);
    } else if (strcmp(argv[i], "--tol-rel") == 0 && i + 1 < argc) {
      tol_rel = atof(argv[++i]);
    } else if (argv[i][0] != '-') {
      netlist_file = argv[i];
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      print_usage(argv[0]);
      return 1;
    }
  }

  if (!netlist_file) {
    fprintf(stderr, "Error: No netlist file specified\n");
    print_usage(argv[0]);
    return 1;
  }

  // Parse netlist
  printf("Parsing netlist: %s\n", netlist_file);
  Circuit* c = parse_netlist_file(netlist_file);
  if (!c) {
    fprintf(stderr, "Error: Failed to parse netlist\n");
    return 1;
  }

  if (verbose) {
    circuit_print_summary(c);
    printf("\n");
  }

  // Allocate solution vector
  double* x = (double*)calloc(c->num_vars, sizeof(double));
  if (!x) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    circuit_free(c);
    return 1;
  }

  // Run DC analysis
  printf("Running DC analysis...\n");
  int iterations = circuit_dc_analysis(c, x, max_iter, tol_abs, tol_rel);

  if (iterations < 0) {
    fprintf(stderr, "Error: DC analysis failed\n");
    free(x);
    circuit_free(c);
    return 1;
  }

  if (verbose) {
    printf("Converged in %d iteration(s)\n\n", iterations);
  }

  // Print results
  circuit_print_solution(c, x);

  // Cleanup
  free(x);
  circuit_free(c);

  return 0;
}

}  // namespace minispice