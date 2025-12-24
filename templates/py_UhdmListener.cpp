#include <uhdm/UhdmListener.h>
#include <uhdm/uhdm.h>

// Third party headers
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// System headers
#include <memory>

// Trampoline class for Python overrides
class PyUhdmListener : public uhdm::UhdmListener {
 public:
  using uhdm::UhdmListener::UhdmListener;  // Inherit constructors

  // clang-format off
  void enterAny(const uhdm::Any* object, uint32_t vpiRelation) override { PYBIND11_OVERRIDE(void, uhdm::UhdmListener, enterAny, object, vpiRelation); }
  void leaveAny(const uhdm::Any* object, uint32_t vpiRelation) override { PYBIND11_OVERRIDE(void, uhdm::UhdmListener, leaveAny, object, vpiRelation); }

// <ENTER_LEAVE_ANY_FUNCTIONS>

// <ENTER_LEAVE_MANY_FUNCTIONS>
  // clang-format on
};

void bind_UhdmListener(pybind11::module_& m) {
  pybind11::class_<uhdm::UhdmListener, PyUhdmListener>(m, "UhdmListener")
      .def(pybind11::init<>())
      .def("listen_Any", &uhdm::UhdmListener::listenAny)
      .def("enter_Any", &uhdm::UhdmListener::enterAny)
      .def("leave_Any", &uhdm::UhdmListener::leaveAny)
      // clang-format off
// <ENTER_LEAVE_ANY_DEFS>

// <ENTER_LEAVE_MANY_DEFS>
      // clang-format on
      ;
}
