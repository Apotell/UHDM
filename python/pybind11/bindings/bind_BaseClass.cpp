#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <uhdm/BaseClass.h>

namespace py = pybind11;

void bind_BaseClass(py::module& m) {
    py::class_<uhdm::BaseClass, std::unique_ptr<uhdm::BaseClass, py::nodelete>> cls(m, "BaseClass");
    
    cls.def_property_readonly("vpiName", &uhdm::BaseClass::getName);
    cls.def_property_readonly("vpiDefName", &uhdm::BaseClass::getDefName);
    cls.def_property_readonly("vpiFile", &uhdm::BaseClass::getFile);
    cls.def_property_readonly("vpiLineNo", &uhdm::BaseClass::getStartLine);
    cls.def_property_readonly("vpiColumnNo", &uhdm::BaseClass::getStartColumn);
    cls.def_property_readonly("vpiEndLineNo", &uhdm::BaseClass::getEndLine);
    cls.def_property_readonly("vpiEndColumnNo", &uhdm::BaseClass::getEndColumn);
    
    cls.def_property_readonly("uhdmType", &uhdm::BaseClass::getUhdmType);
    cls.def_property_readonly("vpiType", &uhdm::BaseClass::getVpiType);
    
    cls.def_property_readonly("vpiParent", [](uhdm::BaseClass* self) {
        return self->getParent();
    }, py::return_value_policy::reference);
}
