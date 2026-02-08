## Mini-SPICE documentation

This directory contains the design and implementation documentation for the mini-spice educational circuit simulator. It explains architecture, the Stamp API, MNA stamps, time-discretization, and the minimal MOSFET charge model.

Table of contents:
- Development roadmap: [0_development_plan.md](0_development_plan.md)
- Architecture & stamping overview: [1_architecture_overview.md](1_architecture_overview.md)
- Stamp API & vtable details: [2_stamp_api_design.md](2_stamp_api_design.md)
- Device stamps (MNA): [3_mna_stamps.md](3_mna_stamps.md)
- A-matrix / Jacobian math: [4_a_matrix_relationship.md](4_a_matrix_relationship.md)
- Minimal MOSFET model: [minimal_charge_based_mosfet.md](minimal_charge_based_mosfet.md)

Notes:
- Links use numeric prefixes to keep ordering clear. The code headers to consult are `src/stamp.h` and `src/device.h`.
