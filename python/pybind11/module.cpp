#include <pybind11/pybind11.h>

namespace py = pybind11;

// Forward declarations
void bind_serializer(py::module& m);

PYBIND11_MODULE(pyuhdm, m) {
    m.doc() = "UHDM Python bindings";

    bind_serializer(m);
}
