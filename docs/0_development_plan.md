# Mini-SPICE Development Plan

## 1. Project Goals

The goal of the "mini-spice" project is to develop a simple circuit simulator capable of performing DC and transient analyses. It will support basic linear and non-linear circuit elements. This project is intended for educational purposes, providing a clear and understandable implementation of core circuit simulation concepts.

## 2. Core Architectural Components

The simulator will be built around a few key components:

-   **Netlist Parser**: A component to read a text-based netlist file that describes the circuit connections and component values.
-   **Circuit Representation**: An in-memory data structure (e.g., a graph or an adjacency list) to represent the circuit, its nodes, and components.
-   **Equation Engine (Modified Nodal Analysis - MNA)**: The core of the simulator. It will build the system of equations representing the circuit using MNA.
-   **Solver**: A numerical solver for the system of equations. This will involve:
    -   A Newton-Raphson solver for DC analysis (handles both linear and non-linear circuits; linear converges in 1 iteration).
    -   A numerical integration method (e.g., Backward Euler or Trapezoidal Rule) for transient analysis.
-   **Device Model Library**: A collection of models for each supported circuit component.

## 3. Analysis Modes

### 3.1. DC Analysis

-   **Functionality**: Computes the DC operating point of the circuit using Newton-Raphson iteration.
-   **Implementation**:
    1.  The MNA engine will create a system of algebraic equations.
    2.  Use the Newton-Raphson method to iteratively solve for the node voltages. Each component provides its current contribution and its derivative (Jacobian entry).
    3.  For linear circuits, NR converges in 1 iteration (constant Jacobian).
    4.  For non-linear circuits, NR iterates multiple times until convergence.

### 3.2. Transient Analysis

-   **Functionality**: Computes the circuit's behavior over a specified time interval.
-   **Implementation**:
    1.  Discretize time into small steps.
    2.  At each time step, use a numerical integration method (like Backward Euler) to convert reactive components (capacitors, inductors) into equivalent DC models.
    3.  The MNA engine will build a system of equations for the current time step.
    4.  Solve the system of equations (using the Newton-Raphson method if non-linear elements are present) to find the node voltages at the current time step.
    5.  Store the results and advance to the next time step.

## 4. Device Models

Each device will be implemented as a class or module that interfaces with the MNA engine.

### 4.1. Linear Devices

-   **Resistor (R)**:
    -   **MNA Contribution**: Adds its conductance `G = 1/R` to the MNA matrix.
-   **Capacitor (C)**:
    -   **DC Analysis**: Treated as an open circuit.
    -   **Transient Analysis**: Modeled as a current source with a parallel conductance using a numerical integration method.
-   **Inductor (L)**:
    -   **DC Analysis**: Treated as a short circuit.
    -   **Transient Analysis**: Modeled as a voltage source with a series resistance, which requires a modification to the standard MNA formulation (e.g., adding a new variable for the inductor current).
-   **DC Voltage Source (V)**:
    -   **MNA Contribution**: Creates a new row and column in the MNA matrix to enforce the voltage constraint.
-   **DC Current Source (I)**:
    -   **MNA Contribution**: Adds its current value to the right-hand side (RHS) vector.

### 4.2. Non-Linear Devices

For non-linear devices, the model must provide its current contribution and the derivative of the current with respect to its controlling voltages for the Newton-Raphson solver.

-   **Diode**:
    -   **Model**: Use the Shockley diode equation.
    -   **MNA Contribution**: Provides the diode current and its derivative (conductance) at each Newton-Raphson iteration.
-   **MOSFET**:
    -   **Model**: Start with a simple square-law model (as already explored in `src/mosfet_model.py`). This can be extended later.
    -   **MNA Contribution**: Provides the drain current `Ids` and its derivatives with respect to `Vgs`, `Vds`, and `Vbs` (transconductance `gm`, output conductance `gds`, and body-effect transconductance `gmb`).
    -   **Charge Model**: The existing charge model will be crucial for transient analysis to calculate capacitive currents.

## 5. Development Roadmap

The development can be broken down into the following phases:

-   **Phase 1: Core DC Simulator (Linear & Nonlinear)**
    1.  Implement the netlist parser for R, L, C, V, I sources.
    2.  Build the circuit representation data structure.
    3.  Implement the MNA engine with the `StampContext` API for device stamping.
    4.  Implement the Newton-Raphson solver (unified for both linear and nonlinear circuits).
    5.  Create device models: Resistor, DC Voltage Source, DC Current Source.
    6.  Verify DC analysis with simple linear resistive circuits (NR converges in 1 iteration).
    7.  Create device models: Diode (Shockley), MOSFET (Level 1 square-law).
    8.  Integrate nonlinear models and verify DC operating points with transistor circuits.
    9.  Write comprehensive unit tests for all device stamps.

-   **Phase 2: Transient Analysis**
    1.  Implement the time-stepping loop with Backward Euler integration.
    2.  Create transient models for Capacitor and Inductor using equivalent conductance + history RHS.
    3.  Implement `update_state()` hook for reactive devices to store history (voltage/current).
    4.  Integrate transient models with NR solver inside the time-stepping loop.
    5.  Add support for time-varying sources (e.g., PWL, SIN, PULSE).
    6.  Write tests for simple transient circuits (e.g., RC charging, RL, RLC oscillation).

-   **Phase 3: Advanced Models & Optimization**
    1.  Improve MOSFET model with charge-based formulation (see [minimal_charge_based_mosfet.md](minimal_charge_based_mosfet.md)).
    2.  Add diode junction capacitance for transient accuracy.
    3.  Improve netlist parser with .MODEL and .PARAM cards.
    4.  Optimize matrix assembly and solver for larger circuits.
    2.  Enhance plotting and output capabilities.
    3.  Implement more advanced device models.
    4.  Performance optimizations.
