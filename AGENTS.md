# Mini-SPICE Agent Instructions

This document provides instructions for AI coding assistants helping with the development of **mini-spice**, an educational C/C++ circuit simulator.

---

## Project Overview

**Mini-SPICE** is an educational circuit simulator designed to teach the fundamentals of SPICE-like circuit solvers. The project implements:

- **Modified Nodal Analysis (MNA)** for equation formulation
- **Newton-Raphson iteration** for both linear and nonlinear DC analysis
- **Transient analysis** with pluggable integration methods (Backward Euler, Trapezoidal, Gear/BDF2)
- **Device models** for linear (R, L, C, V, I) and nonlinear (Diode, MOSFET) components

### Design Principles

1. **Simplicity and Clarity**: The code is designed to be simple and easy to understand, with clear variable names and documentation.
2. **Modularity**: The code is modular, with each component having a clear responsibility and well-defined interface.
3. **Flexibility**: The code is flexible, allowing for easy extension and modification.
4. **Performance**: The code is optimized for performance, with efficient data structures and algorithms.

### Development environment

1. **Pixi**: Use Pixi for dependency management.
2. **CMake**: Use CMake for building the project.
3. **Google Test**: Use Google Test for unit testing.
4. **Google Benchmark**: Use Google Benchmark for performance testing.
5. **Ninja**: Use Ninja for building the project.

### Code Style

1. **Google C++ Style**: Follow the Google C++ Style Guide for code style.
2. **Consistent Naming**: Use descriptive and consistent naming conventions for variables, functions, and classes.
3. **Documentation**: Document the code with clear and concise comments and documentation.
4. **Testing**: Write unit tests for the code to ensure it works as expected.

### Testing Strategy

1. **Unit Tests**: Write with Google Test.
2. **Integration Tests**: Write integration tests to ensure the components work together as expected.
3. **Performance Tests**: Write performance tests to ensure the code is optimized for performance. Keep track of performance metrics in a performance.txt file and ensure they do not degrade over time.

---

## Repository Structure

```
mini-spice/
├── docs/                           # Design documentation
│   ├── 0_development_plan.md       # Development roadmap and phases
│   ├── 1_architecture_overview.md  # High-level architecture
│   ├── 2_stamp_api_design.md       # C-style API for device stamping
│   ├── 3_mna_stamps.md             # MNA matrix stamps for each device
│   ├── 4_a_matrix_relationship.md  # Math behind Jacobian and MNA
│   └── minimal_charge_based_mosfet.md  # MOSFET model derivation
├── src/                            # C/C++ source code
│   ├── CMakeLists.txt
│   └── dc_linear.cc                # Current working prototype
├── examples/                       # Example netlists and scripts
├── scripts/                        # Utility scripts
├── build/                          # CMake build directory
├── CMakeLists.txt                  # Root CMake configuration
├── pixi.toml                       # Pixi package manager config
└── README.md
```.

---

## Build and Run

### Using Pixi (Recommended)

```bash
pixi run build
pixi run test
```

### Using CMake Directly

```bash
cmake --preset default
cmake --build build
./build/src/dc_linear examples/simple.net
```

---

## Documentation

Refer to these documents for detailed specifications:

| Document | Purpose |
|----------|---------|
| `docs/0_development_plan.md` | Project goals and roadmap |
| `docs/1_architecture_overview.md` | High-level system design |
| `docs/2_stamp_api_design.md` | C API specification |
| `docs/3_mna_stamps.md` | Device stamp formulas |
| `docs/4_a_matrix_relationship.md` | Jacobian mathematics |
| `docs/minimal_charge_based_mosfet.md` | MOSFET model details |

---

## Questions to Consider When Implementing

1. **Does this device need an extra variable?** (V-sources, inductors do)
2. **Is the device linear or nonlinear?** (Affects stamping approach)
3. **Does the device have memory?** (Reactive devices need state storage)
4. **What are the Jacobian entries?** (Partial derivatives of current w.r.t. voltage)
5. **What goes in the RHS?** (Source values, history terms, linearization constants)

---

*Last Updated: January 2026*
