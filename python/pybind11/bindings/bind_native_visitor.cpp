#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "../native_visitor_poc.h"

namespace py = pybind11;

// Trampoline class for Python overrides
class PyUhdmVisitor : public uhdm::UhdmVisitor {
public:
    using uhdm::UhdmVisitor::UhdmVisitor; // Inherit constructors

    void visit(const uhdm::BaseClass* obj) override {
        PYBIND11_OVERRIDE(
            void,               // Return type
            uhdm::UhdmVisitor,  // Parent class
            visit,              // Name of function in C++ (must match Python method name)
            obj                 // Argument(s)
        );
    }

    void visit(const uhdm::Design* obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::UhdmVisitor,
            visit,
            obj
        );
    }

    void visit(const uhdm::Module* obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::UhdmVisitor,
            visit,
            obj
        );
    }

    void visit(const uhdm::Port* obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::UhdmVisitor,
            visit,
            obj
        );
    }
};

void bind_native_visitor(py::module_& m) {
    py::class_<uhdm::UhdmVisitor, PyUhdmVisitor>(m, "UhdmVisitor")
        .def(py::init<>())
        .def("visit", [](uhdm::UhdmVisitor& self, const uhdm::BaseClass* obj) { self.visit(obj); })
        .def("visit", [](uhdm::UhdmVisitor& self, const uhdm::Design* obj) { self.visit(obj); })
        .def("visit", [](uhdm::UhdmVisitor& self, const uhdm::Module* obj) { self.visit(obj); })
        .def("visit", [](uhdm::UhdmVisitor& self, const uhdm::Port* obj) { self.visit(obj); });

    m.def("traverse_design", &uhdm::traverse_design, "Minimal PoC traversal of Design");
}
