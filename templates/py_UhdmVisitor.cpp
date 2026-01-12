#include <uhdm/UhdmVisitor.h>
#include <uhdm/uhdm.h>

// Third party headers
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// System headers
#include <memory>

// Trampoline class for Python overrides
class PyUhdmVisitor : public uhdm::UhdmVisitor {
 public:
  using uhdm::UhdmVisitor::UhdmVisitor;  // Inherit constructors

  // clang-format off
  void visitAny(const uhdm::Any* object) override { PYBIND11_OVERRIDE(void, uhdm::UhdmVisitor, visitAny, object); }
// <VISIT_ANY_FUNCTIONS>

// <VISIT_MANY_FUNCTIONS>
  // clang-format on
};

void bind_UhdmVisitor(pybind11::module_& m) {
  pybind11::class_<uhdm::UhdmVisitor, PyUhdmVisitor>(m, "UhdmVisitor")
      .def(pybind11::init<>())
      .def("visit", &uhdm::UhdmVisitor::visit)
      .def("visitAny", &uhdm::UhdmVisitor::visitAny)
      // clang-format off
// <VISIT_ANY_DEFS>

// <VISIT_MANY_DEFS>
      // clang-format on
      ;
}
