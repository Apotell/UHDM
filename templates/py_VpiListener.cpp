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

  // Internal global VPI context pointer (void*)
  // Defaults to nullptr. safe to set from py::capsule.
  static void* g_vpi_context = nullptr;

  m.def("set_vpi_context", [](pybind11::capsule capsule) {
      if (std::string_view(capsule.name()) == "uhdm.vpi_context") {
          void* ptr = capsule.get_pointer();
          if (ptr == nullptr) {
             throw std::runtime_error("VPI context capsule contains nullptr.");
          }
          g_vpi_context = ptr;
          return true;
      }
      throw std::runtime_error("Invalid capsule name. Expected 'uhdm.vpi_context'.");
  }, "Sets the global VPI context from a 'uhdm.vpi_context' capsule. Throws if invalid.");

  m.def("clear_vpi_context", []() {
      g_vpi_context = nullptr;
  }, "Clears the global VPI context pointer (sets to nullptr).");

  m.def("walk_vpi", [](uhdm::Design* design, uhdm::VpiListener* listener) {
      if (g_vpi_context == nullptr) {
          throw std::runtime_error("No VPI runtime context available (g_vpi_context is null).");
      }

      if (design == nullptr || listener == nullptr) {
           return;
      }

      // Minimal traversal logic
      // Create a specific vpiHandle for Design using the helper if available, or just cast if purely internal.
      // Since NewVpiHandle is in vpi_user.cpp but might not be exposed in this translation unit easily without headers.
      // We'll rely on uhdm::NewVpiHandle if linker allows, or just pass generic handle if acceptable for PoC.
      
      // Prototyping: Assuming NewVpiHandle is available (it is in vpi_user.cpp, we might need to declare it)
      // Extern ref in case it's not included
      
      listener->enterDesign(design, nullptr); // Handle is optional/context dependent

      if (design->getTopModules()) {
          for (auto mod : *design->getTopModules()) {
               listener->enterModule(mod, nullptr);
               listener->leaveModule(mod, nullptr);
          }
      }
      
      listener->leaveDesign(design, nullptr);

  }, "Traverses the design (Design -> TopModules) and invokes listener callbacks. Requires active VPI context.");
}
