#include <pybind11/pybind11.h>

namespace py = pybind11;

// Forward declarations
void bind_Design(py::module_& m);
void bind_Scope(py::module_& m);
void bind_Instance(py::module_& m);
void bind_Module(py::module_& m);
void bind_Ports(py::module_& m);
void bind_Port(py::module_& m);

void bind_all_autogen(py::module_& m) {
  bind_Design(m);
  
  // Dependencies: Module -> Instance -> Scope
  bind_Scope(m);
  bind_Instance(m);
  bind_Module(m);
  
  // Dependencies: Port -> Ports
  bind_Ports(m);
  bind_Port(m);
}
