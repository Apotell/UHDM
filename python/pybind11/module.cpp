#include <pybind11/pybind11.h>

namespace py = pybind11;

// Forward declarations
void bind_serializer(py::module& m);
void bind_all_autogen(py::module_& m);
void bind_native_visitor(py::module_& m);
void bind_native_listener(py::module_& m);

PYBIND11_MODULE(pyuhdm, m) {
    m.doc() = "UHDM Python bindings";

    bind_serializer(m);
    bind_all_autogen(m);
    bind_native_visitor(m);
    bind_native_listener(m);
}
