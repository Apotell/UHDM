#pragma once

#include <uhdm/uhdm.h>
#include <uhdm/vpi_user.h>

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

    // Get type
    // In strict VPI we use vpi_get(vpiType, root).
    // UHDM vpi_user.h defines vpiType property and constants like vpiDesign, vpiModule.
    
    // For this PoC, we assume direct checking or usage of uhdm public API if vpi_* functions aren't directly linked/mocked easily here without surelog runtime.
    // However, the prompt says "Do NOT implement full VPI traversal", "Do NOT depend on Surelog runtime beyond basic vpiHandle usage".
    // AND "UHDM" library implements vpi_get etc? 
    // Usually standard VPI functions are exported by the simulator (or UHDM acting as one).
    // uhdm::uhdm_get(vpiType, handle) matches.
    
    uhdm::UhdmType type = ((const BaseClass*)root)->getUhdmType();
    
    if (type == uhdm::UhdmType::Design) {
        listener.on_design(root);
        
        const Design* design = (const Design*)root;
        if (auto* top_modules = design->getTopModules()) {
            for (auto* module : *top_modules) {
                // For each module
                listener.on_object((vpiHandle)module);
                listener.on_module((vpiHandle)module);
            }
        }
    } else if (type == uhdm::UhdmType::Module) {
        listener.on_module(root);
    }
}

} // namespace uhdm
