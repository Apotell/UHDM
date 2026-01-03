#pragma once

#include <uhdm/uhdm.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/port.h>

#include <iostream>
#include <vector>

namespace uhdm {

class UhdmListener {
public:
  virtual ~UhdmListener() = default;

  // Generic fallback (optional)
  virtual void enter(const BaseClass* obj) {}
  virtual void leave(const BaseClass* obj) {}

  // Typed hooks (PoC subset only)
  virtual void enter(const Design* obj) {}
  virtual void leave(const Design* obj) {}

  virtual void enter(const Module* obj) {}
  virtual void leave(const Module* obj) {}

  virtual void enter(const Port* obj) {}
  virtual void leave(const Port* obj) {}
};

inline void walk_design(const Design* design, UhdmListener& listener) {
  if (!design) return;

  // enter(Design)
  listener.enter(design);

  if (auto* top_modules = design->getTopModules()) {
    for (auto* module : *top_modules) {
      // enter(Module)
      listener.enter(module);

      if (auto* ports = module->getPorts()) {
        for (auto* port : *ports) {
          // enter(Port)
          listener.enter(port);

          // leave(Port)
          listener.leave(port);
        }
      }

      // leave(Module)
      listener.leave(module);
    }
  }

  // leave(Design)
  listener.leave(design);
}

} // namespace uhdm
