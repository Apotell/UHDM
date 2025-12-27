#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <uhdm/Serializer.h>

namespace py = pybind11;

void bind_serializer(py::module& m) {
    // Bind uhdm::Serializer class
    py::class_<uhdm::Serializer>(m, "Serializer",
        "The Serializer class manages the creation, saving, and restoring of UHDM models.")
        .def(py::init<>(), "Create a new Serializer instance.")
        .def("save", py::overload_cast<const std::string&>(&uhdm::Serializer::save),
            py::arg("filepath"),
            "Save the current UHDM state to a binary file.")
        .def("restore", py::overload_cast<const std::string&>(&uhdm::Serializer::restore),
            py::arg("filepath"),
            "Restore UHDM state from a binary file.\n"
            "Returns a list of root handles.");
}
