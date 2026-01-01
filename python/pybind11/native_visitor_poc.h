#pragma once

#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/port.h>

// PoC only: visitor API subject to change
class UhdmVisitor {
public:
  virtual ~UhdmVisitor() = default;

  virtual void visit_Design(const uhdm::Design* obj) {}
  virtual void visit_Module(const uhdm::Module* obj) {}
  virtual void visit_Port(const uhdm::Port* obj) {}
};

inline void traverse_design(const uhdm::Design* design, UhdmVisitor& visitor) {
    if (!design) return;

    visitor.visit_Design(design);

    if (auto* modules = design->getTopModules()) {
        for (auto* module : *modules) {
            visitor.visit_Module(module);
            
            if (auto* ports = module->getPorts()) {
                for (auto* port : *ports) {
                    visitor.visit_Port(port);
                }
            }
        }
    }
}
