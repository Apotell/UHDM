#include <pybind11/pybind11.h>
#include "native_visitor_poc.h"

namespace py = pybind11;

// ID: 38
// Trampoline class for allowing Python overrides
class PyUhdmVisitor : public UhdmVisitor {
public:
    using UhdmVisitor::UhdmVisitor; // Inherit constructors

    void visit_Design(const uhdm::Design* obj) override {
        PYBIND11_OVERRIDE(
            void,           // Return type
            UhdmVisitor,    // Parent class
            visit_Design,   // Name of function in C++ (must match Python name if same, typically snake_case mapping)
            obj             // Argument(s)
        );
    }

    void visit_Module(const uhdm::Module* obj) override {
        PYBIND11_OVERRIDE(
            void,
            UhdmVisitor,
            visit_Module,
            obj
        );
    }

    void visit_Port(const uhdm::Port* obj) override {
        PYBIND11_OVERRIDE(
            void,
            UhdmVisitor,
            visit_Port,
            obj
        );
    }
};

void bind_native_visitor(py::module_& m) {
    py::class_<UhdmVisitor, PyUhdmVisitor>(m, "UhdmVisitor")
        .def(py::init<>())
        .def("visit_Design", &UhdmVisitor::visit_Design)
        .def("visit_Module", &UhdmVisitor::visit_Module)
        .def("visit_Port", &UhdmVisitor::visit_Port);

    m.def("traverse_design", &traverse_design, "Traverse design using a visitor");
}
