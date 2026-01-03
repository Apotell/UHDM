#pragma once

#include <uhdm/uhdm.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/port.h>

#include <iostream>
#include <vector>

namespace uhdm {

class UhdmVisitor {
public:
  virtual ~UhdmVisitor() = default;

  // Generic fallback
  virtual void visit(const BaseClass* obj) {}

  // Typed hooks (PoC subset only)
  virtual void visit(const Design* obj) {
      visit(static_cast<const BaseClass*>(obj));
  }
  
  virtual void visit(const Module* obj) {
      visit(static_cast<const BaseClass*>(obj));
  }
  
  virtual void visit(const Port* obj) {
      visit(static_cast<const BaseClass*>(obj));
  }
};

inline void traverse_design(const Design* design, UhdmVisitor& visitor) {
  if (!design) return;

  visitor.visit(design);

  if (auto* top_modules = design->getTopModules()) {
    for (auto* module : *top_modules) {
      visitor.visit(module);

      if (auto* ports = module->getPorts()) {
        for (auto* port : *ports) {
          visitor.visit(port);
        }
      }
    }
  }
}

} // namespace uhdm
