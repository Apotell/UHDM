#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "../native_listener_poc.h"

namespace py = pybind11;

// Trampoline class for Python overrides
class PyUhdmListener : public uhdm::UhdmListener {
public:
    using uhdm::UhdmListener::UhdmListener; // Inherit constructors

    void enter(const uhdm::BaseClass* obj) override {
        PYBIND11_OVERRIDE(
            void,               // Return type
            uhdm::UhdmListener, // Parent class
            enter,              // Name of function in C++ (must match Python method name)
            obj                 // Argument(s)
        );
    }
    void leave(const uhdm::BaseClass* obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::UhdmListener,
            leave,
            obj
        );
    }

    void enter(const uhdm::Design* obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::UhdmListener,
            enter,
            obj
        );
    }
    void leave(const uhdm::Design* obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::UhdmListener,
            leave,
            obj
        );
    }

    void enter(const uhdm::Module* obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::UhdmListener,
            enter,
            obj
        );
    }
    void leave(const uhdm::Module* obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::UhdmListener,
            leave,
            obj
        );
    }

    void enter(const uhdm::Port* obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::UhdmListener,
            enter,
            obj
        );
    }
    void leave(const uhdm::Port* obj) override {
        PYBIND11_OVERRIDE(
            void,
            uhdm::UhdmListener,
            leave,
            obj
        );
    }
};

void bind_native_listener(py::module_& m) {
    py::class_<uhdm::UhdmListener, PyUhdmListener>(m, "UhdmListener")
        .def(py::init<>())
        // enter() overloads
        .def("enter", [](uhdm::UhdmListener& self, const uhdm::BaseClass* obj) { self.enter(obj); })
        .def("enter", [](uhdm::UhdmListener& self, const uhdm::Design* obj) { self.enter(obj); })
        .def("enter", [](uhdm::UhdmListener& self, const uhdm::Module* obj) { self.enter(obj); })
        .def("enter", [](uhdm::UhdmListener& self, const uhdm::Port* obj) { self.enter(obj); })
        // leave() overloads
        .def("leave", [](uhdm::UhdmListener& self, const uhdm::BaseClass* obj) { self.leave(obj); })
        .def("leave", [](uhdm::UhdmListener& self, const uhdm::Design* obj) { self.leave(obj); })
        .def("leave", [](uhdm::UhdmListener& self, const uhdm::Module* obj) { self.leave(obj); })
        .def("leave", [](uhdm::UhdmListener& self, const uhdm::Port* obj) { self.leave(obj); });

    m.def("walk_design", &uhdm::walk_design, "Traverse Design with enter/leave callbacks");
}
