#include <pybind11/pybind11.h>

namespace py = pybind11;

void bind_any(py::module_& m) {
    // NOTE:
    // In UHDM, `uhdm::any` is a type alias to `uhdm::BaseClass`
    // (using any = BaseClass).
    // Therefore, no separate Python binding is required here.
    // The root UHDM object is represented by pyuhdm.BaseClass.
}
