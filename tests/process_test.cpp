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
  proc_always->setStmt(begin_block);
  proc_always->setModule(m1);
  ProcessCollection* processes = s->makeCollection<Process>();
  processes->push_back(proc_always);

  // @(posedge clk)
  EventControl* at = s->make<EventControl>();
  RefObj* clk = s->make<RefObj>();
  clk->setName("clk");
  TchkTerm* posedge_clk = s->make<TchkTerm>();
  posedge_clk->setEdge(vpiPosedge);
  posedge_clk->setExpr(clk);
  AnyCollection* simple_exp_vec = s->makeCollection<Any>();
  simple_exp_vec->push_back(posedge_clk);
  clk->setUses(simple_exp_vec);
  at->setCondition(clk);

  // out = 1;
  AnyCollection* statements = s->makeCollection<Any>();
  RefObj* lhs_rf = s->make<RefObj>();
  lhs_rf->setName("out");
  Assignment* assign1 = s->make<Assignment>();
  assign1->setLhs(lhs_rf);
  Constant* c1 = s->make<Constant>();
  s_vpi_value val;
  val.format = vpiIntVal;
  val.value.integer = 1;
  c1->setValue(VpiValue2String(&val));
  assign1->setRhs(c1);
  at->setStmt(assign1);
  statements->push_back(at);

  begin_block->setStmts(statements);
  m1->setProcesses(processes);

  ModuleCollection* v1 = s->makeCollection<Module>();
  v1->push_back(m1);
  d->setAllModules(v1);
  Package* p1 = s->make<Package>();
  p1->setDefName("P0");
  PackageCollection* v3 = s->makeCollection<Package>();
  v3->push_back(p1);
  d->setAllPackages(v3);

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.push_back(dh);

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
