#include <uhdm/uhdm_types.h>

// Third party headers
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// System headers
#include <memory>

void bind_UhdmType(pybind11::module_& m) {
  pybind11::enum_<uhdm::UhdmType>(m, "UhdmType")
      // clang-format off
// <ENUM_VALUES>
      // clang-format on
      ;
}
