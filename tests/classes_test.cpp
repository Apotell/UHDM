// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
#include <iostream>

#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/ElaboratorListener.h"
#include "uhdm/VpiListener.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

using namespace UHDM;

static std::vector<vpiHandle> build_designs(Serializer* s) {
  std::vector<vpiHandle> designs;

  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design1");

  // Module
  module_inst* m1 = s->MakeModule_inst();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiParent(d);
  m1->VpiFile("fake1.sv");
  m1->VpiLineNo(10);

  /* Base class */
  class_defn* base = s->MakeClass_defn();
  base->VpiName("Base");
  base->VpiParent(m1);

  parameter* param = s->MakeParameter();
  param->VpiName("P1");
  param->VpiParent(base);

  function* f1 = s->MakeFunction();
  f1->VpiName("f1");
  f1->VpiMethod(true);
  f1->VpiParent(base);

  assign_stmt* as = s->MakeAssign_stmt();
  f1->Stmt(as);

  ref_obj* lhs = s->MakeRef_obj();
  lhs->VpiName("a");
  lhs->VpiParent(as);

  ref_obj* rhs = s->MakeRef_obj();
  rhs->VpiName("P1");
  rhs->VpiParent(as);
  as->Lhs(lhs);
  as->Rhs(rhs);

  function* f2 = s->MakeFunction();
  f2->VpiName("f2");
  f2->VpiMethod(true);
  f2->VpiParent(base);

  method_func_call* fcall = s->MakeMethod_func_call();
  f2->Stmt(fcall);
  fcall->VpiName("f1");
  fcall->VpiParent(f2);

  /* Child class */
  class_defn* child = s->MakeClass_defn();
  child->VpiName("Child");
  child->VpiParent(m1);

  UHDM::class_defn* derived = child;
  UHDM::class_defn* parent = base;
  UHDM::extends* extends = s->MakeExtends();
  UHDM::class_typespec* tps = s->MakeClass_typespec();
  UHDM::ref_typespec* rt = s->MakeRef_typespec();
  tps->VpiParent(child);
  rt->Actual_typespec(tps);
  rt->VpiParent(extends);
  extends->VpiParent(child);
  extends->Class_typespec(rt);
  tps->Class_defn(parent);
  derived->Extends(extends);

  UHDM::VectorOfclass_defn* all_derived = s->MakeClass_defnVec();
  parent->Deriveds(all_derived);
  all_derived->push_back(derived);

  function* f3 = s->MakeFunction();
  f3->VpiName("f3");
  f3->VpiMethod(true);
  f3->VpiParent(child);

  method_func_call* fcall2 = s->MakeMethod_func_call();
  f3->Stmt(fcall);
  fcall2->VpiName("f1");  // parent class function
  fcall->VpiParent(f3);

  d->TopModules(true)->emplace_back(m1);

  vpiHandle dh = s->MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);
  return designs;
}

TEST(ClassesTest, DesignSaveRestoreRoundtrip) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(&serializer);
  const std::string before = designs_to_string(designs);

  const std::string filename = testing::TempDir() + "/classes_test.uhdm";
  serializer.Save(filename);

  const std::vector<vpiHandle>& restoredDesigns = serializer.Restore(filename);
  const std::string restored = designs_to_string(restoredDesigns);

  EXPECT_EQ(before, restored);

  // Elaborate restored designs
  ElaboratorContext* elaboratorContext =
      new ElaboratorContext(&serializer, true);
  elaboratorContext->m_elaborator.listenDesigns(restoredDesigns);
  delete elaboratorContext;

  const std::string elaborated = designs_to_string(restoredDesigns);
  EXPECT_NE(restored, elaborated);  // Elaboration should've done _something_
}
