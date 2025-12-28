#include <pybind11/pybind11.h>

namespace py = pybind11;

// Forward declarations
void bind_serializer(py::module& m);
void bind_any(py::module& m);
void bind_baseclass(py::module& m);
void bind_enums(py::module& m);

PYBIND11_MODULE(pyuhdm, m) {
    m.doc() = "UHDM Python bindings";

    bind_serializer(m);
    bind_any(m);
    bind_baseclass(m);
    bind_enums(m);
}
