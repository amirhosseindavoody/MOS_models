# Mini-SPICE Development Plan

## 1. Project Goals

The goal of the "mini-spice" project is to develop a simple circuit simulator capable of performing DC and transient analyses. It will support basic linear and non-linear circuit elements. This project is intended for educational purposes, providing a clear and understandable implementation of core circuit simulation concepts.

## 2. Core Architectural Components

The simulator will be built around a few key components:

-   **Netlist Parser**: A component to read a text-based netlist file that describes the circuit connections and component values.
-   **Circuit Representation**: An in-memory data structure (e.g., a graph or an adjacency list) to represent the circuit, its nodes, and components.
-   **Equation Engine (Modified Nodal Analysis - MNA)**: The core of the simulator. It will build the system of equations representing the circuit using MNA.
-   **Solver**: A numerical solver for the system of equations. This will involve:
    -   A linear solver for DC analysis of linear circuits.
    -   A non-linear solver (e.g., Newton-Raphson) for DC analysis of non-linear circuits.
    -   A numerical integration method (e.g., Backward Euler or Trapezoidal Rule) for transient analysis.
-   **Device Model Library**: A collection of models for each supported circuit component.

## 3. Analysis Modes

### 3.1. DC Analysis

-   **Functionality**: Computes the DC operating point of the circuit.
-   **Implementation**:
    1.  The MNA engine will create a system of linear or non-linear algebraic equations.
    2.  For linear circuits, solve `Ax = b` directly.
    3.  For non-linear circuits, use the Newton-Raphson method to iteratively solve for the node voltages. This requires each non-linear component to provide its current contribution and its derivative (Jacobian entry).

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

-   **Phase 1: Core DC Simulator for Linear Circuits**
    1.  Implement the netlist parser for R, L, C, V, I sources.
    2.  Build the circuit representation data structure.
    3.  Implement the MNA engine for linear resistive circuits.
    4.  Implement a linear solver for the `Ax = b` system.
    5.  Add support for inductors and DC voltage sources.
    6.  Write tests to verify DC analysis with simple linear circuits.

-   **Phase 2: Non-Linear DC Analysis**
    1.  Implement the Newton-Raphson solver.
    2.  Create the device model for the Diode.
    3.  Integrate the Diode model into the MNA engine and Newton-Raphson solver.
    4.  Create the device model for the MOSFET (Level 1 model).
    5.  Integrate the MOSFET model.
    6.  Write tests for DC operating points of circuits with diodes and transistors.

-   **Phase 3: Transient Analysis**
    1.  Implement the time-stepping loop.
    2.  Implement the Backward Euler (or Trapezoidal) integration method.
    3.  Update Capacitor and Inductor models to support their transient behavior.
    4.  Integrate the transient models with the MNA engine and solvers.
    5.  Add support for time-varying sources (e.g., PWL, SIN).
    6.  Write tests for simple transient circuits (e.g., RC, RL, RLC).

-   **Phase 4: Refinement and Expansion**
    1.  Improve the netlist parser with more features (.MODEL cards, etc.).
    2.  Enhance plotting and output capabilities.
    3.  Implement more advanced device models.
    4.  Performance optimizations.
