#include <uhdm/VpiListener.h>
#include <uhdm/uhdm.h>

// Third party headers
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// System headers
#include <memory>

// Trampoline for Python overrides
class PyVpiListener : public uhdm::VpiListener {
 public:
  using uhdm::VpiListener::VpiListener;

  // clang-format off
  void enterAny(const uhdm::Any* object, vpiHandle handle) override { PYBIND11_OVERRIDE(void, uhdm::VpiListener, enterAny, object, handle); }
  void leaveAny(const uhdm::Any* object, vpiHandle handle) override { PYBIND11_OVERRIDE(void, uhdm::VpiListener, leaveAny, object, handle); }

// <ENTER_LEAVE_FUNCTIONS>
  // clang-format on
};

void bind_VpiListener(pybind11::module_& m) {
  pybind11::class_<uhdm::VpiListener, PyVpiListener>(m, "VpiListener")
      .def(pybind11::init<>())
      .def("listen_Any", &uhdm::VpiListener::listenAny)
      .def("enter_Any", &uhdm::VpiListener::enterAny)
      .def("leave_Any", &uhdm::VpiListener::leaveAny)
      // clang-format off
// <ENTER_LEAVE_DEFS>
      // clang-format on
      ;
}
