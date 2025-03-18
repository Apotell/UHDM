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
 * File:   listener_elab
 * Author: alain
 *
 * Created on May 4, 2020, 10:03 PM
 */

#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <vector>

// Verifies that the forward declaration header compiles
#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/VpiListener.h"
#include "uhdm/uhdm.h"
#include "uhdm/uhdm_forward_decl.h"
#include "uhdm/vpi_visitor.h"

using namespace uhdm;

//-------------------------------------------
// This self-contained example demonstrate how one can navigate the Folded Model
// of UHDM By extracting the complete information in between in the instance
// tree and the module definitions using the Listener Design Pattern
//-------------------------------------------

//-------------------------------------------
// Unit test design

std::vector<vpiHandle> build_designs(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  Module* m1 = s->make<Module>();
  {
    m1->setDefName("M1");
    m1->setParent(d);
    m1->setFile("fake1.sv");
    m1->setStartLine(10);
  }

  //-------------------------------------------
  // Module definition M2 (non elaborated)
  Module* m2 = s->make<Module>();
  {
    m2->setDefName("M2");
    m2->setFile("fake2.sv");
    m2->setStartLine(20);
    m2->setParent(d);
    // M2 Ports
    PortCollection* vp = s->makeCollection<Port>();
    Port* p = s->make<Port>();
    p->setName("i1");
    p->setDirection(vpiInput);
    vp->push_back(p);
    p = s->make<Port>();
    p->setName("o1");
    p->setDirection(vpiOutput);
    vp->push_back(p);
    m2->setPorts(vp);
    // M2 Nets
    NetCollection* vn = s->makeCollection<Net>();
    LogicNet* n = s->make<LogicNet>();
    n->setName("i1");
    vn->push_back(n);
    n = s->make<LogicNet>();
    n->setName("o1");
    vn->push_back(n);
    m2->setNets(vn);

    // M2 continuous assignment
    ContAssignCollection* assigns = s->makeCollection<ContAssign>();
    ContAssign* cassign = s->make<ContAssign>();
    assigns->push_back(cassign);
    RefObj* lhs = s->make<RefObj>();
    RefObj* rhs = s->make<RefObj>();
    lhs->setName("o1");
    rhs->setName("i1");
    cassign->setLhs(lhs);
    cassign->setRhs(rhs);
    m2->setContAssigns(assigns);
  }

  //-------------------------------------------
  // Instance tree (Elaborated tree)
  // Top level module
  Module* m3 = s->make<Module>();
  ModuleCollection* v1 = s->makeCollection<Module>();
  {
    m3->setDefName("M1");  // Points to the module def (by name)
    m3->setName("M1");     // Instance name
    m3->setTopModule(true);
    m3->setModules(v1);
    m3->setParent(d);
  }

  //-------------------------------------------
  // Sub Instance
  Module* m4 = s->make<Module>();
  {
    m4->setDefName("M2");         // Points to the module def (by name)
    m4->setName("inst1");         // Instance name
    m4->setFullName("M1.inst1");  // Instance full name
    PortCollection* inst_vp =
        s->makeCollection<Port>();  // Create elaborated ports
    m4->setPorts(inst_vp);
    Port* p1 = s->make<Port>();
    p1->setName("i1");
    inst_vp->push_back(p1);
    Port* p2 = s->make<Port>();
    p2->setName("o1");
    inst_vp->push_back(p2);
    // M2 Nets
    NetCollection* vn = s->makeCollection<Net>();  // Create elaborated nets
    LogicNet* n = s->make<LogicNet>();
    n->setName("i1");
    n->setFullName("M1.inst.i1");
    RefObj* low_conn = s->make<RefObj>();
    low_conn->setActual(n);
    low_conn->setName("i1");
    p1->setLowConn(low_conn);
    vn->push_back(n);
    n = s->make<LogicNet>();
    n->setName("o1");
    n->setFullName("M1.inst.o1");
    low_conn = s->make<RefObj>();
    low_conn->setActual(n);
    low_conn->setName("o1");
    p2->setLowConn(low_conn);
    vn->push_back(n);
    m4->setNets(vn);
  }

  // Create parent-child relation in between the 2 modules in the instance tree
  v1->push_back(m4);
  m4->setParent(m3);

  //-------------------------------------------
  // Create both non-elaborated and elaborated lists
  ModuleCollection* allModules = s->makeCollection<Module>();
  d->setAllModules(allModules);
  allModules->push_back(m1);
  allModules->push_back(m2);

  ModuleCollection* topModules = s->makeCollection<Module>();
  d->setTopModules(topModules);
  topModules->push_back(
      m3);  // Only m3 goes there as it is the top level module

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.push_back(dh);

  return designs;
}

//-------------------------------------------
// Elaboration based on the Listener pattern

class MyElaboratorListener : public VpiListener {
 public:
  MyElaboratorListener() {}

 protected:
  typedef std::map<std::string, const BaseClass*, std::less<>> ComponentMap;

  void leaveDesign(const Design* object, vpiHandle handle) override {
    Design* root = (Design*)object;
    root->setElaborated(true);
  }

  void enterModule(const Module* object, vpiHandle handle) override {
    bool topLevelModule = object->getTopModule();
    const std::string_view instName = object->getName();
    const std::string_view defName = object->getDefName();
    bool flatModule = instName.empty() &&
                      ((object->getParent() == 0) ||
                       ((object->getParent() != 0) &&
                        (object->getParent()->getVpiType() != vpiModule)));
    // false when it is a module in a hierachy tree
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << ", Top:" << topLevelModule
              << std::endl;

    if (flatModule) {
      // Flat list of module (unelaborated)
      flatComponentMap_.insert(
          ComponentMap::value_type(object->getDefName(), object));
    } else {
      // Hierachical module list (elaborated)

      // Collect instance elaborated nets
      ComponentMap netMap;
      if (object->getNets()) {
        for (Net* net : *object->getNets()) {
          netMap.insert(ComponentMap::value_type(net->getName(), net));
        }
      }

      // Push instance context on the stack
      instStack_.push(std::make_pair(object, netMap));

      // Check if Module instance has a definition
      ComponentMap::iterator itrDef = flatComponentMap_.find(defName);
      if (itrDef != flatComponentMap_.end()) {
        const BaseClass* comp = (*itrDef).second;
        int32_t compType = comp->getVpiType();
        switch (compType) {
          case vpiModule: {
            Module* defMod = (Module*)comp;

            // 1) This section illustrates how one can walk the data model in
            // the listener context

            // Bind the cont assign lhs and rhs to elaborated nets
            if (defMod->getContAssigns()) {
              for (ContAssign* assign :
                   *defMod->getContAssigns()) {  // explicit walking
                Net* lnet = nullptr;
                Net* rnet = nullptr;
                const Expr* lhs = assign->getLhs();
                if (lhs->getVpiType() == vpiRefObj) {
                  RefObj* lref = (RefObj*)lhs;
                  lnet = bindNet_(lref->getName());
                }
                const Expr* rhs = assign->getRhs();
                if (rhs->getVpiType() == vpiRefObj) {
                  RefObj* rref = (RefObj*)rhs;
                  rnet = bindNet_(rref->getName());
                }
                // Client code has now access the cont assign and the
                // hierarchical nets
                std::cout << "[2] assign " << lnet->getFullName() << " = "
                          << rnet->getFullName() << "\n";
              }
            }

            // Or

            // 2) This section illustrates how one can use the listener pattern
            // all the way

            // Trigger a listener of the definition module with the instance
            // context on the stack (hirarchical nets) enterCont_assign listener
            // method below will be trigerred to capture the same data as the
            // walking above in (1)
            if (vpiHandle defModule = NewVpiHandle(defMod)) {
              listenModule(defModule);
            }

            break;
          }
          default:
            break;
        }
      }
    }
  }

  void leaveModule(const Module* object, vpiHandle handle) override {
    const std::string_view instName = object->getName();
    bool flatModule = instName.empty() &&
                      ((object->getParent() == 0) ||
                       ((object->getParent() != 0) &&
                        (object->getParent()->getVpiType() != vpiModule)));
    // false when it is a module in a hierachy tree
    if (!flatModule) instStack_.pop();
  }

  // Make full use of the listener pattern for all objects in a module, example
  // with "cont assign":
  void enterContAssign(const ContAssign* assign, vpiHandle handle) override {
    Net* lnet = nullptr;
    Net* rnet = nullptr;
    RefObj* lref = nullptr;
    RefObj* rref = nullptr;
    const Expr* lhs = assign->getLhs();
    if (lhs->getVpiType() == vpiRefObj) {
      lref = (RefObj*)lhs;
    }
    const Expr* rhs = assign->getRhs();
    if (rhs->getVpiType() == vpiRefObj) {
      rref = (RefObj*)rhs;
    }

    if (instStack_.size() == 0) {
      // Flat module traversal
      std::cout << "[1] assign " << lref->getName() << " = " << rref->getName()
                << "\n";

    } else {
      // In the instance context (through the trigered listener)

      lnet = bindNet_(lref->getName());
      rnet = bindNet_(rref->getName());

      // Client code has now access the cont assign and the hierarchical nets
      std::cout << "[3] assign " << lnet->getFullName() << " = "
                << rnet->getFullName() << "\n";
    }
  }

  // Listen to processes, stmts....

 private:
  // Bind to a net in the parent instace
  Net* bindParentNet_(std::string_view name) {
    std::pair<const BaseClass*, ComponentMap> mem = instStack_.top();
    instStack_.pop();
    ComponentMap& netMap = instStack_.top().second;
    instStack_.push(mem);
    ComponentMap::iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      return (Net*)(*netItr).second;
    }
    return nullptr;
  }

  // Bind to a net in the current instace
  Net* bindNet_(std::string_view name) {
    ComponentMap& netMap = instStack_.top().second;
    ComponentMap::iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      return (Net*)(*netItr).second;
    }
    return nullptr;
  }

  // Instance context stack
  std::stack<std::pair<const BaseClass*, ComponentMap>> instStack_;

  // Flat list of components (modules, udps, interfaces)
  ComponentMap flatComponentMap_;
};

// TODO: this is way too coarse.
TEST(ListenerElabTest, RoundTrip) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(&serializer);
  std::string orig;
  orig += "DUMP Design content:\n";
  orig += designs_to_string(designs);
  // std::cout << orig;
  bool elaborated = false;
  for (auto design : designs) {
    elaborated = vpi_get(vpiElaborated, design) || elaborated;
  }
  if (!elaborated) {
    std::cout << "Elaborating...\n";
    MyElaboratorListener* listener = new MyElaboratorListener();
    listener->listenDesigns(designs);
    delete listener;
  }
  std::string post_elab1 = designs_to_string(designs);
  for (auto design : designs) {
    elaborated = vpi_get(vpiElaborated, design) || elaborated;
  }
  EXPECT_TRUE(elaborated);

  // 2nd elab. We expect no change
  {
    MyElaboratorListener* listener = new MyElaboratorListener();
    listener->listenDesigns(designs);
    delete listener;
  }
  std::string post_elab2 = designs_to_string(designs);
  EXPECT_EQ(post_elab1, post_elab2);
}
