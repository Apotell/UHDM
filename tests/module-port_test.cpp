// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <iostream>

#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

using namespace uhdm;

// TODO: These tests are 'too big', i.e. they don't test a particular aspect
// of serialization.

class MyPayLoad : public ClientData {
 public:
  MyPayLoad(int32_t f) { foo_ = f; }

 private:
  int32_t foo_;
};

static std::vector<vpiHandle> buildModulePortDesign(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("design1");
  // Module
  Module* m1 = s->make<Module>();
  m1->setTopModule(true);
  m1->setDefName("M1");
  m1->setFullName("top::M1");
  m1->setParent(d);
  m1->setFile("fake1.sv");
  m1->setStartLine(10);

  auto vars = s->makeCollection<Variables>();
  m1->setVariables(vars);
  LogicVar* lvar = s->make<LogicVar>();
  vars->push_back(lvar);
  lvar->setFullName("top::M1::v1");

  // Module
  Module* m2 = s->make<Module>();
  m2->setDefName("M2");
  m2->setName("u1");
  m2->setParent(m1);
  m2->setFile("fake2.sv");
  m2->setStartLine(20);

  // Ports
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

  // Module
  Module* m3 = s->make<Module>();
  m3->setDefName("M3");
  m3->setName("u2");
  m3->setParent(m1);
  m3->setFile("fake3.sv");
  m3->setStartLine(30);

  // Instance
  Module* m4 = s->make<Module>();
  m4->setDefName("M4");
  m4->setName("u3");
  m4->setPorts(vp);
  m4->setParent(m3);
  m4->setInstance(m3);
  ModuleCollection* v1 = s->makeCollection<Module>();
  v1->push_back(m1);
  d->setAllModules(v1);
  ModuleCollection* v2 = s->makeCollection<Module>();
  v2->push_back(m2);
  v2->push_back(m3);
  m1->setModules(v2);

  // Package
  Package* p1 = s->make<Package>();
  p1->setName("P1");
  p1->setDefName("P0");
  PackageCollection* v3 = s->makeCollection<Package>();
  v3->push_back(p1);
  d->setAllPackages(v3);
  // Function
  Function* f1 = s->make<Function>();
  f1->setName("MyFunc1");
  f1->setSize(100);
  Function* f2 = s->make<Function>();
  f2->setName("MyFunc2");
  f2->setSize(200);
  TaskFuncCollection* v4 = s->makeCollection<TaskFunc>();
  v4->push_back(f1);
  v4->push_back(f2);
  p1->setTaskFuncs(v4);

  // Instance items, illustrates the use of groups
  Program* pr1 = s->make<Program>();
  pr1->setDefName("PR1");
  pr1->setParent(m1);
  AnyCollection* inst_items = s->makeCollection<Any>();
  inst_items->push_back(pr1);
  Function* f3 = s->make<Function>();
  f3->setName("MyFunc3");
  f3->setSize(300);
  f3->setParent(m1);
  inst_items->push_back(f3);
  m1->setInstanceItems(inst_items);
  MyPayLoad* pl = new MyPayLoad(10);
  m1->setClientData(pl);

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.push_back(dh);

  {
    char name[]{"P1"};
    vpiHandle obj_h = vpi_handle_by_name(name, dh);
    EXPECT_NE(obj_h, nullptr);

    char name1[]{"MyFunc1"};
    vpiHandle obj_h1 = vpi_handle_by_name(name1, obj_h);
    EXPECT_NE(obj_h1, nullptr);
  }

  return designs;
}

TEST(Serialization, SerializeModulePortDesign_e2e) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = buildModulePortDesign(&serializer);

  const std::string orig = designs_to_string(designs);

  const std::string filename =
      testing::TempDir() + "/serialize-module-port-roundrip.uhdm";
  serializer.save(filename);

  const std::vector<vpiHandle>& restoredDesigns = serializer.restore(filename);
  const std::string restored = designs_to_string(restoredDesigns);
  EXPECT_EQ(orig, restored);
}
