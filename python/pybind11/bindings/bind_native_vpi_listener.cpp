#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "../native_vpi_listener_poc.h"

namespace py = pybind11;

// Trampoline for Python overrides
class PyVpiListener : public uhdm::VpiListener {
public:
    using uhdm::VpiListener::VpiListener;

    void on_object(vpiHandle obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::VpiListener,
            on_object,
            obj
        );
    }

    void on_design(vpiHandle obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::VpiListener,
            on_design,
            obj
        );
    }

    void on_module(vpiHandle obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::VpiListener,
            on_module,
            obj
        );
    }
};

void bind_native_vpi_listener(py::module_& m) {
    py::class_<uhdm::VpiListener, PyVpiListener>(m, "VpiListener")
        .def(py::init<>())
        .def("on_object", &uhdm::VpiListener::on_object)
        .def("on_design", &uhdm::VpiListener::on_design)
        .def("on_module", &uhdm::VpiListener::on_module);

    m.def("walk_vpi", &uhdm::walk_vpi, "Traverse VPI hierarchy (PoC)");
}
