// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <iostream>

#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

using namespace uhdm;

static std::vector<vpiHandle> build_designs(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("design1");

  // Module
  Module* m1 = s->make<Module>();
  m1->setTopModule(true);
  m1->setDefName("M1");
  m1->setParent(d);

  // Module
  Module* m2 = s->make<Module>();
  m2->setDefName("M2");
  m2->setName("u1");

  ModuleCollection* v1 = s->makeCollection<Module>();
  v1->push_back(m1);
  d->setTopModules(v1);

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.push_back(dh);

  return designs;
}

TEST(GarbageCollectTest, NoLeakExpectation) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs(&serializer);
  const std::string before = designs_to_string(designs);

  const std::string filename = testing::TempDir() + "/gc_test.uhdm";
  serializer.save(filename);
  for (vpiHandle design : designs) {
    vpi_release_handle(design);
  }

  const std::vector<vpiHandle>& restoredDesigns = serializer.restore(filename);
  const std::string restored = designs_to_string(restoredDesigns);

  std::string decompiled;
  for (auto& objIndexPair : serializer.getAllObjects()) {
    if (objIndexPair.first) {
      decompiled += "OBJECT:\n";
      decompiled += decompile((Any*)objIndexPair.first);
    }
  }
  // TODO: decompiled needs to be compared with something, otherwise this
  // test is not useful. Nobody looks a the logs.
  std::cerr << decompiled << "\n";

  for (vpiHandle design : restoredDesigns) {
    vpi_release_handle(design);
  }
}
