# Pybind11 PoC Generator

**Purpose**: This script (`pybind_gen_poc.py`) validates the feasibility of autogenerating pybind11 bindings from UHDM YAML models.

**Constraints (PoC)**:
- Standalone script; not integrated into the build system.
- Generates read-only bindings (getters).
- Does not modify existing bindings or source files.
- Not production-ready; no guarantee of full API coverage.

**Usage**:
```bash
python3 scripts/pybind_gen_poc.py --yaml model/design.yaml --out output_design.cpp
```
