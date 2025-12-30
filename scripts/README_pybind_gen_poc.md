# Pybind11 PoC Generator

**Purpose**: This script (`pybind_gen_poc.py`) validates the feasibility of autogenerating pybind11 bindings from UHDM YAML models.

**Constraints (PoC)**:
- Standalone script; not integrated into the build system.
- Generates read-only bindings (getters).
- Scalar fields exposed as pythonic properties (e.g., `obj.name` instead of `obj.getName()`).
- Does not modify existing bindings or source files.
- Not production-ready; no guarantee of full API coverage.

**Usage**:
```bash
python3 scripts/pybind_gen_poc.py --yaml model/design.yaml --out output_design.cpp
```
### Parser Note

This PoC intentionally reuses the native UHDM model loader
(`loader._load_one_model`) instead of parsing YAML directly.
Some model files contain duplicate keys which are not compatible
with standard YAML parsers. The private loader is used temporarily
and can be wrapped or exposed as a public API in a follow-up step.

