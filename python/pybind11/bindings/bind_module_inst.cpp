#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <uhdm/module.h>
#include <uhdm/port.h>

namespace py = pybind11;

void bind_module_inst(py::module_& m) {
    // Expose uhdm::Module as "module_inst"
    py::class_<uhdm::Module, uhdm::BaseClass, std::unique_ptr<uhdm::Module, py::nodelete>>(m, "module_inst")
        .def("getDefName", &uhdm::Module::getDefName)
        .def("getPorts", [](uhdm::Module& m) {
             py::list list;
             const auto* ports = m.getPorts();
             if (ports) {
                 for (auto* port : *ports) {
                     // Manual conversion, returning plain object (likely BaseClass/opaque if not bound)
                     // Since Port is not bound, it's opaque for now.
                     // reference_internal keeps it alive as long as Module is alive.
                     list.append(py::cast(port, py::return_value_policy::reference_internal, py::cast(&m)));
                 }
             }
             return list;
        });
}
