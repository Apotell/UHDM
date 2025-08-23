#include <iostream>

#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

using namespace uhdm;

// This builds a simple design:
// module m1;
//   always @(posedge clk)
//     out = 1;
//   end
// endmodule
static std::vector<vpiHandle> buildSimpleAlawysDesign(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("design_process");

  Module* m1 = s->make<Module>();
  m1->setTopModule(true);
  m1->setDefName("M1");
  m1->setName("u1");
  m1->setParent(d);
  m1->setFile("fake1.sv");
  m1->setStartLine(10);

  // always @(posedge clk) begin
  Always* proc_always = s->make<Always>();
  Begin* begin_block = s->make<Begin>();
  begin_block->setParent(m1);
  proc_always->setStmt(begin_block);
  proc_always->setModule(m1);
  proc_always->setParent(m1);

  // @(posedge clk)
  EventControl* at = s->make<EventControl>();
  RefObj* clk = s->make<RefObj>();
  clk->setName("clk");
  clk->setParent(at);

  TchkTerm* posedge_clk = s->make<TchkTerm>();
  posedge_clk->setEdge(vpiPosedge);
  posedge_clk->setExpr(clk);
  posedge_clk->setParent(clk);

  AnyCollection* simple_exp_vec = clk->getUses(true);
  simple_exp_vec->emplace_back(posedge_clk);
  clk->setUses(simple_exp_vec);
  at->setCondition(clk);
  at->setParent(begin_block);

  // out = 1;
  AnyCollection* statements = begin_block->getStmts(true);
  RefObj* lhs_rf = s->make<RefObj>();
  lhs_rf->setName("out");

  Assignment* assign1 = s->make<Assignment>();
  assign1->setLhs(lhs_rf);
  lhs_rf->setParent(assign1);

  Constant* c1 = s->make<Constant>();
  s_vpi_value val;
  val.format = vpiIntVal;
  val.value.integer = 1;
  c1->setValue(VpiValue2String(&val));
  assign1->setRhs(c1);
  c1->setParent(assign1);
  at->setStmt(assign1);
  assign1->setParent(at);
  statements->emplace_back(at);

  Package* p1 = s->make<Package>();
  p1->setDefName("P0");
  p1->setParent(d);

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.emplace_back(dh);

  char name[]{"u1"};
  vpiHandle obj_h = vpi_handle_by_name(name, dh);
  EXPECT_NE(obj_h, nullptr);

  return designs;
}

TEST(SerializationProcess, ProcessSerialization) {
  Serializer serializer;
  const std::string orig =
      designs_to_string(buildSimpleAlawysDesign(&serializer));

  const std::string filename = testing::TempDir() + "/surelog_process.uhdm";
  serializer.save(filename);

  const std::vector<vpiHandle> restoredDesigns = serializer.restore(filename);
  const std::string restored = designs_to_string(restoredDesigns);
  EXPECT_EQ(orig, restored);
}

// TODO: other, object level tests ?
