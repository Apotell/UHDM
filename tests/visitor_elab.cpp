// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

/*

 Copyright 2020 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   visitor_elab
 * Author:
 *
 * Created on May 4, 2020, 10:03 PM
 */


#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <stack>

#include "headers/uhdm.h"
#include "headers/vpi_listener.h"
#include "headers/vpi_visitor.h"

using namespace UHDM;

std::vector<vpiHandle> build_designs (Serializer& s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s.MakeDesign();
  d->VpiName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  module* m1 = s.MakeModule();
  {
    m1->VpiDefName("M1");
    m1->VpiParent(d);
    m1->VpiFile("fake1.sv");
    m1->VpiLineNo(10);
  }

  //-------------------------------------------
  // Module definition M2 (non elaborated)
  module* m2 = s.MakeModule();
  {
    m2->VpiDefName("M2");
    m2->VpiFile("fake2.sv");
    m2->VpiLineNo(20);
    // M2 Ports
    VectorOfport* vp = s.MakePortVec();
    port* p = s.MakePort();
    p->VpiName("i1");
    p->VpiDirection(vpiInput);
    vp->push_back(p);
    p = s.MakePort();
    p->VpiName("o1");
    p->VpiDirection(vpiOutput);
    vp->push_back(p);
    m2->Ports(vp);
    // M2 Nets
    VectorOfnet* vn = s.MakeNetVec();
    logic_net* n = s.MakeLogic_net();
    n->VpiName("i1");
    vn->push_back(n);
    n = s.MakeLogic_net();
    n->VpiName("o1");
    vn->push_back(n);
    m2->Nets(vn);
  
    // M2 continuous assignment
    VectorOfcont_assign* assigns = s.MakeCont_assignVec();
    cont_assign* cassign = s.MakeCont_assign();
    assigns->push_back(cassign);
    ref_obj* lhs = s.MakeRef_obj();
    ref_obj* rhs = s.MakeRef_obj();
    lhs->VpiName("o1");
    rhs->VpiName("i1");
    cassign->Lhs(lhs);
    cassign->Rhs(rhs);
    m2->Cont_assigns(assigns);
  }
  
 
  //-------------------------------------------
  // Instance tree (Elaborated tree)
  // Top level module
  module* m3 = s.MakeModule();
  VectorOfmodule* v1 = s.MakeModuleVec();
  {
    m3->VpiDefName("M1"); // Points to the module def (by name)
    m3->VpiName("M1");    // Instance name
    m3->VpiTopModule(true);
    m3->Modules(v1);
  }
  
  //-------------------------------------------
  // Sub Instance
  module* m4 = s.MakeModule();
  {
    m4->VpiDefName("M2"); // Points to the module def (by name)
    m4->VpiName("inst1"); // Instance name
    m4->VpiFullName("M1.inst1"); // Instance full name
    VectorOfport* inst_vp = s.MakePortVec(); // Create elaborated ports
    m4->Ports(inst_vp);
    port* p1 = s.MakePort();
    p1->VpiName("i1");
    inst_vp->push_back(p1);
    port* p2 = s.MakePort();
    p2->VpiName("o1");
    inst_vp->push_back(p2);
    // M2 Nets
    VectorOfnet* vn = s.MakeNetVec(); // Create elaborated nets
    logic_net* n = s.MakeLogic_net();
    n->VpiName("i1");
    n->VpiFullName("M1.inst.i1");
    ref_obj* low_conn = s.MakeRef_obj();
    low_conn->Actual_group(n);
    low_conn->VpiName("i1");
    p1->Low_conn(low_conn);
    vn->push_back(n);
    n = s.MakeLogic_net();
    n->VpiName("o1");
    n->VpiFullName("M1.inst.o1");
    low_conn = s.MakeRef_obj();
    low_conn->Actual_group(n);
    low_conn->VpiName("o1");
    p2->Low_conn(low_conn);
    vn->push_back(n);
    m4->Nets(vn);
    
  }
  
  // Create parent-child relation in between the 2 modules in the instance tree
  v1->push_back(m4);
  m4->VpiParent(m3);

  //-------------------------------------------
  // Create both non-elaborated and elaborated lists 
  VectorOfmodule* allModules = s.MakeModuleVec();
  d->AllModules(allModules);
  allModules->push_back(m1);
  allModules->push_back(m2);

  VectorOfmodule* topModules = s.MakeModuleVec();
  d->TopModules(topModules);
  topModules->push_back(m3); // Only m3 goes there as it is the top level module
  
  vpiHandle dh = s.MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  return designs;
}




class MyElaboratorListener : public VpiListener {
protected:
  void enterModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    std::cout << "Module: " << object->VpiDefName() << " (" << object->VpiName() << ")" << std::endl;
    componentMap_.insert(std::make_pair(object->VpiDefName(), object));
    
    stack_.push(object);
  }

  void leaveModule(const module* object, const BaseClass* parent,
                   vpiHandle handle, vpiHandle parentHandle) override {
    stack_.pop();
  }


private:
  std::stack<const BaseClass*> stack_;

  std::map<std::string, const BaseClass*> componentMap_;
};

int main (int argc, char** argv) {

  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(serializer);
  std::string orig;
  orig += "VISITOR:\n";
  orig += visit_designs(designs);
  std::cout << orig;
 
  MyElaboratorListener* listener = new MyElaboratorListener();
  listen_designs(designs,listener);
  return 0;
}

