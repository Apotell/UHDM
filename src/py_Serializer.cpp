#include <uhdm/Serializer.h>
#include <uhdm/design.h>
#include <uhdm/vpi_uhdm.h>

// Third party headers
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// System headers
#include <memory>

void bind_Serializer(pybind11::module& m) {
  pybind11::class_<uhdm::Serializer>(m, "Serializer",
                                     "The Serializer class manages the creation, saving, and restoring of UHDM models.")
      .def(pybind11::init<>(), "Create a new Serializer instance.")
      .def("save", pybind11::overload_cast<const std::string&>(&uhdm::Serializer::save), pybind11::arg("filepath"),
           "Save the current UHDM state to a binary file.")
      .def(
          "restore",
          [](uhdm::Serializer& self, const std::string& filepath) {
            std::vector<vpiHandle> handles = self.restore(filepath);
            std::vector<uhdm::Design*> objects;
            objects.reserve(handles.size());
            std::transform(handles.begin(), handles.end(), std::back_inserter(objects), UhdmDesignFromVpiHandle);
            return objects;
          },
          pybind11::arg("filepath"), pybind11::return_value_policy::reference,
          "Restore UHDM state from a binary file.\n"
          "Returns a list of root objects (Design, etc).");
}
