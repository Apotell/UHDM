// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include "uhdm/Cloner.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test_util.h"
//#include "uhdm/UhdmComparer.h"
#include "uhdm/uhdm.h"

using namespace UHDM;
using testing::ElementsAre;

static design* buildModuleProg(Serializer* s) {
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design1");

  VectorOfmodule_inst* const allModules = s->MakeModule_instVec();
  VectorOfpackage* const allPackages = s->MakePackageVec();
  VectorOfprogram* const allPrograms = s->MakeProgramVec();

  d->AllModules(allModules);
  d->AllPackages(allPackages);
  d->AllPrograms(allPrograms);

  // Module
  module_inst* m1 = s->MakeModule_inst();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiFullName("top::M1");
  m1->VpiParent(d);
  allModules->emplace_back(m1);

  // Module
  module_inst* m2 = s->MakeModule_inst();
  m2->VpiDefName("M2");
  m2->VpiName("u1");
  m2->VpiParent(m1);
  allModules->emplace_back(m2);

  // Module
  module_inst* m3 = s->MakeModule_inst();
  m3->VpiDefName("M3");
  m3->VpiName("u2");
  m3->VpiParent(m1);
  allModules->emplace_back(m3);

  // Instance
  module_inst* m4 = s->MakeModule_inst();
  m4->VpiDefName("M4");
  m4->VpiName("u3");
  m3->VpiParent(m3);
  m4->Instance(m3);
  allModules->emplace_back(m4);

  // Module
  module_inst* m5 = s->MakeModule_inst();
  m5->VpiDefName("M5");
  m5->VpiFullName("top::M1");
  m5->VpiParent(d);
  m5->VpiTopModule(true);
  allModules->emplace_back(m5);

  // Package
  package* p1 = s->MakePackage();
  p1->VpiName("P1");
  p1->VpiDefName("P0");
  p1->VpiParent(d);
  allPackages->emplace_back(p1);

  // Instance items, illustrates the use of groups
  program* pr1 = s->MakeProgram();
  pr1->VpiDefName("PR1");
  pr1->VpiParent(d);
  allPrograms->emplace_back(pr1);
  return d;
}

TEST(ClonerTest, Default) {
  Serializer serializer;
  design* const source = buildModuleProg(&serializer);

  const size_t countBeforeClone = serializer.AllObjects().size();

  Cloner cloner(&serializer);
  design* const target = cloner.clone<>(source, nullptr);

  const size_t countAfterCloning = serializer.AllObjects().size();

  EXPECT_GE(countAfterCloning, countBeforeClone * 2);

  // UhdmComparer comparer;
  // EXPECT_EQ(comparer.compare(source, target), 0);
}

TEST(ClonerTest, PassThroughTest) {
  Serializer serializer;
  design* const source = buildModuleProg(&serializer);

  const size_t countBeforeClone = serializer.AllObjects().size();

  PassThroughCloner cloner(&serializer);
  design* const target = cloner.clone<>(source, nullptr);

  const size_t countAfterCloning = serializer.AllObjects().size();

  EXPECT_EQ(countBeforeClone, countAfterCloning);

  // UhdmComparer comparer;
  // EXPECT_EQ(comparer.compare(source, target), 0);
}

TEST(ClonerTest, Mirrored) {
  Serializer serializer;
  design* const source = buildModuleProg(&serializer);

  const size_t countBeforeClone = serializer.AllObjects().size();

  MirrorCloner cloner(&serializer);
  design* const target = cloner.clone<>(source, nullptr);

  const size_t countAfterCloning = serializer.AllObjects().size();

  EXPECT_EQ(countAfterCloning, countBeforeClone * 2);

  // UhdmComparer comparer;
  // EXPECT_EQ(comparer.compare(source, target), 0);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
