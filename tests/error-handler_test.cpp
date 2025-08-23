#include <iostream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

using namespace uhdm;
using testing::HasSubstr;

static std::vector<vpiHandle> build_designs(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("design3");

  Module* m1 = s->make<Module>();
  m1->setTopModule(true);
  m1->setDefName("M1");
  m1->setParent(d);
  m1->setFile("fake1.sv");
  m1->setStartLine(10);

  Module* m2 = s->make<Module>();
  m2->setDefName("M2");
  m2->setName("u1");
  m2->setFullName("M1.u1");
  m2->setParent(m1);
  m2->setInstance(m1);
  m2->setModule(m1);
  m2->setFile("fake2.sv");
  m2->setStartLine(20);

  Initial* init = s->make<Initial>();
  init->setParent(m2);

  Begin* begin_block = s->make<Begin>();
  init->setStmt(begin_block);
  begin_block->setParent(init);
  AnyCollection* statements = begin_block->getStmts(true);

  RefObj* lhs_rf = s->make<RefObj>();
  lhs_rf->setName("out");

  Assignment* assign1 = s->make<Assignment>();
  assign1->setLhs(lhs_rf);
  assign1->setParent(begin_block);
  lhs_rf->setParent(assign1);

  Constant* c1 = s->make<Constant>();
  c1->setValue("INT:0");
  c1->setParent(assign1);
  assign1->setRhs(m1);  // Triggers error handler!
  statements->emplace_back(assign1);

  Assignment* assign2 = s->make<Assignment>();
  assign2->setLhs(lhs_rf);
  assign2->setParent(begin_block);

  Constant* c2 = s->make<Constant>();
  c2->setValue("STRING:a string");
  c2->setParent(assign2);
  assign2->setRhs(c2);
  statements->emplace_back(assign2);

  DelayControl* dc = s->make<DelayControl>();
  dc->setVpiDelay("#100");

  Assignment* assign3 = s->make<Assignment>();
  assign3->setLhs(lhs_rf);
  assign3->setParent(dc);

  Constant* c3 = s->make<Constant>();
  s_vpi_value val;
  val.format = vpiIntVal;
  val.value.integer = 1;
  c3->setValue(VpiValue2String(&val));
  c3->setParent(assign3);
  assign3->setRhs(c3);
  dc->setStmt(assign3);
  dc->setParent(begin_block);
  statements->emplace_back(dc);

  Module* m3 = s->make<Module>();
  m3->setDefName("M3");
  m3->setName("u2");
  m3->setFullName("M1.u2");
  m3->setParent(m1);
  m3->setInstance(m1);
  m3->setModule(m1);
  m3->setFile("fake3.sv");
  m3->setStartLine(30);

  Package* p1 = s->make<Package>();
  p1->setDefName("P0");
  p1->setParent(d);
  designs.emplace_back(s->makeUhdmHandle(UhdmType::Design, d));

  return designs;
}

TEST(ErrorHandlerTest, ErrorHandlerIsCalled) {
  Serializer serializer;

  // Install a customer error handler
  bool issuedError = false;
  std::string last_msg;
  ErrorHandler MyErrorHandler = [&](ErrorType errType, const std::string& msg,
                                    const Any* object1, const Any* object2) {
    last_msg = msg;
    issuedError = true;
  };
  serializer.setErrorHandler(MyErrorHandler);

  issuedError = false;
  const std::string before = designs_to_string(build_designs(&serializer));

  EXPECT_TRUE(issuedError);  // Issue in design.
  EXPECT_THAT(last_msg, HasSubstr("adding wrong object type"));

  issuedError = false;
  const std::string filename = testing::TempDir() + "/error-handler_test.uhdm";
  serializer.save(filename);

  EXPECT_FALSE(issuedError);

  // Save/restore will not contain the errornous part.
  issuedError = false;
  std::vector<vpiHandle> restoredDesigns = serializer.restore(filename);
  const std::string restored = designs_to_string(restoredDesigns);
  EXPECT_FALSE(issuedError);

  EXPECT_EQ(before, restored);
}
