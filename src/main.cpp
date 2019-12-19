#include "headers/uhdm.h"
#include <iostream>
 
using namespace UHDM;

int main (int argc, char** argv) {
  // Design building
  design* d = new design();
  d->set_vpiName("design1");
  module* m1 = new module();
  m1->set_vpiTopModule(true);
  m1->set_vpiName("M1");
  m1->set_vpiParent(d);
  m1->set_uhdmParentType(uhdmdesign);
  module* m2 = new module();
  m2->set_vpiName("M2");
  m2->set_vpiParent(m1);
  m2->set_uhdmParentType(uhdmmodule);
  module* m3 = new module();
  m3->set_vpiName("M3");
  m3->set_vpiParent(m1);
  m3->set_uhdmParentType(uhdmmodule);
  VectorOfmodule* v1 = new VectorOfmodule();
  v1->push_back(m1);
  d->set_allModules(v1);
  VectorOfmodule* v2 = new VectorOfmodule();
  v2->push_back(m2);
  v2->push_back(m3);
  m1->set_modules(v2);
  vpiHandle design1 = (vpiHandle) new uhdm_handle(uhdmdesign, d);

  // VPI test
  vpiHandle modItr = vpi_iterate(uhdmallModules,design1); 
  while (vpiHandle obj_h = vpi_scan(modItr) ) {
    std::cout << "mod:" << vpi_get_str(vpiName, obj_h)
	      << ", top:" 
              << vpi_get(vpiTopModule, obj_h)
              << ", parent:" << vpi_get_str(vpiName, vpi_handle(vpiParent, obj_h));
    vpiHandle submodItr = vpi_iterate(vpiModule, obj_h); 
    while (vpiHandle sub_h = vpi_scan(submodItr) ) {
      std::cout << "\n  \\_ mod:" << vpi_get_str(vpiName, sub_h) 
		<< ", top:" << vpi_get(vpiTopModule, sub_h)
		<< ", parent:" << vpi_get_str(vpiName, vpi_handle(vpiParent, sub_h));
      vpi_release_handle (sub_h);
    }
    vpi_release_handle (submodItr);
    
    std::cout << std::endl;
    vpi_release_handle (obj_h);
  }
  vpi_release_handle(modItr);

  return 0;
};
