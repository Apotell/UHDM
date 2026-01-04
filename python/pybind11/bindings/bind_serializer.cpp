#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <uhdm/uhdm.h>
#include <uhdm/vpi_uhdm.h>

namespace py = pybind11;

void bind_serializer(py::module& m) {
    // Bind uhdm::Serializer class
    py::class_<uhdm::Serializer>(m, "Serializer",
        "The Serializer class manages the creation, saving, and restoring of UHDM models.")
        .def(py::init<>(), "Create a new Serializer instance.")
        .def("save", py::overload_cast<const std::string&>(&uhdm::Serializer::save),
            py::arg("filepath"),
            "Save the current UHDM state to a binary file.")
        .def("restore", [](uhdm::Serializer& self, const std::string& filepath) {
            std::vector<vpiHandle> handles = self.restore(filepath);
            std::vector<uhdm::BaseClass*> objects;
            objects.reserve(handles.size());
            for (auto h : handles) {
                if (h) {
                    const uhdm_handle* wrapper = (const uhdm_handle*)h;
                    if (wrapper->object) {
                        objects.push_back((uhdm::BaseClass*)wrapper->object);
                    }
                }
            }
            return objects;
        }, py::arg("filepath"), py::return_value_policy::reference,
           "Restore UHDM state from a binary file.\n"
           "Returns a list of root objects (Design, etc).");
}
