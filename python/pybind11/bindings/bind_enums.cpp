#include <pybind11/pybind11.h>
#include <uhdm/uhdm_types.h>

namespace py = pybind11;

void bind_enums(py::module_& m) {
    py::enum_<uhdm::UhdmType>(m, "UhdmType")
        .value("Design", uhdm::UhdmType::Design);
}
