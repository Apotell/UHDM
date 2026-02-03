#include "gtest/gtest.h"
#include "uhdm/ExprEval.h"
#include "uhdm/NumUtils.h"
#include "uhdm/Utils.h"
#include "uhdm/uhdm.h"

using namespace uhdm;

using Constants = std::map<std::string_view, Constant*>;

class TestObjectProvider : public ObjectProvider {
 public:
  Serializer m_serializer;
  Constants m_constants;
  std::unordered_map<std::string_view, Any*> m_objects;

  const Any* getObject(std::string_view name, const Any*, const Any*, bool = false) final {
    auto it = m_objects.find(name);
    return (it == m_objects.end()) ? nullptr : it->second;
  }

  Any* getValue(std::string_view name, const Any*, const Any*, bool = false) final {
    auto it = m_constants.find(name);
    return (it == m_constants.end()) ? nullptr : it->second;
  }
  const TaskFunc* getTaskFunc(std::string_view, const Any*, const Any*, bool = false) final { return nullptr; }
};

static void setTypespec(Constant* constant, UhdmType uhdmType, bool sign, std::string_view svalue,
                        Serializer& m_serializer) {
  Typespec* t = nullptr;
  switch (uhdmType) {
    case UhdmType::ByteTypespec: t = m_serializer.make<ByteTypespec>(); break;
    case UhdmType::ShortIntTypespec: t = m_serializer.make<ShortIntTypespec>(); break;
    case UhdmType::IntTypespec: t = m_serializer.make<IntTypespec>(); break;
    case UhdmType::LongIntTypespec: t = m_serializer.make<LongIntTypespec>(); break;
    case UhdmType::ShortRealTypespec: t = m_serializer.make<ShortRealTypespec>(); break;
    case UhdmType::RealTypespec: t = m_serializer.make<RealTypespec>(); break;
    case UhdmType::IntegerTypespec: t = m_serializer.make<IntegerTypespec>(); break;
    case UhdmType::LogicTypespec: t = m_serializer.make<LogicTypespec>(); break;
    case UhdmType::TimeTypespec: t = m_serializer.make<TimeTypespec>(); break;
    default: break;
  }
  uhdm::setTypespec(constant, t);
  setSigned(t, sign);
  constant->setDecompile(svalue);
  constant->setValue(svalue);
}

TEST(ExprEvalRefObj, ReduceRefObjResolvesConstant) {
  TestObjectProvider provider;
  Serializer& m_serializer = provider.m_serializer;
  Constants& m_constants = provider.m_constants;
  ExprEval m_evaluator(&provider);

  RefObj* const ro = m_serializer.make<RefObj>();
  ro->setName("a");

  Constant* const c = m_serializer.make<Constant>();
  c->setValue("42");
  c->setDecompile("42");
  c->setConstType(vpiDecConst);
  c->setSize(32);
  ro->setActual(c);

  setTypespec(c, UhdmType::IntTypespec, true, "42", m_serializer);

  m_constants.emplace("a", c);

  Expr* result = nullptr;
  const bool succeeded = m_evaluator.reduceExpr(ro, ro, &result, true);

  ASSERT_TRUE(succeeded);
  ASSERT_NE(result, nullptr);

  Constant* const rc = any_cast<Constant>(result);
  ASSERT_NE(rc, nullptr);
  ASSERT_EQ(rc->getDecompile(), "42");

  Typespec* const t = getTypespec(result);
  ASSERT_NE(t, nullptr);
  ASSERT_EQ(t->getUhdmType(), UhdmType::IntTypespec);
  ASSERT_TRUE(getSigned(t));
}

TEST(ExprEvalReduceExpr, HierPathSimpleConstant) {
  TestObjectProvider provider;
  Serializer& s = provider.m_serializer;
  Constants& m_constants = provider.m_constants;
  ExprEval eval(&provider);

  // ---- constant a = 10 ----
  Constant* const c = s.make<Constant>();
  c->setValue("10");
  c->setDecompile("10");
  c->setConstType(vpiDecConst);
  c->setSize(32);
  setTypespec(c, UhdmType::IntTypespec, true, "10", s);

  m_constants.emplace("a", c);

  HierPath* const hp = s.make<HierPath>();

  RefObj* ref = s.make<RefObj>();
  ref->setName("a");
  ref->setParent(hp);
  ref->setActual(c);

  AnyCollection* const elems = hp->getPathElems(true);
  elems->push_back(ref);

  Expr* result = nullptr;
  bool ok = eval.reduceExpr(hp, hp, &result, true);

  ASSERT_TRUE(ok);
  ASSERT_NE(result, nullptr);

  Constant* const rc = any_cast<Constant>(result);
  ASSERT_NE(rc, nullptr);
  ASSERT_EQ(rc->getDecompile(), "10");

  Typespec* const t = getTypespec(rc);
  ASSERT_NE(t, nullptr);
  ASSERT_EQ(t->getUhdmType(), UhdmType::IntTypespec);
  ASSERT_TRUE(getSigned(t));
}

TEST(ExprEvalReduceExpr, PartSelect_ConstantVector) {
  TestObjectProvider provider;
  Serializer& s = provider.m_serializer;
  Constants& m_constants = provider.m_constants;
  ExprEval eval(&provider);

  auto makeInt = [&](const char* v) {
    Constant* c = s.make<Constant>();
    c->setValue(v);
    c->setDecompile(v);
    c->setConstType(vpiDecConst);
    c->setSize(32);
    setTypespec(c, UhdmType::IntTypespec, true, v, s);
    return c;
  };

  LogicTypespec* const lts = s.make<LogicTypespec>();
  lts->setSigned(false);

  Range* const pr = s.make<Range>();
  pr->setLeftExpr(makeInt("7"));
  pr->setRightExpr(makeInt("0"));

  RangeCollection* const prc = s.makeCollection<Range>();
  prc->push_back(pr);
  lts->setRanges(prc);

  Constant* const vec = s.make<Constant>();
  vec->setValue("10110011");
  vec->setDecompile("10110011");
  vec->setConstType(vpiBinaryConst);
  vec->setSize(8);
  uhdm::setTypespec(vec, lts);

  m_constants.emplace("vec", vec);

  Constant* left = makeInt("5");
  Constant* right = makeInt("2");

  PartSelect* const ps = s.make<PartSelect>();
  ps->setName("vec");
  ps->setLeftExpr(left);
  ps->setRightExpr(right);

  Expr* result = nullptr;
  bool ok = eval.reduceExpr(ps, ps, &result, true);

  ASSERT_TRUE(ok);
  ASSERT_NE(result, nullptr);

  Constant* const rc = any_cast<Constant>(result);
  ASSERT_NE(rc, nullptr);

  ASSERT_EQ(rc->getValue(), "1100");
  ASSERT_EQ(rc->getConstType(), vpiBinaryConst);
  ASSERT_EQ(rc->getSize(), 4);
  ASSERT_EQ(rc->getDecompile(), "1100");
}

TEST(ExprEvalReduceExpr, IndexedPartSelect_ConstantVector) {
  TestObjectProvider provider;
  Serializer& s = provider.m_serializer;
  Constants& m_constants = provider.m_constants;
  ExprEval eval(&provider);

  auto makeInt = [&](const char* v) {
    Constant* c = s.make<Constant>();
    c->setValue(v);
    c->setDecompile(v);
    c->setConstType(vpiDecConst);
    c->setSize(32);
    setTypespec(c, UhdmType::IntTypespec, true, v, s);
    return c;
  };

  LogicTypespec* const lts = s.make<LogicTypespec>();
  lts->setSigned(false);

  Range* const pr = s.make<Range>();
  pr->setLeftExpr(makeInt("7"));
  pr->setRightExpr(makeInt("0"));

  RangeCollection* const prc = s.makeCollection<Range>();
  prc->push_back(pr);
  lts->setRanges(prc);

  Constant* vec = s.make<Constant>();
  vec->setValue("10110011");
  vec->setDecompile("10110011");
  vec->setConstType(vpiBinaryConst);
  vec->setSize(8);
  uhdm::setTypespec(vec, lts);

  m_constants.emplace("vec", vec);

  IndexedPartSelect* ipsPos = s.make<IndexedPartSelect>();
  ipsPos->setName("vec");
  ipsPos->setBaseExpr(makeInt("2"));
  ipsPos->setWidthExpr(makeInt("3"));
  ipsPos->setIndexedPartSelectType(vpiPosIndexed);

  Expr* resultPos = nullptr;
  bool okPos = eval.reduceExpr(ipsPos, ipsPos, &resultPos, true);
  ASSERT_TRUE(okPos);
  ASSERT_NE(resultPos, nullptr);

  Constant* rcPos = any_cast<Constant>(resultPos);
  ASSERT_NE(rcPos, nullptr);

  ASSERT_EQ(rcPos->getValue(), "100");
  ASSERT_EQ(rcPos->getConstType(), vpiBinaryConst);
  ASSERT_EQ(rcPos->getSize(), 3);
  ASSERT_EQ(rcPos->getDecompile(), "100");

  IndexedPartSelect* ipsNeg = s.make<IndexedPartSelect>();
  ipsNeg->setName("vec");
  ipsNeg->setBaseExpr(makeInt("5"));
  ipsNeg->setWidthExpr(makeInt("3"));
  ipsNeg->setIndexedPartSelectType(vpiNegIndexed);

  Expr* resultNeg = nullptr;
  bool okNeg = eval.reduceExpr(ipsNeg, ipsNeg, &resultNeg, true);
  ASSERT_TRUE(okNeg);
  ASSERT_NE(resultNeg, nullptr);

  Constant* rcNeg = any_cast<Constant>(resultNeg);
  ASSERT_NE(rcNeg, nullptr);

  ASSERT_EQ(rcNeg->getValue(), "110");
  ASSERT_EQ(rcNeg->getConstType(), vpiBinaryConst);
  ASSERT_EQ(rcNeg->getSize(), 3);
  ASSERT_EQ(rcNeg->getDecompile(), "110");
}

TEST(ExprEvalReduceExpr, VarSelect_Concat) {
  TestObjectProvider provider;
  Serializer& s = provider.m_serializer;
  ExprEval eval(&provider);

  auto makeInt = [&](const char* v) {
    Constant* c = s.make<Constant>();
    c->setValue(v);
    c->setDecompile(v);
    c->setConstType(vpiDecConst);
    c->setSize(32);
    setTypespec(c, UhdmType::IntTypespec, false, v, s);
    return c;
  };

  Constant* a = makeInt("10");
  Constant* b = makeInt("20");
  Constant* c = makeInt("30");

  Operation* concat = s.make<Operation>();
  concat->setOpType(vpiConcatOp);

  AnyCollection* ops = s.makeCollection<Any>();
  ops->push_back(a);
  ops->push_back(b);
  ops->push_back(c);
  concat->setOperands(ops);

  provider.m_objects.emplace("vec", concat);

  VarSelect* vs = s.make<VarSelect>();
  vs->setName("vec");

  ExprCollection* indexes = s.makeCollection<Expr>();
  indexes->push_back(makeInt("1"));
  vs->setIndexes(indexes);

  Expr* result = nullptr;
  bool ok = eval.reduceExpr(vs, vs, &result, true);

  ASSERT_TRUE(ok);
  ASSERT_NE(result, nullptr);

  Constant* rc = any_cast<Constant>(result);
  ASSERT_NE(rc, nullptr);

  ASSERT_EQ(rc->getValue(), "20");
  ASSERT_EQ(rc->getConstType(), vpiDecConst);
}

// TEST(ExprEvalReduceExpr, BitSelect_ConstantVector) {
//   TestObjectProvider provider;
//   Serializer& s = provider.m_serializer;
//   Constants& m_constants = provider.m_constants;
//   ExprEval eval(&provider);
//
//   auto makeInt = [&](const char* v) {
//     Constant* c = s.make<Constant>();
//     c->setValue(v);
//     c->setDecompile(v);
//     c->setConstType(vpiDecConst);
//     c->setSize(32);
//     setTypespec(c, UhdmType::IntTypespec, false, v, s);
//     return c;
//   };
//
//
//   LogicTypespec* const lts = s.make<LogicTypespec>();
//   lts->setSigned(false);
//
//   Range* const pr = s.make<Range>();
//   pr->setLeftExpr(makeInt("7"));
//   pr->setRightExpr(makeInt("0"));
//
//   RangeCollection* const prc = s.makeCollection<Range>();
//   prc->push_back(pr);
//   lts->setRanges(prc);
//
//
//   Constant* vec = s.make<Constant>();
//   vec->setValue("10110011");
//   vec->setDecompile("10110011");
//   vec->setConstType(vpiBinaryConst);
//   vec->setSize(8);
//   uhdm::setTypespec(vec, lts);
//
//   m_constants.emplace("vec", vec);
//
//   Constant* idx = makeInt("0");
//
//   BitSelect* bs = s.make<BitSelect>();
//   bs->setName("vec");
//   bs->setIndex(idx);
//
//   Expr* result = nullptr;
//   bool ok = eval.reduceExpr(bs, bs, &result, true);
//
//   ASSERT_TRUE(ok);
//   ASSERT_NE(result, nullptr);
//
//   Constant* rc = any_cast<Constant>(result);
//   ASSERT_NE(rc, nullptr);
//
//   ASSERT_EQ(rc->getValue(), "1");
//   ASSERT_EQ(rc->getConstType(), vpiBinaryConst);
// }

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
