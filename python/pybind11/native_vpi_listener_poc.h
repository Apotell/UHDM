#pragma once

#include <uhdm/uhdm.h>
#include <uhdm/vpi_user.h>
#include <uhdm/vpi_uhdm.h>

#include <iostream>

namespace uhdm {

// Mimic vpi_user.h defines if not available, but we included <uhdm/vpi_user.h>
// Assuming vpiHandle is available.

class VpiListener {
public:
  virtual ~VpiListener() = default;

  // Called for every visited VPI object
  virtual void on_object(vpiHandle obj) {}

  // Optional typed hooks (PoC subset)
  virtual void on_design(vpiHandle obj) {}
  virtual void on_module(vpiHandle obj) {}
};

inline void walk_vpi(vpiHandle root, VpiListener& listener) {
    if (!root) return;

    // Notify generic
    listener.on_object(root);

    // Unwrap handle
    const uhdm_handle* handle = (const uhdm_handle*)root;
    const BaseClass* obj = (const BaseClass*)handle->object;

    uhdm::UhdmType type = obj->getUhdmType();
    
    if (type == uhdm::UhdmType::Design) {
        listener.on_design(root);
        
        const Design* design = (const Design*)obj;
        if (auto* top_modules = design->getTopModules()) {
            for (auto* module : *top_modules) {
                // For each module
                vpiHandle h_mod = NewVpiHandle(module);
                listener.on_object(h_mod);
                listener.on_module(h_mod);
                // In full implementation we should track/free handles
            }
        }
    } else if (type == uhdm::UhdmType::Module) {
        listener.on_module(root);
    }
}

} // namespace uhdm
