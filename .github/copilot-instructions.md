# mini-spice — Copilot instructions for AI coding agents

Be concise, change only what’s necessary, and prefer minimal, well-tested edits.

Quick context
- Purpose: educational MNA-based circuit simulator (linear + nonlinear DC, transient).
- Key dirs: `src/` (implementation), `docs/` (design + API + theory), `examples/`, `scripts/`.
- Primary build: Pixi (recommended) and CMake/Ninja. See `AGENTS.md` for full overview.

What to know first (essential files)
- `src/device.h` — canonical `DeviceVTable` (PascalCase hooks): `Init`, `StampNonlinear`, `StampTransient`, `UpdateState`, `Free`.
- `src/stamp.h` — `StampContext` helpers used by devices: `ctx_create`, `ctx_free`, `ctx_reset`, `ctx_add_A`, `ctx_add_z`, `ctx_alloc_extra_var`, `ctx_assemble_dense`, `ctx_get_z`.
- `src/circuit.cc/.h` — circuit lifecycle: node indexing, `circuit_finalize()` assigns node var indices, devices request extra vars in `Init`, extras are allocated when finalize runs (devices use `extra_var == -2` to request).
- `docs/2_stamp_api_design.md` — canonical stamping examples and mapping to header symbols; use it as the authoritative doc when editing device stamps.

Project conventions & gotchas
- Vtable names are PascalCase in headers but many examples/docs used lowercase; prefer header names when editing code.
- Device `extra_var` semantics: `-1` = none, `-2` = request allocation during `circuit_finalize()`, >=0 = assigned global variable index (index space: node vars then extra vars).
- Stamp collection: devices append triplets to `StampContext` via `ctx_add_A`/`ctx_add_z`. Assembly to a solver matrix is explicit via `ctx_assemble_dense`.
- Linear vs nonlinear flow: DC uses a unified NR loop; linear devices stamp constant Jacobians and will converge in one iteration.

Developer workflows (commands)
- Build and run tests (recommended via Pixi):
```
pixi run build
pixi run test
```
- CMake alternative:
```
cmake --preset default
cmake --build build
./build/src/dc_linear examples/simple.net
```

When making changes
- Update tests in `src/` (googletest present under `build/_deps/googletest-src`) for new behavior.
- Preserve public API symbols defined in headers (`src/*.h`) — keep casing and signatures.
- Update `docs/2_stamp_api_design.md` and `docs/1_architecture_overview.md` for any API/semantic changes.

Editing patterns & examples
- To add a device, follow `src/device.h` factory pattern and mirror stamping logic from `docs/2_stamp_api_design.md`.
- Example: resistor stamping uses `ctx_add_A(ctx,n1,n1,+g)` etc.; voltage source requests an extra var via `d->extra_var = -2` in `Init`, then `circuit_finalize()` assigns the actual index.

Scope for AI edits
- Prefer small, targeted doc fixes, variable-name alignments, and tests. Avoid broad refactors unless requested.
- When touching code, run the build/tests locally and include test changes with PRs.

Reference & further reading
- AGENTS.md at repository root contains a longer agent guide — consult it when unsure.
- Key headers: `src/device.h`, `src/stamp.h`, `src/circuit.h`.

If anything here is unclear or missing for your task, ask for the specific target (file/function) and I will update these instructions.
