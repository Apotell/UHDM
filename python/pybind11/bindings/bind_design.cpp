#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <uhdm/design.h>
#include <uhdm/module.h>

namespace py = pybind11;

void bind_design(py::module_& m) {
    py::class_<uhdm::Design, uhdm::BaseClass, std::unique_ptr<uhdm::Design, py::nodelete>>(m, "Design")
        .def("getName", &uhdm::Design::getName)
        .def("getTopModules", [](uhdm::Design& d) {
            py::list list;
            const auto* modules = d.getTopModules();
            if (modules) {
                for (auto* mod : *modules) {
                    // Manual conversion to opaque/bound pointer
                    // Since Module is not bound, this will be opaque or cast to BaseClass if RTTI works?
                    // Pybind11 will try to find a registered type for 'mod' (which is Module*).
                    // If Module is not registered, it might return a capsule or error if we don't have opaque decl.
                    // But we want to return a list of things.
                    // return_value_policy::reference_internal ensures Python doesn't own them.
                    list.append(py::cast(mod, py::return_value_policy::reference_internal, py::cast(&d)));
                }
            }
            return list;
        });
}
