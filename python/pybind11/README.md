# UHDM Python Bindings (pyuhdm)

This directory contains **pybind11-based Python bindings** for the UHDM (Universal Hardware Data Model) library.

## Milestone 1: Serializer Access

The current implementation exposes the native `uhdm::Serializer` class, allowing:
- **Restoring** UHDM binary files from disk
- **Saving** UHDM data back to binary files

This provides a **thin, direct mapping** to the native C++ API with no additional state or abstractions.

## Building the Module

The bindings are built as part of the main UHDM project.

### Build Commands

From the UHDM root directory:

```bash
# Configure the build (enabling PIC for shared module)
cmake -S . -B build -DCMAKE_POSITION_INDEPENDENT_CODE=ON

# Build the project (including pyuhdm)
cmake --build build -j$(nproc)

# The module will be located at:
# build/lib/pyuhdm.cpython-<version>-<platform>.so
```

## Python Usage Example

```python
import sys
# Adjust path to point to build/lib
sys.path.insert(0, 'build/lib')

import pyuhdm

# Create a serializer instance
s = pyuhdm.Serializer()

# Restore a UHDM binary file
# Returns a list of root handles (currently opaque capsules)
data = s.restore("design.uhdm")

print(f"Restored {len(data)} root objects.")

# Save to a new file
s.save("design_copy.uhdm")
```

### Exception Handling

Native C++ exceptions are propagated to Python as `RuntimeError`.

```python
import pyuhdm

s = pyuhdm.Serializer()

try:
    s.restore("nonexistent.uhdm")
except RuntimeError as e:
    print(f"UHDM Error: {e}")
```

## API Reference

### `pyuhdm.Serializer`

Direct binding to the native `uhdm::Serializer` C++ class.

#### Methods

| Method | Description |
|--------|-------------|
| `__init__()` | Create a new Serializer instance. |
| `restore(path: str) -> list` | Restore UHDM state from a binary file. Returns a list of root handles. |
| `save(path: str) -> None` | Save the current UHDM state to a binary file. |

**Note**: `restore()` returns a list of opaque handles. Inspection of these handles will be enabled in future milestones.

## Project Structure

```
python/pybind11/
├── CMakeLists.txt              # Core binding build configuration
├── module.cpp                  # Main pybind11 module definition
├── bindings/
│   └── bind_serializer.cpp     # Direct bindings for uhdm::Serializer
├── tests/
│   └── test_serializer.py      # Verification tests
└── README.md                   # This file
```

## Next Steps

Future milestones will add:
- Inspection of restored VPI handles
- Query methods for design hierarchy
- Expression tree traversal
