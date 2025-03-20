#include <iostream>
#include <stack>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/ElaboratorListener.h"
#include "uhdm/ExprEval.h"
#include "uhdm/VpiListener.h"
#include "uhdm/uhdm.h"
#include "uhdm/vpi_visitor.h"

using namespace uhdm;

std::vector<vpiHandle> build_designs_MinusOp(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  Module* dut = s->make<Module>();
  {
    dut->setDefName("M1");
    dut->setParent(d);
    // dut->VpiFile("fake1.sv");
    // dut->VpiLineNo(10);
    PortCollection* vp = s->makeCollection<Port>();
    dut->setPorts(vp);
    Port* p = s->make<Port>();
    vp->push_back(p);
    p->setName("wire_i");
    p->setDirection(vpiInput);

    TypespecCollection* typespecs = s->makeCollection<Typespec>();
    dut->setTypespecs(typespecs);

    LogicTypespec* tps = s->make<LogicTypespec>();
    typespecs->emplace_back(tps);

    RefTypespec* tps_rt = s->make<RefTypespec>();
    tps_rt->setActualTypespec(tps);
    tps_rt->setParent(p);
    p->setTypespec(tps_rt);

    RangeCollection* ranges = s->makeCollection<Range>();
    tps->setRanges(ranges);
    Range* range = s->make<Range>();
    ranges->push_back(range);

    Operation* oper = s->make<Operation>();
    range->setLeftExpr(oper);
    oper->setOpType(vpiSubOp);
    AnyCollection* operands = s->makeCollection<Any>();
    oper->setOperands(operands);

    RefObj* SIZE = s->make<RefObj>();
    operands->push_back(SIZE);
    SIZE->setName("SIZE");
    LogicNet* n = s->make<LogicNet>();
    SIZE->setActual(n);

    Constant* c1 = s->make<Constant>();
    c1->setValue("UINT:1");
    c1->setConstType(vpiIntConst);
    c1->setDecompile("1");
    operands->push_back(c1);

    Constant* c2 = s->make<Constant>();
    c2->setValue("UINT:0");
    c2->setConstType(vpiIntConst);
    c2->setDecompile("0");

    range->setRightExpr(c2);
  }

  ModuleCollection* topModules = s->makeCollection<Module>();
  d->setTopModules(topModules);
  topModules->push_back(dut);

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.push_back(dh);

  return designs;
}

TEST(exprVal, prettyPrint_MinusOp) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs_MinusOp(&serializer);
  // serializer.Save("expr_MinusOp_test.uhdm");
  // const std::string before = designs_to_string(designs);
  // std::cout << before <<std::endl;

  bool elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_FALSE(elaborated);

  ElaboratorContext* elaboratorContext =
      new ElaboratorContext(&serializer, true);
  elaboratorContext->m_elaborator.listenDesigns(designs);
  delete elaboratorContext;

  elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_TRUE(elaborated);

  vpiHandle dh = designs.at(0);
  Design* d = UhdmDesignFromVpiHandle(dh);

  ExprEval eval;
  for (auto m : *d->getTopModules()) {
    for (auto p : *m->getPorts()) {
      const RefTypespec* rt = p->getTypespec();
      const LogicTypespec* typespec = rt->getActualTypespec<LogicTypespec>();
      RangeCollection* ranges = typespec->getRanges();
      for (auto range : *ranges) {
        Expr* left = (Expr*)range->getLeftExpr();
        std::string left_str = eval.prettyPrint((Any*)left);
        EXPECT_EQ(left_str, "SIZE - 1");

        Expr* right = (Expr*)range->getRightExpr();
        std::string right_str = eval.prettyPrint((Any*)right);
        EXPECT_EQ(right_str, "0");
      }
    }
  }
}

std::vector<vpiHandle> build_designs_ConditionOp(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  Module* dut = s->make<Module>();
  {
    dut->setDefName("M1");
    dut->setParent(d);
    dut->setParameters(s->makeCollection<Any>());

    ParamAssignCollection* vpa = s->makeCollection<ParamAssign>();

    ParamAssign* param = s->make<ParamAssign>();
    vpa->push_back(param);
    dut->setParamAssigns(vpa);

    Parameter* p = s->make<Parameter>();
    dut->getParameters()->push_back(p);
    p->setName("a");
    param->setLhs(p);

    Operation* oper = s->make<Operation>();
    oper->setOpType(vpiConditionOp);
    AnyCollection* operands = s->makeCollection<Any>();
    oper->setOperands(operands);
    RefObj* b = s->make<RefObj>();
    b->setName("b");
    operands->push_back(b);

    Constant* c1 = s->make<Constant>();
    c1->setValue("UINT:1");
    c1->setConstType(vpiIntConst);
    c1->setDecompile("1");
    operands->push_back(c1);

    Constant* c2 = s->make<Constant>();
    c2->setValue("UINT:3");
    c2->setConstType(vpiIntConst);
    c2->setDecompile("3");
    operands->push_back(c2);
    param->setRhs(oper);
  }

  ModuleCollection* topModules = s->makeCollection<Module>();
  d->setTopModules(topModules);
  topModules->push_back(dut);

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.push_back(dh);

  return designs;
}

TEST(exprVal, prettyPrint_ConditionOp) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs =
      build_designs_ConditionOp(&serializer);
  // serializer.Save("expr_ConditionOp_test.uhdm");
  // const std::string before = designs_to_string(designs);
  // std::cout << before <<std::endl;

  bool elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_FALSE(elaborated);

  ElaboratorContext* elaboratorContext =
      new ElaboratorContext(&serializer, true);
  elaboratorContext->m_elaborator.listenDesigns(designs);
  delete elaboratorContext;

  elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_TRUE(elaborated);

  vpiHandle dh = designs.at(0);
  Design* d = UhdmDesignFromVpiHandle(dh);

  ExprEval eval;
  for (auto m : *d->getTopModules()) {
    for (auto pa : *m->getParamAssigns()) {
      const Any* rhs = pa->getRhs();
      std::string result = eval.prettyPrint((Any*)rhs);
      EXPECT_EQ(result, "b ? 1 : 3");
    }
  }
}

std::vector<vpiHandle> build_designs_functionCall(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  Module* dut = s->make<Module>();
  {
    dut->setDefName("M1");
    dut->setParent(d);
    dut->setParameters(s->makeCollection<Any>());

    ParamAssignCollection* vpa = s->makeCollection<ParamAssign>();

    ParamAssign* param = s->make<ParamAssign>();
    vpa->push_back(param);
    dut->setParamAssigns(vpa);

    Parameter* p = s->make<Parameter>();
    dut->getParameters()->push_back(p);
    p->setName("a");
    param->setLhs(p);

    SysFuncCall* sfc = s->make<SysFuncCall>();
    param->setRhs(sfc);

    sfc->setName("$sformatf");

    AnyCollection* args = s->makeCollection<Any>();
    sfc->setArguments(args);
    Constant* c1 = s->make<Constant>();
    c1->setValue("%d");
    c1->setConstType(vpiStringConst);
    c1->setDecompile("\"%d\"");
    args->push_back(c1);

    RefObj* b = s->make<RefObj>();
    b->setName("b");
    args->push_back(b);
    LogicNet* n = s->make<LogicNet>();
    b->setActual(n);
  }

  ModuleCollection* topModules = s->makeCollection<Module>();
  d->setTopModules(topModules);
  topModules->push_back(dut);

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.push_back(dh);

  return designs;
}

TEST(exprVal, prettyPrint_functionCall) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs =
      build_designs_functionCall(&serializer);
  // serializer.Save("expr_functionCall_test.uhdm");
  // const std::string before = designs_to_string(designs);
  // std::cout << before <<std::endl;

  bool elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_FALSE(elaborated);

  ElaboratorContext* elaboratorContext =
      new ElaboratorContext(&serializer, true);
  elaboratorContext->m_elaborator.listenDesigns(designs);
  delete elaboratorContext;

  elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_TRUE(elaborated);

  vpiHandle dh = designs.at(0);
  Design* d = UhdmDesignFromVpiHandle(dh);

  ExprEval eval;
  for (auto m : *d->getTopModules()) {
    for (auto pa : *m->getParamAssigns()) {
      const Any* rhs = pa->getRhs();
      std::string result = eval.prettyPrint((Any*)rhs);
      std::string expected_result = "$sformatf(\"%d\",b)";
      std::cout << expected_result << std::endl;

      EXPECT_EQ(expected_result, result);
    }
  }
}

std::vector<vpiHandle> build_designs_select(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  Module* dut = s->make<Module>();
  {
    dut->setDefName("M1");
    dut->setParent(d);
    dut->setParameters(s->makeCollection<Any>());

    ParamAssignCollection* vpa = s->makeCollection<ParamAssign>();

    ParamAssign* param = s->make<ParamAssign>();
    vpa->push_back(param);
    dut->setParamAssigns(vpa);

    Parameter* p = s->make<Parameter>();
    dut->getParameters()->push_back(p);
    p->setName("a");
    param->setLhs(p);

    VarSelect* vs = s->make<VarSelect>();
    param->setRhs(vs);

    vs->setName("b");
    ExprCollection* exprs = s->makeCollection<Expr>();
    vs->setIndexes(exprs);

    Constant* c1 = s->make<Constant>();
    c1->setValue("UINT:3");
    c1->setConstType(vpiIntConst);
    c1->setDecompile("3");
    exprs->push_back(c1);

    Constant* c2 = s->make<Constant>();
    c2->setValue("UINT:2");
    c2->setConstType(vpiIntConst);
    c2->setDecompile("2");
    exprs->push_back(c2);

    PartSelect* ps = s->make<PartSelect>();
    exprs->push_back(ps);
    ps->setConstantSelect(true);

    Constant* c3 = s->make<Constant>();
    c3->setValue("UINT:1");
    c3->setConstType(vpiIntConst);
    c3->setDecompile("1");
    ps->setLeftExpr(c3);

    Constant* c4 = s->make<Constant>();
    c4->setValue("UINT:0");
    c4->setConstType(vpiIntConst);
    c4->setDecompile("0");
    ps->setRightExpr(c4);
  }

  ModuleCollection* topModules = s->makeCollection<Module>();
  d->setTopModules(topModules);
  topModules->push_back(dut);

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.push_back(dh);

  return designs;
}

TEST(exprVal, prettyPrint_select) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs = build_designs_select(&serializer);
  // serializer.Save("expr_select_test.uhdm");
  // const std::string before = designs_to_string(designs);
  // std::cout << before <<std::endl;

  bool elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_FALSE(elaborated);

  ElaboratorContext* elaboratorContext =
      new ElaboratorContext(&serializer, true);
  elaboratorContext->m_elaborator.listenDesigns(designs);
  delete elaboratorContext;

  elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_TRUE(elaborated);

  vpiHandle dh = designs.at(0);
  Design* d = UhdmDesignFromVpiHandle(dh);

  ExprEval eval;
  for (auto m : *d->getTopModules()) {
    for (auto pa : *m->getParamAssigns()) {
      const Any* rhs = pa->getRhs();
      std::string result = eval.prettyPrint((Any*)rhs);
      std::string expected_result = "b[3][2][1:0]";
      std::cout << expected_result << std::endl;

      EXPECT_EQ(expected_result, result);
    }
  }
}

std::vector<vpiHandle> build_designs_AssignmentPatternOp(Serializer* s) {
  std::vector<vpiHandle> designs;
  // Design building
  Design* d = s->make<Design>();
  d->setName("design1");

  //-------------------------------------------
  // Module definition M1 (non elaborated)
  Module* dut = s->make<Module>();
  {
    dut->setDefName("M1");
    dut->setParent(d);
    dut->setParameters(s->makeCollection<Any>());

    ParamAssignCollection* vpa = s->makeCollection<ParamAssign>();

    ParamAssign* param = s->make<ParamAssign>();
    vpa->push_back(param);
    dut->setParamAssigns(vpa);

    Parameter* p = s->make<Parameter>();
    dut->getParameters()->push_back(p);
    p->setName("a");
    param->setLhs(p);

    Operation* op = s->make<Operation>();
    param->setRhs(op);

    op->setOpType(vpiAssignmentPatternOp);
    AnyCollection* operands = s->makeCollection<Any>();
    op->setOperands(operands);

    Constant* c1 = s->make<Constant>();
    c1->setValue("UINT:1");
    c1->setConstType(vpiIntConst);
    c1->setDecompile("1");
    operands->push_back(c1);

    Constant* c2 = s->make<Constant>();
    c2->setValue("UINT:2");
    c2->setConstType(vpiIntConst);
    c2->setDecompile("2");
    operands->push_back(c2);

    Constant* c3 = s->make<Constant>();
    c3->setValue("UINT:3");
    c3->setConstType(vpiIntConst);
    c3->setDecompile("3");
    operands->push_back(c3);
  }

  ModuleCollection* topModules = s->makeCollection<Module>();
  d->setTopModules(topModules);
  topModules->push_back(dut);

  vpiHandle dh = s->makeUhdmHandle(UhdmType::Design, d);
  designs.push_back(dh);

  return designs;
}

TEST(exprVal, prettyPrint_array) {
  Serializer serializer;
  const std::vector<vpiHandle>& designs =
      build_designs_AssignmentPatternOp(&serializer);
  // serializer.Save("expr_AssignmentPatternOp_test.uhdm");
  // const std::string before = designs_to_string(designs);
  // std::cout << before <<std::endl;

  bool elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_FALSE(elaborated);

  ElaboratorContext* elaboratorContext =
      new ElaboratorContext(&serializer, true);
  elaboratorContext->m_elaborator.listenDesigns(designs);
  delete elaboratorContext;

  elaborated = false;
  for (auto Design : designs) {
    elaborated = vpi_get(vpiElaborated, Design) || elaborated;
  }
  EXPECT_TRUE(elaborated);

  vpiHandle dh = designs.at(0);
  Design* d = UhdmDesignFromVpiHandle(dh);

  ExprEval eval;
  for (auto m : *d->getTopModules()) {
    for (auto pa : *m->getParamAssigns()) {
      const Any* rhs = pa->getRhs();
      std::string result = eval.prettyPrint((Any*)rhs);
      std::string expected_result = "'{1,2,3}";
      std::cout << expected_result << std::endl;

      EXPECT_EQ(expected_result, result);
    }
  }
}
