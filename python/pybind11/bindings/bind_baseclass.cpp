#include <pybind11/pybind11.h>
#include <uhdm/BaseClass.h>

namespace py = pybind11;

void bind_baseclass(py::module_& m) {
    py::class_<uhdm::BaseClass, std::unique_ptr<uhdm::BaseClass, py::nodelete>>(m, "BaseClass")
        .def("getUhdmId", &uhdm::BaseClass::getUhdmId)
        .def("getFile", &uhdm::BaseClass::getFile)
        .def("getStartLine", &uhdm::BaseClass::getStartLine)
        .def("getStartColumn", &uhdm::BaseClass::getStartColumn)
        .def("getEndLine", &uhdm::BaseClass::getEndLine)
        .def("getEndColumn", &uhdm::BaseClass::getEndColumn)
        .def("getVpiType", &uhdm::BaseClass::getVpiType)
        .def("getUhdmType", &uhdm::BaseClass::getUhdmType);
}
