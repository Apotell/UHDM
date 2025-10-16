#include "uhdm/ExprEval.h"

#include "gtest/gtest.h"
#include "uhdm/uhdm.h"

using namespace uhdm;
using Constants = std::map<std::string_view, Constant*>;

static const std::map<int32_t, std::string_view> kConstTypeMap{
    {vpiIntConst, "INT:"}, {vpiUIntConst, "UINT:"}, {vpiDecConst, "DEC:"}};

class UnaryOperationTest
    : public testing::TestWithParam<std::tuple<
          int32_t, int32_t, std::string_view, int32_t, std::string_view>> {
 public:
  Serializer m_serializer;
  Constants m_constants;
  ExprEval m_evaluator;
  Operation* m_operation = nullptr;

  void SetUp() override {
    RefObj* const ro = m_serializer.make<RefObj>();
    ro->setName("a");

    m_operation = m_serializer.make<Operation>();
    AnyCollection* const operands = m_operation->getOperands(true);
    operands->emplace_back(ro);
    ro->setParent(m_operation);

    m_constants.emplace("a", m_serializer.make<Constant>());

    m_evaluator.setGetObjectFunctor(
        [=](std::string_view name, const Any* inst, const Any* pexpr) -> Any* {
          Constants::const_iterator it = m_constants.find(name);
          return (it == m_constants.cend()) ? nullptr : it->second;
        });
    m_evaluator.setGetValueFunctor(
        [=](std::string_view name, const Any* inst, const Any* pexpr) -> Any* {
          Constants::const_iterator it = m_constants.find(name);
          return (it == m_constants.cend()) ? nullptr : it->second;
        });
  }

  void TearDown() override {
    m_operation = nullptr;
    m_serializer.purge();
    m_constants.clear();
  }
};

class BinaryOperationTest
    : public testing::TestWithParam<
          std::tuple<int32_t, int32_t, std::string_view, int32_t,
                     std::string_view, int32_t, std::string_view>> {
 public:
  Serializer m_serializer;
  Constants m_constants;
  ExprEval m_evaluator;
  Operation* m_operation = nullptr;

  void SetUp() override {
    RefObj* const ro1 = m_serializer.make<RefObj>();
    ro1->setName("a");

    RefObj* const ro2 = m_serializer.make<RefObj>();
    ro2->setName("b");

    m_operation = m_serializer.make<Operation>();
    AnyCollection* const operands = m_operation->getOperands(true);
    operands->emplace_back(ro1);
    operands->emplace_back(ro2);
    ro1->setParent(m_operation);
    ro2->setParent(m_operation);

    m_constants.emplace("a", m_serializer.make<Constant>());
    m_constants.emplace("b", m_serializer.make<Constant>());

    m_evaluator.setGetObjectFunctor(
        [=](std::string_view name, const Any* inst, const Any* pexpr) -> Any* {
          Constants::const_iterator it = m_constants.find(name);
          return (it == m_constants.cend()) ? nullptr : it->second;
        });
    m_evaluator.setGetValueFunctor(
        [=](std::string_view name, const Any* inst, const Any* pexpr) -> Any* {
          Constants::const_iterator it = m_constants.find(name);
          return (it == m_constants.cend()) ? nullptr : it->second;
        });
  }

  void TearDown() override {
    m_operation = nullptr;
    m_serializer.purge();
    m_constants.clear();
  }
};

TEST_P(UnaryOperationTest, UnaryOperators) {
  auto [opType, cType1, v1, rType, r] = GetParam();

  m_operation->setOpType(opType);
  m_constants["a"]->setValue(v1);
  m_constants["a"]->setConstType(cType1);

  bool invalidValue = false;
  if (Any* const result = m_evaluator.reduceExpr(m_operation, invalidValue,
                                                 nullptr, m_operation, true)) {
    EXPECT_FALSE(invalidValue);
    EXPECT_EQ(result->getUhdmType(), UhdmType::Constant);

    if (Constant* const c = any_cast<Constant>(result)) {
      EXPECT_EQ(c->getValue(), r);
      EXPECT_EQ(c->getConstType(), rType);
    }
  } else {
    GTEST_FAIL();
  }
}

TEST_P(BinaryOperationTest, BinaryOperators) {
  auto [opType, cType1, v1, cType2, v2, rType, r] = GetParam();

  m_operation->setOpType(opType);
  m_constants["a"]->setValue(v1);
  m_constants["a"]->setConstType(cType1);
  m_constants["b"]->setValue(v2);
  m_constants["b"]->setConstType(cType2);

  bool invalidValue = false;
  if (Any* const result = m_evaluator.reduceExpr(m_operation, invalidValue,
                                                 nullptr, m_operation, true)) {
    EXPECT_FALSE(invalidValue);
    EXPECT_EQ(result->getUhdmType(), UhdmType::Constant);

    if (Constant* const c = any_cast<Constant>(result)) {
      EXPECT_EQ(c->getValue(), r);
      EXPECT_EQ(c->getConstType(), rType);
    }
  } else {
    GTEST_FAIL();
  }
}

INSTANTIATE_TEST_SUITE_P(
    UnaryOperators, UnaryOperationTest,
    testing::Values(std::make_tuple(vpiMinusOp, vpiIntConst, "INT:10",
                                    vpiIntConst, "INT:-10")
                    // std::make_tuple(vpiMinusOp, vpiUIntConst, "UINT:10",
                    //                 vpiUIntConst, "UINT:-10"),
                    // std::make_tuple(vpiNotOp, vpiIntConst, "INT:10",
                    //                 vpiIntConst, "INT:-10"),
                    // std::make_tuple(vpiNotOp, vpiUIntConst, "UINT:10",
                    //                 vpiUIntConst, "UINT:-10")
                    ));

INSTANTIATE_TEST_SUITE_P(
    BinaryOperators, BinaryOperationTest,
    testing::Values(
        std::make_tuple(vpiPlusOp, vpiUIntConst, "UINT:10", vpiUIntConst,
                        "UINT:20", vpiUIntConst, "UINT:30")
        // std::make_tuple(vpiPlusOp, vpiUIntConst, "UINT:10", vpiUIntConst,
        //                 "UINT:20", vpiUIntConst, "UINT:30"),
        // std::make_tuple(vpiSubOp, vpiIntConst, "INT:10", vpiIntConst,
        //                 "INT:20", vpiIntConst, "INT:-10"),
        // std::make_tuple(vpiSubOp, vpiUIntConst, "UINT:20", vpiUIntConst,
        //                 "UINT:10", vpiUIntConst, "UINT:10")
        ));
