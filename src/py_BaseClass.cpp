#include <uhdm/BaseClass.h>

// Third party headers
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// System headers
#include <memory>

void bind_BaseClass(pybind11::module_& m) {
  pybind11::class_<uhdm::BaseClass, std::unique_ptr<uhdm::BaseClass, pybind11::nodelete>>(m, "BaseClass")
      .def_property("uhdm_id", &uhdm::BaseClass::getUhdmId, &uhdm::BaseClass::setUhdmId)
      .def_property("parent",
                    pybind11::cpp_function([](uhdm::BaseClass* self) { return self->getParent(); },
                                           pybind11::return_value_policy::reference),
                    pybind11::cpp_function(&uhdm::BaseClass::setParent))
      .def_property("file", &uhdm::BaseClass::getFile, &uhdm::BaseClass::setFile)
      .def_property("start_line", &uhdm::BaseClass::getStartLine, &uhdm::BaseClass::setStartLine)
      .def_property("start_column", &uhdm::BaseClass::getStartColumn, &uhdm::BaseClass::setStartColumn)
      .def_property("end_line", &uhdm::BaseClass::getEndLine, &uhdm::BaseClass::setEndLine)
      .def_property("end_column", &uhdm::BaseClass::getEndColumn, &uhdm::BaseClass::setEndColumn)
      .def_property_readonly("name", &uhdm::BaseClass::getName)
      .def_property_readonly("def_name", &uhdm::BaseClass::getDefName)
      .def_property_readonly("vpi_type", &uhdm::BaseClass::getVpiType)
      .def_property_readonly("uhdm_type", &uhdm::BaseClass::getUhdmType);
}
