#include <uhdm/uhdm.h>
#include <uhdm/vpi_visitor.h>

#include <iostream>

#include "test-util.h"

using namespace UHDM;

#include <uhdm/vpi_visitor.h>

// This builds a simple design:
// module m1;
//   always @(posedge clk)
//     out = 1;
//   end
// endmodule
std::vector<vpiHandle> build_designs(Serializer& s) {
  std::vector<vpiHandle> designs;
  // Design building
  design* d = s.MakeDesign();
  d->VpiName("design_process");
  module* m1 = s.MakeModule();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiName("u1");
  m1->VpiParent(d);
  m1->VpiFile("fake1.sv");
  m1->VpiLineNo(10);

  // always @(posedge clk) begin
  always* proc_always = s.MakeAlways();
  begin* begin_block = s.MakeBegin();
  proc_always->Stmt(begin_block);
  proc_always->Module(m1);
  VectorOfprocess_stmt* processes = s.MakeProcess_stmtVec();
  processes->push_back(proc_always);

  // @(posedge clk)
  event_control* at = s.MakeEvent_control();
  ref_obj* clk = s.MakeRef_obj();
  clk->VpiName("clk");
  tchk_term* posedge_clk = s.MakeTchk_term();
  posedge_clk->VpiEdge(vpiPosedge);
  posedge_clk->Expr(clk);
  VectorOfany* simple_exp_vec = s.MakeAnyVec();
  simple_exp_vec->push_back(posedge_clk);
  clk->VpiUses(simple_exp_vec);
  at->VpiCondition(clk);

  // out = 1;
  VectorOfany* statements = s.MakeAnyVec();
  ref_obj* lhs_rf = s.MakeRef_obj();
  lhs_rf->VpiName("out");
  assignment* assign1 = s.MakeAssignment();
  assign1->Lhs(lhs_rf);
  constant* c1 = s.MakeConstant();
  s_vpi_value val;
  val.format = vpiIntVal;
  val.value.integer = 1;
  c1->VpiValue(VpiValue2String(&val));
  assign1->Rhs(c1);
  at->Stmt(assign1);
  statements->push_back(at);

  begin_block->Stmts(statements);
  m1->Process(processes);

  VectorOfmodule* v1 = s.MakeModuleVec();
  v1->push_back(m1);
  d->AllModules(v1);
  package* p1 = s.MakePackage();
  p1->VpiDefName("P0");
  VectorOfpackage* v3 = s.MakePackageVec();
  v3->push_back(p1);
  d->AllPackages(v3);

  vpiHandle dh = s.MakeUhdmHandle(uhdmdesign, d);
  designs.push_back(dh);

  char name[]{"u1"};
  vpiHandle obj_h = vpi_handle_by_name(name, dh);
  if (obj_h == 0) {
    exit(1);
  }
  return designs;
}

int main(int argc, char** argv) {
  std::cout << "Make design" << std::endl;
  Serializer serializer;

  std::string orig = visit_designs(build_designs(serializer));

  std::cout << orig;
  const std::string filename = uhdm_test::getTmpDir() + "/surelog_process.uhdm";
  std::cout << "\nSave design" << std::endl;
  serializer.Save(filename);

  std::cout << "Restore design" << std::endl;
  std::vector<vpiHandle> restoredDesigns = serializer.Restore(filename);

  std::string restored = visit_designs(restoredDesigns);
  std::cout << restored;
  return (orig != restored);
}
