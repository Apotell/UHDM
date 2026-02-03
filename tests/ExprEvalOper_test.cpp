#include "gtest/gtest.h"
#include "uhdm/ExprEval.h"
#include "uhdm/Utils.h"
#include "uhdm/uhdm.h"

using namespace uhdm;
using Constants = std::map<std::string_view, Constant*>;

class TestObjectProvider : public ObjectProvider {
 public:
  Serializer m_serializer;
  Constants m_constants;

  const Any* getObject(std::string_view name, const Any* inst, const Any* pexpr, bool muteErrors = false) final {
    Constants::const_iterator it = m_constants.find(name);
    return (it == m_constants.cend()) ? nullptr : it->second;
  }
  const TaskFunc* getTaskFunc(std::string_view name, const Any* inst, const Any* pexpr, bool muteErrors = false) final {
    return nullptr;
  }
  Any* getValue(std::string_view name, const Any* inst, const Any* pexpr, bool muteErrors = false) final {
    Constants::const_iterator it = m_constants.find(name);
    return (it == m_constants.cend()) ? nullptr : it->second;
  }
};

static Constant* makeConst(const std::string& v, Serializer& s) {
  Constant* c = s.make<Constant>();
  c->setConstType(vpiIntConst);
  c->setValue(v);
  c->setSize(v.size());
  return c;
}

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
  constant->setValue(svalue);
}

class UnaryOperationTest
    : public testing::TestWithParam<
          std::tuple<std::string, int32_t, UhdmType, bool, std::string_view, UhdmType, bool, std::string_view>>,
      public ObjectProvider {
 public:
  Serializer m_serializer;
  Constants m_constants;
  ExprEval m_evaluator;
  Operation* m_operation = nullptr;

  UnaryOperationTest() : m_evaluator(this) {}

  void SetUp() override {
    RefObj* const ro = m_serializer.make<RefObj>();
    ro->setName("a");

    m_operation = m_serializer.make<Operation>();
    AnyCollection* const operands = m_operation->getOperands(true);
    operands->emplace_back(ro);
    ro->setParent(m_operation);

    Constant* const c = m_serializer.make<Constant>();
    RefTypespec* const rt = m_serializer.make<RefTypespec>();
    c->setTypespec(rt);
    ro->setActual(c);
    m_constants.emplace("a", c);
  }

  const Any* getObject(std::string_view name, const Any* inst, const Any* pexpr, bool muteErrors = false) final {
    Constants::const_iterator it = m_constants.find(name);
    return (it == m_constants.cend()) ? nullptr : it->second;
  }
  const TaskFunc* getTaskFunc(std::string_view name, const Any* inst, const Any* pexpr, bool muteErrors = false) final {
    return nullptr;
  }
  Any* getValue(std::string_view name, const Any* inst, const Any* pexpr, bool muteErrors = false) final {
    Constants::const_iterator it = m_constants.find(name);
    return (it == m_constants.cend()) ? nullptr : it->second;
  }

  void TearDown() override {
    m_operation = nullptr;
    m_serializer.purge();
    m_constants.clear();
  }
};

class BinaryOperationTest : public testing::TestWithParam<
                                std::tuple<std::string, int32_t, UhdmType, bool, int32_t, std::string_view, UhdmType,
                                           bool, int32_t, std::string_view, UhdmType, bool, int32_t, std::string_view>>,
                            public ObjectProvider {
 public:
  Serializer m_serializer;
  Constants m_constants;
  ExprEval m_evaluator;
  Operation* m_operation = nullptr;

  BinaryOperationTest() : m_evaluator(this) {}

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

    Constant* const ac = m_serializer.make<Constant>();
    RefTypespec* const art = m_serializer.make<RefTypespec>();
    ac->setTypespec(art);
    ro1->setActual(ac);
    m_constants.emplace("a", ac);

    Constant* const bc = m_serializer.make<Constant>();
    RefTypespec* const brt = m_serializer.make<RefTypespec>();
    bc->setTypespec(brt);
    ro2->setActual(bc);
    m_constants.emplace("b", bc);
  }

  const Any* getObject(std::string_view name, const Any* inst, const Any* pexpr, bool muteErrors = false) final {
    Constants::const_iterator it = m_constants.find(name);
    return (it == m_constants.cend()) ? nullptr : it->second;
  }
  const TaskFunc* getTaskFunc(std::string_view name, const Any* inst, const Any* pexpr, bool muteErrors = false) final {
    return nullptr;
  }
  Any* getValue(std::string_view name, const Any* inst, const Any* pexpr, bool muteErrors = false) final {
    Constants::const_iterator it = m_constants.find(name);
    return (it == m_constants.cend()) ? nullptr : it->second;
  }

  void TearDown() override {
    m_operation = nullptr;
    m_serializer.purge();
    m_constants.clear();
  }
};

TEST_P(UnaryOperationTest, UnaryOperators) {
  const auto& [testname, opType, inputTypespecType, inputTypespecSign, inputValue, resultTypespecType,
               resultTypespecSign, resultValue] = GetParam();

  m_operation->setOpType(opType);
  m_constants["a"]->setValue(inputValue);
  setTypespec(m_constants["a"], inputTypespecType, inputTypespecSign, inputValue, m_serializer);

  int32_t inputConstType = 0;
  if ((inputTypespecType == UhdmType::LogicTypespec) || (inputTypespecType == UhdmType::IntegerTypespec) ||
      (inputTypespecType == UhdmType::BitTypespec)) {
    inputConstType = vpiBinaryConst;
    m_constants["a"]->setSize(inputValue.length());
  } else if (inputTypespecType == UhdmType::RealTypespec) {
    inputConstType = vpiRealConst;
    m_constants["a"]->setSize(64);
  } else if (inputTypespecType == UhdmType::ShortRealTypespec) {
    inputConstType = vpiRealConst;
    m_constants["a"]->setSize(32);
  } else if (inputTypespecType == UhdmType::TimeTypespec) {
    inputConstType = vpiTimeConst;
    m_constants["a"]->setSize(64);
  } else {
    inputConstType = vpiDecConst;
    m_constants["a"]->setSize(32);
  }
  m_constants["a"]->setConstType(inputConstType);

  Expr* result = nullptr;
  const bool succeeded = m_evaluator.reduceExpr(m_operation, m_operation, &result, true);
  ASSERT_TRUE(succeeded);
  ASSERT_NE(result, nullptr);

  Constant* const c = any_cast<Constant>(result);
  ASSERT_NE(c, nullptr);
  ASSERT_EQ(c->getValue(), resultValue);

  Typespec* const t = getTypespec(result);
  ASSERT_NE(t, nullptr);
  ASSERT_EQ(t->getUhdmType(), resultTypespecType);
  ASSERT_EQ(getSigned(t), resultTypespecSign);

  ASSERT_EQ(c->getConstType(), inputConstType);
}

TEST_P(BinaryOperationTest, BinaryOperators) {
  const auto& [testname, opType, inputTypespecType1, inputTypespecSign1, inputConstType1, inputValue1,
               inputTypespecType2, inputTypespecSign2, inputConstType2, inputValue2, resultTypespecType,
               resultTypespecSign, resultConstType, resultValue] = GetParam();

  m_operation->setOpType(opType);
  setTypespec(m_constants["a"], inputTypespecType1, inputTypespecSign1, inputValue1, m_serializer);
  setTypespec(m_constants["b"], inputTypespecType2, inputTypespecSign2, inputValue2, m_serializer);
  m_constants["a"]->setConstType(inputConstType1);
  m_constants["b"]->setConstType(inputConstType2);

  if ((inputTypespecType1 == UhdmType::LogicTypespec) || (inputTypespecType1 == UhdmType::IntegerTypespec) ||
      (inputTypespecType1 == UhdmType::BitTypespec) || (inputTypespecType2 == UhdmType::LogicTypespec) ||
      (inputTypespecType2 == UhdmType::IntegerTypespec) || (inputTypespecType2 == UhdmType::BitTypespec)) {
    size_t max = std::max(inputValue1.length(), inputValue2.length());
    m_constants["a"]->setSize(max);
  } else if (inputTypespecType1 == UhdmType::RealTypespec || inputTypespecType2 == UhdmType::RealTypespec) {
    m_constants["a"]->setSize(64);
  } else if (inputTypespecType1 == UhdmType::ShortRealTypespec || inputTypespecType2 == UhdmType::ShortRealTypespec) {
    m_constants["a"]->setSize(32);
  } else {
    m_constants["a"]->setSize(32);
  }

  Expr* result = nullptr;
  const bool succeeded = m_evaluator.reduceExpr(m_operation, m_operation, &result, true);
  ASSERT_TRUE(succeeded);
  ASSERT_NE(result, nullptr);

  Constant* const c = any_cast<Constant>(result);
  ASSERT_NE(c, nullptr);
  ASSERT_EQ(c->getValue(), resultValue);

  Typespec* const t = getTypespec(result);
  ASSERT_NE(t, nullptr);
  ASSERT_EQ(t->getUhdmType(), resultTypespecType);
  ASSERT_EQ(getSigned(t), resultTypespecSign);

  if ((inputTypespecType1 == UhdmType::IntegerTypespec) || (inputTypespecType1 == UhdmType::LogicTypespec) ||
      (inputTypespecType1 == UhdmType::BitTypespec) || (inputTypespecType1 == UhdmType::IntegerTypespec) ||
      (inputTypespecType1 == UhdmType::LogicTypespec) || (inputTypespecType1 == UhdmType::BitTypespec)) {
    ASSERT_EQ(c->getConstType(), resultConstType);
  }
}

#if 1
INSTANTIATE_TEST_SUITE_P(
    UnaryOperators, UnaryOperationTest,
    testing::Values(
        // clang-format off
        // int / 32-bit signed/unsigned
        std::make_tuple("UnaryTest_0001", vpiMinusOp, UhdmType::IntTypespec, true,   "10",         UhdmType::IntTypespec, true,  "-10"),
        std::make_tuple("UnaryTest_0002", vpiMinusOp, UhdmType::IntTypespec, false,  "20",         UhdmType::IntTypespec, false, "4294967276"),
        std::make_tuple("UnaryTest_0003", vpiMinusOp, UhdmType::IntTypespec, false,  "1",          UhdmType::IntTypespec, false, "4294967295"),
        std::make_tuple("UnaryTest_0004", vpiMinusOp, UhdmType::IntTypespec, false,  "4294967295", UhdmType::IntTypespec, false, "1"),
        std::make_tuple("UnaryTest_0005", vpiMinusOp, UhdmType::IntTypespec, false,  "0",          UhdmType::IntTypespec, false, "0"),
        std::make_tuple("UnaryTest_0006", vpiMinusOp, UhdmType::IntTypespec, true,   "0",          UhdmType::IntTypespec, true,  "0"),

        // longint / 64-bit signed/unsigned
        std::make_tuple("UnaryTest_0007", vpiMinusOp, UhdmType::LongIntTypespec, true,  "30",  UhdmType::LongIntTypespec, true,  "-30"),
        std::make_tuple("UnaryTest_0008", vpiMinusOp, UhdmType::LongIntTypespec, false, "40",  UhdmType::LongIntTypespec, false, "18446744073709551576"),
        std::make_tuple("UnaryTest_0009", vpiMinusOp, UhdmType::LongIntTypespec, false, "1",   UhdmType::LongIntTypespec, false, "18446744073709551615"),
        std::make_tuple("UnaryTest_0010", vpiMinusOp, UhdmType::LongIntTypespec, false, "255", UhdmType::LongIntTypespec, false, "18446744073709551361"),

        // shortint (16-bit signed) and byte (8-bit)
        std::make_tuple("UnaryTest_0011", vpiMinusOp, UhdmType::ShortIntTypespec, true,  "32767", UhdmType::ShortIntTypespec, true,  "-32767"),
        std::make_tuple("UnaryTest_0012", vpiMinusOp, UhdmType::ShortIntTypespec, false, "1",     UhdmType::ShortIntTypespec, false, "65535"),
        std::make_tuple("UnaryTest_0013", vpiMinusOp, UhdmType::ByteTypespec,     false, "1",     UhdmType::ByteTypespec,     false, "255"),
        std::make_tuple("UnaryTest_0014", vpiMinusOp, UhdmType::ByteTypespec,     true,  "127",   UhdmType::ByteTypespec,     true,  "-127"),

        // integer (SystemVerilog 'integer' typically 32-bit signed)
        std::make_tuple("UnaryTest_0015", vpiMinusOp, UhdmType::IntegerTypespec, true, "01111111111111111111111111111111", UhdmType::IntegerTypespec, true, "10000000000000000000000000000001"),

        // real / shortreal (floating point)
        std::make_tuple("UnaryTest_0016", vpiMinusOp, UhdmType::RealTypespec,      true, "3.14159", UhdmType::RealTypespec,      true, "-3.14159"),
        std::make_tuple("UnaryTest_0017", vpiMinusOp, UhdmType::ShortRealTypespec, true, "0.5",     UhdmType::ShortRealTypespec, true, "-0.5"),

        // time (treated as 64-bit unsigned in Verilog)
        std::make_tuple("UnaryTest_0018", vpiMinusOp, UhdmType::TimeTypespec,  false, "1",                                UhdmType::TimeTypespec,  false, "18446744073709551615"),
        std::make_tuple("UnaryTest_0019", vpiMinusOp, UhdmType::LogicTypespec, true,  "00000000000000000000000000000101", UhdmType::LogicTypespec, true,  "11111111111111111111111111111011"),
        std::make_tuple("UnaryTest_0020", vpiMinusOp, UhdmType::LogicTypespec, false, "11111111111111111111111111111111", UhdmType::LogicTypespec, false, "00000000000000000000000000000001"),
        std::make_tuple("UnaryTest_0021", vpiMinusOp, UhdmType::LogicTypespec, true,  "00001111",                         UhdmType::LogicTypespec, true,  "11110001"),
        std::make_tuple("UnaryTest_0022", vpiMinusOp, UhdmType::LogicTypespec, false, "11111111111111111111111111111111", UhdmType::LogicTypespec, false, "00000000000000000000000000000001"),

        // hex decimal example for 64-bit signed input
        std::make_tuple("UnaryTest_0023", vpiMinusOp, UhdmType::LongIntTypespec, true, "9223372036854775807", UhdmType::LongIntTypespec, true, "-9223372036854775807"),

        //---------------------- Unary Plus ----------------------
        std::make_tuple("UnaryTest_0024", vpiPlusOp, UhdmType::IntTypespec, true,  "10",         UhdmType::IntTypespec, true,  "10"),
        std::make_tuple("UnaryTest_0025", vpiPlusOp, UhdmType::IntTypespec, true,  "-10",        UhdmType::IntTypespec, true,  "-10"),
        std::make_tuple("UnaryTest_0026", vpiPlusOp, UhdmType::IntTypespec, false, "20",         UhdmType::IntTypespec, false, "20"),
        std::make_tuple("UnaryTest_0027", vpiPlusOp, UhdmType::IntTypespec, false, "4294967295", UhdmType::IntTypespec, false, "4294967295"),

        std::make_tuple("UnaryTest_0028", vpiPlusOp, UhdmType::LongIntTypespec, true,  "30",                   UhdmType::LongIntTypespec, true,  "30"),
        std::make_tuple("UnaryTest_0029", vpiPlusOp, UhdmType::LongIntTypespec, true,  "-30",                  UhdmType::LongIntTypespec, true,  "-30"),
        std::make_tuple("UnaryTest_0030", vpiPlusOp, UhdmType::LongIntTypespec, false, "40",                   UhdmType::LongIntTypespec, false, "40"),
        std::make_tuple("UnaryTest_0031", vpiPlusOp, UhdmType::LongIntTypespec, false, "18446744073709551615", UhdmType::LongIntTypespec, false, "18446744073709551615"),

        std::make_tuple("UnaryTest_0032", vpiPlusOp, UhdmType::ShortIntTypespec, true,  "32767",  UhdmType::ShortIntTypespec, true,  "32767"),
        std::make_tuple("UnaryTest_0033", vpiPlusOp, UhdmType::ShortIntTypespec, true,  "-32767", UhdmType::ShortIntTypespec, true,  "-32767"),
        std::make_tuple("UnaryTest_0034", vpiPlusOp, UhdmType::ShortIntTypespec, false, "65535",  UhdmType::ShortIntTypespec, false, "65535"),

        std::make_tuple("UnaryTest_0035", vpiPlusOp, UhdmType::ByteTypespec, true,  "127",  UhdmType::ByteTypespec, true,  "127"),
        std::make_tuple("UnaryTest_0036", vpiPlusOp, UhdmType::ByteTypespec, true,  "-127", UhdmType::ByteTypespec, true,  "-127"),
        std::make_tuple("UnaryTest_0037", vpiPlusOp, UhdmType::ByteTypespec, false, "255",  UhdmType::ByteTypespec, false, "255"),

        std::make_tuple("UnaryTest_0038", vpiPlusOp, UhdmType::IntegerTypespec, true, "01111111111111111111111111111111", UhdmType::IntegerTypespec, true, "01111111111111111111111111111111"),
        std::make_tuple("UnaryTest_0039", vpiPlusOp, UhdmType::IntegerTypespec, true, "10000000000000000000000000000001", UhdmType::IntegerTypespec, true, "10000000000000000000000000000001"),

        std::make_tuple("UnaryTest_0040", vpiPlusOp, UhdmType::LogicTypespec, true,  "00000101",                         UhdmType::LogicTypespec, true,  "00000101"),
        std::make_tuple("UnaryTest_0041", vpiPlusOp, UhdmType::LogicTypespec, true,  "000000011111",                     UhdmType::LogicTypespec, true,  "000000011111"),
        std::make_tuple("UnaryTest_0042", vpiPlusOp, UhdmType::LogicTypespec, false, "11111111111111111111111111111111", UhdmType::LogicTypespec, false, "11111111111111111111111111111111"),

        std::make_tuple("UnaryTest_0043", vpiPlusOp, UhdmType::RealTypespec, true, "3.14159",  UhdmType::RealTypespec, true, "3.14159"),
        std::make_tuple("UnaryTest_0044", vpiPlusOp, UhdmType::RealTypespec, true, "-3.14159", UhdmType::RealTypespec, true, "-3.14159"),

        std::make_tuple("UnaryTest_0045", vpiPlusOp, UhdmType::ShortRealTypespec, true, "0.5",  UhdmType::ShortRealTypespec, true, "0.5"),
        std::make_tuple("UnaryTest_0046", vpiPlusOp, UhdmType::ShortRealTypespec, true, "-0.5", UhdmType::ShortRealTypespec, true, "-0.5"),

        std::make_tuple("UnaryTest_0047", vpiPlusOp, UhdmType::TimeTypespec, false, "1",                    UhdmType::TimeTypespec, false, "1"),
        std::make_tuple("UnaryTest_0048", vpiPlusOp, UhdmType::TimeTypespec, false, "18446744073709551615", UhdmType::TimeTypespec, false, "18446744073709551615"),

        //----------------------------- Logical NOT -------------------------
        std::make_tuple("UnaryTest_0049", vpiNotOp, UhdmType::IntTypespec, true,  "0",   UhdmType::LogicTypespec, true,  "1"),
        std::make_tuple("UnaryTest_0050", vpiNotOp, UhdmType::IntTypespec, true,  "5",   UhdmType::LogicTypespec, true,  "0"),
        std::make_tuple("UnaryTest_0051", vpiNotOp, UhdmType::IntTypespec, false, "0",   UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0052", vpiNotOp, UhdmType::IntTypespec, false, "100", UhdmType::LogicTypespec, false, "0"),

        std::make_tuple("UnaryTest_0053", vpiNotOp, UhdmType::LongIntTypespec, true,  "0",                    UhdmType::LogicTypespec, true,  "1"),
        std::make_tuple("UnaryTest_0054", vpiNotOp, UhdmType::LongIntTypespec, true,  "123",                  UhdmType::LogicTypespec, true,  "0"),
        std::make_tuple("UnaryTest_0055", vpiNotOp, UhdmType::LongIntTypespec, false, "0",                    UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0056", vpiNotOp, UhdmType::LongIntTypespec, false, "18446744073709551615", UhdmType::LogicTypespec, false, "0"),

        std::make_tuple("UnaryTest_0057", vpiNotOp, UhdmType::ShortIntTypespec, true, "0",  UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0058", vpiNotOp, UhdmType::ShortIntTypespec, true, "-5", UhdmType::LogicTypespec, true, "0"),

        std::make_tuple("UnaryTest_0059", vpiNotOp, UhdmType::ByteTypespec, true, "0",   UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0060", vpiNotOp, UhdmType::ByteTypespec, true, "100", UhdmType::LogicTypespec, true, "0"),

        std::make_tuple("UnaryTest_0061", vpiNotOp, UhdmType::IntegerTypespec, true, "0",        UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0062", vpiNotOp, UhdmType::IntegerTypespec, true, "01100011", UhdmType::LogicTypespec, true, "0"),

        std::make_tuple("UnaryTest_0063", vpiNotOp, UhdmType::LogicTypespec, true,  "00000000",                         UhdmType::LogicTypespec, true,  "1"),
        std::make_tuple("UnaryTest_0064", vpiNotOp, UhdmType::LogicTypespec, true,  "00000101",                         UhdmType::LogicTypespec, true,  "0"),
        std::make_tuple("UnaryTest_0065", vpiNotOp, UhdmType::LogicTypespec, false, "00000000000000000000000000000000", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0066", vpiNotOp, UhdmType::LogicTypespec, false, "00000000000000000000000011111111", UhdmType::LogicTypespec, false, "0"),

        std::make_tuple("UnaryTest_0067", vpiNotOp, UhdmType::RealTypespec, true, "0.0",   UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0068", vpiNotOp, UhdmType::RealTypespec, true, "3.14",  UhdmType::LogicTypespec, true, "0"),
        std::make_tuple("UnaryTest_0069", vpiNotOp, UhdmType::RealTypespec, true, "-2.71", UhdmType::LogicTypespec, true, "0"),

        std::make_tuple("UnaryTest_0070", vpiNotOp, UhdmType::ShortRealTypespec, true,  "0.0", UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0071", vpiNotOp, UhdmType::ShortRealTypespec, true,  "0.5", UhdmType::LogicTypespec, true, "0"),
        std::make_tuple("UnaryTest_0072", vpiNotOp, UhdmType::ShortRealTypespec, true, "-0.5", UhdmType::LogicTypespec, true, "0"),

        std::make_tuple("UnaryTest_0073", vpiNotOp, UhdmType::TimeTypespec, false, "0",   UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0074", vpiNotOp, UhdmType::TimeTypespec, false, "100", UhdmType::LogicTypespec, false, "0"),

        //---------------------------------- Bitwise NOT --------------------------
        std::make_tuple("UnaryTest_0075", vpiBitNegOp, UhdmType::IntTypespec, true,  "5",  UhdmType::IntTypespec, true,  "-6"),
        std::make_tuple("UnaryTest_0076", vpiBitNegOp, UhdmType::IntTypespec, false, "5",  UhdmType::IntTypespec, false, "4294967290"),
        std::make_tuple("UnaryTest_0077", vpiBitNegOp, UhdmType::IntTypespec, true,  "0",  UhdmType::IntTypespec, true,  "-1"),
        std::make_tuple("UnaryTest_0078", vpiBitNegOp, UhdmType::IntTypespec, false, "0",  UhdmType::IntTypespec, false, "4294967295"),
        std::make_tuple("UnaryTest_0079", vpiBitNegOp, UhdmType::IntTypespec, true,  "-5", UhdmType::IntTypespec, true,  "4"),
        std::make_tuple("UnaryTest_0080", vpiBitNegOp, UhdmType::IntTypespec, false, "-5", UhdmType::IntTypespec, false, "4"),

        std::make_tuple("UnaryTest_0081", vpiBitNegOp, UhdmType::LongIntTypespec, true,  "5",  UhdmType::LongIntTypespec, true,  "-6"),
        std::make_tuple("UnaryTest_0082", vpiBitNegOp, UhdmType::LongIntTypespec, false, "5",  UhdmType::LongIntTypespec, false, "18446744073709551610"),
        std::make_tuple("UnaryTest_0083", vpiBitNegOp, UhdmType::LongIntTypespec, true,  "0",  UhdmType::LongIntTypespec, true,  "-1"),
        std::make_tuple("UnaryTest_0084", vpiBitNegOp, UhdmType::LongIntTypespec, false, "0",  UhdmType::LongIntTypespec, false, "18446744073709551615"),
        std::make_tuple("UnaryTest_0085", vpiBitNegOp, UhdmType::LongIntTypespec, true,  "-5", UhdmType::LongIntTypespec, true,  "4"),
        std::make_tuple("UnaryTest_0086", vpiBitNegOp, UhdmType::LongIntTypespec, false, "-5", UhdmType::LongIntTypespec, false, "4"),

        std::make_tuple("UnaryTest_0087", vpiBitNegOp, UhdmType::ShortIntTypespec, true,  "5",  UhdmType::ShortIntTypespec, true,  "-6"),
        std::make_tuple("UnaryTest_0088", vpiBitNegOp, UhdmType::ShortIntTypespec, false, "5",  UhdmType::ShortIntTypespec, false, "65530"),
        std::make_tuple("UnaryTest_0089", vpiBitNegOp, UhdmType::ShortIntTypespec, true,  "-5", UhdmType::ShortIntTypespec, true,  "4"),
        std::make_tuple("UnaryTest_0090", vpiBitNegOp, UhdmType::ShortIntTypespec, false, "-5", UhdmType::ShortIntTypespec, false, "4"),
        std::make_tuple("UnaryTest_0091", vpiBitNegOp, UhdmType::ShortIntTypespec, true,  "0",  UhdmType::ShortIntTypespec, true,  "-1"),
        std::make_tuple("UnaryTest_0092", vpiBitNegOp, UhdmType::ShortIntTypespec, false, "0",  UhdmType::ShortIntTypespec, false, "65535"),

        std::make_tuple("UnaryTest_0093", vpiBitNegOp, UhdmType::ByteTypespec, true,  "5",  UhdmType::ByteTypespec, true,  "-6"),
        std::make_tuple("UnaryTest_0094", vpiBitNegOp, UhdmType::ByteTypespec, false, "5",  UhdmType::ByteTypespec, false, "250"),
        std::make_tuple("UnaryTest_0095", vpiBitNegOp, UhdmType::ByteTypespec, true,  "-5", UhdmType::ByteTypespec, true,  "4"),
        std::make_tuple("UnaryTest_0096", vpiBitNegOp, UhdmType::ByteTypespec, false, "-5", UhdmType::ByteTypespec, false, "4"),
        std::make_tuple("UnaryTest_0097", vpiBitNegOp, UhdmType::ByteTypespec, true,  "0",  UhdmType::ByteTypespec, true,  "-1"),
        std::make_tuple("UnaryTest_0098", vpiBitNegOp, UhdmType::ByteTypespec, false, "0",  UhdmType::ByteTypespec, false, "255"),

        std::make_tuple("UnaryTest_0099", vpiBitNegOp, UhdmType::IntegerTypespec, true,  "00000101",                         UhdmType::IntegerTypespec, true,  "11111010"),
        std::make_tuple("UnaryTest_0102", vpiBitNegOp, UhdmType::IntegerTypespec, false, "11111111111111111111111111111011", UhdmType::IntegerTypespec, false, "00000000000000000000000000000100"),

        std::make_tuple("UnaryTest_0103", vpiBitNegOp, UhdmType::LogicTypespec, true,  "00000101", UhdmType::LogicTypespec, true,  "11111010"),
        std::make_tuple("UnaryTest_0104", vpiBitNegOp, UhdmType::LogicTypespec, true,  "11111010", UhdmType::LogicTypespec, true,  "00000101"),
        std::make_tuple("UnaryTest_0106", vpiBitNegOp, UhdmType::LogicTypespec, false, "11111011", UhdmType::LogicTypespec, false, "00000100"),

        std::make_tuple("UnaryTest_0108", vpiBitNegOp, UhdmType::TimeTypespec, false, "5",  UhdmType::TimeTypespec, false, "18446744073709551610"),
        std::make_tuple("UnaryTest_0110", vpiBitNegOp, UhdmType::TimeTypespec, false, "-5", UhdmType::TimeTypespec, false, "4"),

        //-------------------------- Reduction AND -------------------------
        std::make_tuple("UnaryTest_0111", vpiUnaryAndOp, UhdmType::IntTypespec,      true,  "-1",    UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0112", vpiUnaryAndOp, UhdmType::IntTypespec,      true,  "-5",    UhdmType::LogicTypespec, true, "0"),
        std::make_tuple("UnaryTest_0113", vpiUnaryAndOp, UhdmType::IntTypespec,      true,  "0",     UhdmType::LogicTypespec, true, "0"),
        std::make_tuple("UnaryTest_0114", vpiUnaryAndOp, UhdmType::LongIntTypespec,  true,  "-1",    UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0115", vpiUnaryAndOp, UhdmType::ShortIntTypespec, true,  "32767", UhdmType::LogicTypespec, true, "0"),
        std::make_tuple("UnaryTest_0116", vpiUnaryAndOp, UhdmType::ByteTypespec,     true,  "-1",    UhdmType::LogicTypespec, true, "1"),

        //--------------------------- Reduction OR ----------------------------
        std::make_tuple("UnaryTest_0117", vpiUnaryOrOp, UhdmType::IntTypespec,   true, "-1",       UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0118", vpiUnaryOrOp, UhdmType::IntTypespec,   true, "-5",       UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0119", vpiUnaryOrOp, UhdmType::IntTypespec,   true, "0",        UhdmType::LogicTypespec, true, "0"),
        std::make_tuple("UnaryTest_0120", vpiUnaryOrOp, UhdmType::LogicTypespec, true, "00000000", UhdmType::LogicTypespec, true, "0"),
        std::make_tuple("UnaryTest_0121", vpiUnaryOrOp, UhdmType::LogicTypespec, true, "00000101", UhdmType::LogicTypespec, true, "1"),

        //--------------------------- Reduction NAND -------------------------
        std::make_tuple("UnaryTest_0122", vpiUnaryNandOp, UhdmType::IntTypespec,      true, "-1", UhdmType::LogicTypespec, true, "0"),
        std::make_tuple("UnaryTest_0123", vpiUnaryNandOp, UhdmType::IntTypespec,      true, "-5", UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0124", vpiUnaryNandOp, UhdmType::ShortIntTypespec, true, "0",  UhdmType::LogicTypespec, true, "1"),

        //--------------------------- Reduction NOR --------------------------
        std::make_tuple("UnaryTest_0125", vpiUnaryNorOp, UhdmType::IntTypespec,   true,  "0", UhdmType::LogicTypespec, true,  "1"),
        std::make_tuple("UnaryTest_0126", vpiUnaryNorOp, UhdmType::IntTypespec,   true,  "5", UhdmType::LogicTypespec, true,  "0"),
        std::make_tuple("UnaryTest_0127", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "1"),

        //---------------------------- Reduction XOR --------------------------
        std::make_tuple("UnaryTest_0128", vpiUnaryXorOp, UhdmType::IntTypespec,      true, "-1", UhdmType::LogicTypespec, true, "0"),
        std::make_tuple("UnaryTest_0129", vpiUnaryXorOp, UhdmType::IntTypespec,      true, "-5", UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0130", vpiUnaryXorOp, UhdmType::IntTypespec,      true, "7",  UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0131", vpiUnaryXorOp, UhdmType::ShortIntTypespec, true, "5",  UhdmType::LogicTypespec, true, "0"),

        //---------------------------- Reduction XNOR --------------------------
        std::make_tuple("UnaryTest_0132", vpiUnaryXNorOp, UhdmType::IntTypespec,      true, "-1", UhdmType::LogicTypespec, true, "1"),
        std::make_tuple("UnaryTest_0133", vpiUnaryXNorOp, UhdmType::IntTypespec,      true, "-5", UhdmType::LogicTypespec, true, "0"),
        std::make_tuple("UnaryTest_0134", vpiUnaryXNorOp, UhdmType::ShortIntTypespec, true, "5",  UhdmType::LogicTypespec, true, "1"),

        //---------------------------- ++/-- Pre-Increment ------------------------
        std::make_tuple("UnaryTest_0135", vpiPreIncOp, UhdmType::IntTypespec,     true,  "10",                   UhdmType::IntTypespec,     true,  "11"),
        std::make_tuple("UnaryTest_0136", vpiPreIncOp, UhdmType::IntTypespec,     true,  "-1",                   UhdmType::IntTypespec,     true,  "0"),
        std::make_tuple("UnaryTest_0137", vpiPreIncOp, UhdmType::IntTypespec,     false, "4294967295",           UhdmType::IntTypespec,     false, "0"),
        std::make_tuple("UnaryTest_0138", vpiPreIncOp, UhdmType::LongIntTypespec, true,  "9223372036854775806",  UhdmType::LongIntTypespec, true,  "9223372036854775807"),
        std::make_tuple("UnaryTest_0139", vpiPreIncOp, UhdmType::LongIntTypespec, true,  "-1",                   UhdmType::LongIntTypespec, true,  "0"),
        std::make_tuple("UnaryTest_0140", vpiPreIncOp, UhdmType::LongIntTypespec, false, "18446744073709551615", UhdmType::LongIntTypespec, false, "0"),

        //---------------------------- ++/-- Pre-Decrement --------------------------
        std::make_tuple("UnaryTest_0141", vpiPreDecOp, UhdmType::IntTypespec,     true,  "10",                   UhdmType::IntTypespec,     true,  "9"),
        std::make_tuple("UnaryTest_0142", vpiPreDecOp, UhdmType::IntTypespec,     true,  "0",                    UhdmType::IntTypespec,     true,  "-1"),
        std::make_tuple("UnaryTest_0143", vpiPreDecOp, UhdmType::IntTypespec,     false, "0",                    UhdmType::IntTypespec,     false, "4294967295"),
        std::make_tuple("UnaryTest_0144", vpiPreDecOp, UhdmType::LongIntTypespec, true,  "0",                    UhdmType::LongIntTypespec, true,  "-1"),
        std::make_tuple("UnaryTest_0145", vpiPreDecOp, UhdmType::LongIntTypespec, true,  "-9223372036854775808", UhdmType::LongIntTypespec, true,  "9223372036854775807"), // min -> max on two's complement wrap in unsigned interpretation
        std::make_tuple("UnaryTest_0146", vpiPreDecOp, UhdmType::LongIntTypespec, false, "0",                    UhdmType::LongIntTypespec, false, "18446744073709551615"),

        //---------------------------- ++/-- Post-Increment --------------------------
        std::make_tuple("UnaryTest_0147", vpiPostIncOp, UhdmType::IntTypespec,     true,  "5",                   UhdmType::IntTypespec,     true,  "5"),
        std::make_tuple("UnaryTest_0148", vpiPostIncOp, UhdmType::IntTypespec,     true,  "-1",                  UhdmType::IntTypespec,     true,  "-1"),
        std::make_tuple("UnaryTest_0149", vpiPostIncOp, UhdmType::IntTypespec,     false,"4294967295",           UhdmType::IntTypespec,     false, "4294967295"),
        std::make_tuple("UnaryTest_0150", vpiPostIncOp, UhdmType::LongIntTypespec, true,  "-1",                  UhdmType::LongIntTypespec, true,  "-1"),
        std::make_tuple("UnaryTest_0151", vpiPostIncOp, UhdmType::LongIntTypespec, false,"18446744073709551615", UhdmType::LongIntTypespec, false, "18446744073709551615"),

        //--------------------------- ++/-- Post-Decrement ---------------------------
        std::make_tuple("UnaryTest_0152", vpiPostDecOp, UhdmType::IntTypespec,     true,  "5",  UhdmType::IntTypespec,      true,  "5"),
        std::make_tuple("UnaryTest_0153", vpiPostDecOp, UhdmType::IntTypespec,     true,  "-1", UhdmType::IntTypespec,      true,  "-1"),
        std::make_tuple("UnaryTest_0154", vpiPostDecOp, UhdmType::IntTypespec,     false, "0",   UhdmType::IntTypespec,     false, "0"),
        std::make_tuple("UnaryTest_0155", vpiPostDecOp, UhdmType::LongIntTypespec, true,  "-5", UhdmType::LongIntTypespec,  true,  "-5"),
        std::make_tuple("UnaryTest_0156", vpiPostDecOp, UhdmType::LongIntTypespec, false, "0",   UhdmType::LongIntTypespec, false, "0"),

        //---------------------------- Real / ShortReal --------------------------------
        std::make_tuple("UnaryTest_0163", vpiPreIncOp, UhdmType::RealTypespec, true,  "0.5", UhdmType::RealTypespec, true, "1.5"),
        std::make_tuple("UnaryTest_0164", vpiPreIncOp, UhdmType::RealTypespec, true, "-0.5", UhdmType::RealTypespec, true, "0.5"),
        std::make_tuple("UnaryTest_0165", vpiPreDecOp, UhdmType::RealTypespec, true,  "0.5", UhdmType::RealTypespec, true, "-0.5"),
        std::make_tuple("UnaryTest_0166", vpiPreDecOp, UhdmType::RealTypespec, true, "-0.5", UhdmType::RealTypespec, true, "-1.5"),

        // ================= Additional Unary Tests with X/Z patterns =================
        std::make_tuple("UnaryTest_0167", vpiMinusOp, UhdmType::LogicTypespec, false, "x1z0", UhdmType::LogicTypespec, false, "xxxx"),
        std::make_tuple("UnaryTest_0168", vpiMinusOp, UhdmType::LogicTypespec, false, "zzzz", UhdmType::LogicTypespec, false, "xxxx"),
        std::make_tuple("UnaryTest_0169", vpiMinusOp, UhdmType::LogicTypespec, false, "xx11", UhdmType::LogicTypespec, false, "xxxx"),

        std::make_tuple("UnaryTest_0170", vpiMinusOp, UhdmType::IntegerTypespec, false, "zx01", UhdmType::IntegerTypespec, false, "xxxx"),
        std::make_tuple("UnaryTest_0171", vpiMinusOp, UhdmType::IntegerTypespec, false, "000z", UhdmType::IntegerTypespec, false, "xxxx"),
        std::make_tuple("UnaryTest_0172", vpiMinusOp, UhdmType::IntegerTypespec, false, "111x", UhdmType::IntegerTypespec, false, "xxxx"),

        std::make_tuple("UnaryTest_0173", vpiPlusOp, UhdmType::LogicTypespec, false, "x1z0", UhdmType::LogicTypespec, false, "x1z0"),
        std::make_tuple("UnaryTest_0174", vpiPlusOp, UhdmType::LogicTypespec, false, "zzzz", UhdmType::LogicTypespec, false, "zzzz"),
        std::make_tuple("UnaryTest_0175", vpiPlusOp, UhdmType::LogicTypespec, false, "xx11", UhdmType::LogicTypespec, false, "xx11"),

        std::make_tuple("UnaryTest_0176", vpiPlusOp, UhdmType::IntegerTypespec, false, "zx01", UhdmType::IntegerTypespec, false, "zx01"),
        std::make_tuple("UnaryTest_0178", vpiPlusOp, UhdmType::IntegerTypespec, false, "000z", UhdmType::IntegerTypespec, false, "000z"),
        std::make_tuple("UnaryTest_0179", vpiPlusOp, UhdmType::IntegerTypespec, false, "111x", UhdmType::IntegerTypespec, false, "111x"),

        std::make_tuple("UnaryTest_0180", vpiNotOp, UhdmType::LogicTypespec, false, "x1z0", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0181", vpiNotOp, UhdmType::LogicTypespec, false, "zzzz", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0182", vpiNotOp, UhdmType::LogicTypespec, false, "xx11", UhdmType::LogicTypespec, false, "0"),

        std::make_tuple("UnaryTest_0183", vpiNotOp, UhdmType::IntegerTypespec, false, "zx01", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0184", vpiNotOp, UhdmType::IntegerTypespec, false, "000z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0185", vpiNotOp, UhdmType::IntegerTypespec, false, "111x", UhdmType::LogicTypespec, false, "0"),

        std::make_tuple("UnaryTest_0186", vpiBitNegOp,    UhdmType::LogicTypespec, false, "1x0z", UhdmType::LogicTypespec, false, "0x1x"),
        std::make_tuple("UnaryTest_0187", vpiNotOp,       UhdmType::LogicTypespec, false, "xxxz", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0188", vpiUnaryAndOp,  UhdmType::LogicTypespec, false, "xx11", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0189", vpiUnaryNorOp,  UhdmType::LogicTypespec, false, "00xz", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0184", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "1zx0", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0185", vpiPreIncOp,  UhdmType::LogicTypespec, false, "1xz0", UhdmType::LogicTypespec, false, "xxxx"),
        std::make_tuple("UnaryTest_0186", vpiPostIncOp, UhdmType::LogicTypespec, false, "zxzx", UhdmType::LogicTypespec, false, "zxzx"),
        std::make_tuple("UnaryTest_0187", vpiPreDecOp,  UhdmType::LogicTypespec, false, "xx0z", UhdmType::LogicTypespec, false, "xxxx"),
        std::make_tuple("UnaryTest_0188", vpiPostDecOp, UhdmType::LogicTypespec, false, "zzz1", UhdmType::LogicTypespec, false, "zzz1"),

        std::make_tuple("UnaryTest_0189", vpiBitNegOp, UhdmType::LogicTypespec, false, "x000", UhdmType::LogicTypespec, false, "x111"),
        std::make_tuple("UnaryTest_0190", vpiBitNegOp, UhdmType::LogicTypespec, false, "111x", UhdmType::LogicTypespec, false, "000x"),
        std::make_tuple("UnaryTest_0191", vpiBitNegOp, UhdmType::LogicTypespec, false, "111z", UhdmType::LogicTypespec, false, "000x"),
        std::make_tuple("UnaryTest_0192", vpiBitNegOp, UhdmType::LogicTypespec, false, "000z", UhdmType::LogicTypespec, false, "111x"),

        std::make_tuple("UnaryTest_0193", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "x000", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0194", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "111x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0195", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "111z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0196", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "000z", UhdmType::LogicTypespec, false, "0"),

        std::make_tuple("UnaryTest_0197", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "x000", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0198", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "111x", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0199", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "111z", UhdmType::LogicTypespec, false, "1"),

        std::make_tuple("UnaryTest_0200", vpiBitNegOp, UhdmType::IntegerTypespec, false, "x000",  UhdmType::IntegerTypespec, false, "x111"),
        std::make_tuple("UnaryTest_0201", vpiBitNegOp, UhdmType::IntegerTypespec, false, "111x",  UhdmType::IntegerTypespec, false, "000x"),
        std::make_tuple("UnaryTest_0202", vpiBitNegOp, UhdmType::IntegerTypespec, false, "111z",  UhdmType::IntegerTypespec, false, "000x"),
        std::make_tuple("UnaryTest_0203", vpiBitNegOp, UhdmType::IntegerTypespec, false, "000z",  UhdmType::IntegerTypespec, false, "111x"),

        std::make_tuple("UnaryTest_0204", vpiUnaryAndOp, UhdmType::IntegerTypespec, false, "x000", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0205", vpiUnaryAndOp, UhdmType::IntegerTypespec, false, "111x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0206", vpiUnaryAndOp, UhdmType::IntegerTypespec, false, "111z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0207", vpiUnaryAndOp, UhdmType::IntegerTypespec, false, "000z", UhdmType::LogicTypespec, false, "0"),

        std::make_tuple("UnaryTest_0208", vpiUnaryOrOp, UhdmType::IntegerTypespec, false, "x000", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0209", vpiUnaryOrOp, UhdmType::IntegerTypespec, false, "111x", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0210", vpiUnaryOrOp, UhdmType::IntegerTypespec, false, "111z", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0211", vpiUnaryOrOp, UhdmType::IntegerTypespec, false, "000z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0212", vpiUnaryXorOp, UhdmType::IntegerTypespec, false, "x000", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0213", vpiUnaryXorOp, UhdmType::IntegerTypespec, false, "111x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0214", vpiUnaryXorOp, UhdmType::IntegerTypespec, false, "111z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0215", vpiUnaryXorOp, UhdmType::IntegerTypespec, false, "000z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0216", vpiUnaryNandOp, UhdmType::IntegerTypespec, false, "x000", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0217", vpiUnaryNorOp,  UhdmType::IntegerTypespec, false, "111x", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0218", vpiUnaryXNorOp, UhdmType::IntegerTypespec, false, "111z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0219", vpiUnaryNandOp, UhdmType::IntegerTypespec, false, "000z", UhdmType::LogicTypespec, false, "1"),

        // 2 bit and unit test
        std::make_tuple("UnaryTest_0220", vpiNotOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0221", vpiNotOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0222", vpiNotOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0223", vpiNotOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0224", vpiNotOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0225", vpiNotOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0226", vpiNotOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0227", vpiNotOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0228", vpiNotOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0229", vpiNotOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0230", vpiNotOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0231", vpiNotOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0232", vpiNotOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0233", vpiNotOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0234", vpiNotOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0235", vpiNotOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0236", vpiBitNegOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "11"),
        std::make_tuple("UnaryTest_0237", vpiBitNegOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "10"),
        std::make_tuple("UnaryTest_0238", vpiBitNegOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "01"),
        std::make_tuple("UnaryTest_0239", vpiBitNegOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "00"),
        std::make_tuple("UnaryTest_0240", vpiBitNegOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "1x"),
        std::make_tuple("UnaryTest_0241", vpiBitNegOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "1x"),
        std::make_tuple("UnaryTest_0242", vpiBitNegOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "0x"),
        std::make_tuple("UnaryTest_0243", vpiBitNegOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "0x"),
        std::make_tuple("UnaryTest_0244", vpiBitNegOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "x1"),
        std::make_tuple("UnaryTest_0245", vpiBitNegOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "x0"),
        std::make_tuple("UnaryTest_0246", vpiBitNegOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0247", vpiBitNegOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0248", vpiBitNegOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "x1"),
        std::make_tuple("UnaryTest_0249", vpiBitNegOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "x0"),
        std::make_tuple("UnaryTest_0250", vpiBitNegOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0251", vpiBitNegOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "xx"),

        std::make_tuple("UnaryTest_0252", vpiPlusOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "00"),
        std::make_tuple("UnaryTest_0253", vpiPlusOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "01"),
        std::make_tuple("UnaryTest_0254", vpiPlusOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "10"),
        std::make_tuple("UnaryTest_0255", vpiPlusOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "11"),
        std::make_tuple("UnaryTest_0256", vpiPlusOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "0x"),
        std::make_tuple("UnaryTest_0257", vpiPlusOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "0z"),
        std::make_tuple("UnaryTest_0258", vpiPlusOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "1x"),
        std::make_tuple("UnaryTest_0259", vpiPlusOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "1z"),
        std::make_tuple("UnaryTest_0260", vpiPlusOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "x0"),
        std::make_tuple("UnaryTest_0261", vpiPlusOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "x1"),
        std::make_tuple("UnaryTest_0262", vpiPlusOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0263", vpiPlusOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "xz"),
        std::make_tuple("UnaryTest_0264", vpiPlusOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "z0"),
        std::make_tuple("UnaryTest_0265", vpiPlusOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "z1"),
        std::make_tuple("UnaryTest_0266", vpiPlusOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "zx"),
        std::make_tuple("UnaryTest_0267", vpiPlusOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "zz"),

        std::make_tuple("UnaryTest_0268", vpiMinusOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "00"),
        std::make_tuple("UnaryTest_0269", vpiMinusOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "11"),
        std::make_tuple("UnaryTest_0270", vpiMinusOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "10"),
        std::make_tuple("UnaryTest_0271", vpiMinusOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "01"),
        std::make_tuple("UnaryTest_0272", vpiMinusOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0273", vpiMinusOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0274", vpiMinusOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0275", vpiMinusOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0276", vpiMinusOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0277", vpiMinusOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0278", vpiMinusOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0279", vpiMinusOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0280", vpiMinusOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0281", vpiMinusOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0282", vpiMinusOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0283", vpiMinusOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "xx"),

        std::make_tuple("UnaryTest_0284", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0285", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0286", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0287", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0288", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0289", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0290", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0291", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0292", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0293", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0294", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0295", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0296", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0297", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0298", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0299", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0300", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0301", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0302", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0303", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0304", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0305", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0306", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0307", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0308", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0309", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0310", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0311", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0312", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0313", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0314", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0315", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0316", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0317", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0318", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0319", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0320", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0321", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0322", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0323", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0324", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0325", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0326", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0327", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0328", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0329", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0330", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0331", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0332", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0333", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0334", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0335", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0336", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0337", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0338", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0339", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0340", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0341", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0342", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0343", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0344", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0345", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0346", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0347", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0348", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0349", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0350", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0351", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0352", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0353", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0354", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0355", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0356", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0357", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0358", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0359", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0360", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0361", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0362", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0363", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0364", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0365", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0366", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0367", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0368", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0369", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0370", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0371", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0372", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0373", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0374", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0375", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0376", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0377", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0378", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0379", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0380", vpiPreIncOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "01"),
        std::make_tuple("UnaryTest_0381", vpiPreIncOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "10"),
        std::make_tuple("UnaryTest_0382", vpiPreIncOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "11"),
        std::make_tuple("UnaryTest_0383", vpiPreIncOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "00"),
        std::make_tuple("UnaryTest_0384", vpiPreIncOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0385", vpiPreIncOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0386", vpiPreIncOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0387", vpiPreIncOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0388", vpiPreIncOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0389", vpiPreIncOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0390", vpiPreIncOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0391", vpiPreIncOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0392", vpiPreIncOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0393", vpiPreIncOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0394", vpiPreIncOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0395", vpiPreIncOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "xx"),

        std::make_tuple("UnaryTest_0396", vpiPostIncOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "00"),
        std::make_tuple("UnaryTest_0397", vpiPostIncOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "01"),
        std::make_tuple("UnaryTest_0398", vpiPostIncOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "10"),
        std::make_tuple("UnaryTest_0399", vpiPostIncOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "11"),
        std::make_tuple("UnaryTest_0400", vpiPostIncOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "0x"),
        std::make_tuple("UnaryTest_0401", vpiPostIncOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "0z"),
        std::make_tuple("UnaryTest_0402", vpiPostIncOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "1x"),
        std::make_tuple("UnaryTest_0403", vpiPostIncOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "1z"),
        std::make_tuple("UnaryTest_0404", vpiPostIncOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "x0"),
        std::make_tuple("UnaryTest_0405", vpiPostIncOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "x1"),
        std::make_tuple("UnaryTest_0406", vpiPostIncOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0407", vpiPostIncOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "xz"),
        std::make_tuple("UnaryTest_0408", vpiPostIncOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "z0"),
        std::make_tuple("UnaryTest_0409", vpiPostIncOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "z1"),
        std::make_tuple("UnaryTest_0410", vpiPostIncOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "zx"),
        std::make_tuple("UnaryTest_0411", vpiPostIncOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "zz"),

        std::make_tuple("UnaryTest_0412", vpiPreDecOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "11"),
        std::make_tuple("UnaryTest_0413", vpiPreDecOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "00"),
        std::make_tuple("UnaryTest_0414", vpiPreDecOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "01"),
        std::make_tuple("UnaryTest_0415", vpiPreDecOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "10"),
        std::make_tuple("UnaryTest_0416", vpiPreDecOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0417", vpiPreDecOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0418", vpiPreDecOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0419", vpiPreDecOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0420", vpiPreDecOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0421", vpiPreDecOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0422", vpiPreDecOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0423", vpiPreDecOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0424", vpiPreDecOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0425", vpiPreDecOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0426", vpiPreDecOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0427", vpiPreDecOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "xx"),

        std::make_tuple("UnaryTest_0428", vpiPostDecOp, UhdmType::LogicTypespec, false, "00", UhdmType::LogicTypespec, false, "00"),
        std::make_tuple("UnaryTest_0429", vpiPostDecOp, UhdmType::LogicTypespec, false, "01", UhdmType::LogicTypespec, false, "01"),
        std::make_tuple("UnaryTest_0430", vpiPostDecOp, UhdmType::LogicTypespec, false, "10", UhdmType::LogicTypespec, false, "10"),
        std::make_tuple("UnaryTest_0431", vpiPostDecOp, UhdmType::LogicTypespec, false, "11", UhdmType::LogicTypespec, false, "11"),
        std::make_tuple("UnaryTest_0432", vpiPostDecOp, UhdmType::LogicTypespec, false, "0x", UhdmType::LogicTypespec, false, "0x"),
        std::make_tuple("UnaryTest_0433", vpiPostDecOp, UhdmType::LogicTypespec, false, "0z", UhdmType::LogicTypespec, false, "0z"),
        std::make_tuple("UnaryTest_0434", vpiPostDecOp, UhdmType::LogicTypespec, false, "1x", UhdmType::LogicTypespec, false, "1x"),
        std::make_tuple("UnaryTest_0435", vpiPostDecOp, UhdmType::LogicTypespec, false, "1z", UhdmType::LogicTypespec, false, "1z"),
        std::make_tuple("UnaryTest_0436", vpiPostDecOp, UhdmType::LogicTypespec, false, "x0", UhdmType::LogicTypespec, false, "x0"),
        std::make_tuple("UnaryTest_0437", vpiPostDecOp, UhdmType::LogicTypespec, false, "x1", UhdmType::LogicTypespec, false, "x1"),
        std::make_tuple("UnaryTest_0438", vpiPostDecOp, UhdmType::LogicTypespec, false, "xx", UhdmType::LogicTypespec, false, "xx"),
        std::make_tuple("UnaryTest_0439", vpiPostDecOp, UhdmType::LogicTypespec, false, "xz", UhdmType::LogicTypespec, false, "xz"),
        std::make_tuple("UnaryTest_0440", vpiPostDecOp, UhdmType::LogicTypespec, false, "z0", UhdmType::LogicTypespec, false, "z0"),
        std::make_tuple("UnaryTest_0441", vpiPostDecOp, UhdmType::LogicTypespec, false, "z1", UhdmType::LogicTypespec, false, "z1"),
        std::make_tuple("UnaryTest_0442", vpiPostDecOp, UhdmType::LogicTypespec, false, "zx", UhdmType::LogicTypespec, false, "zx"),
        std::make_tuple("UnaryTest_0443", vpiPostDecOp, UhdmType::LogicTypespec, false, "zz", UhdmType::LogicTypespec, false, "zz"),

        //1 bit unit testcases
        std::make_tuple("UnaryTest_0444", vpiNotOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0445", vpiNotOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0446", vpiNotOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0447", vpiNotOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0448", vpiBitNegOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0449", vpiBitNegOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0450", vpiBitNegOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0451", vpiBitNegOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0452", vpiPlusOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0453", vpiPlusOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0454", vpiPlusOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0455", vpiPlusOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "z"),

        std::make_tuple("UnaryTest_0456", vpiMinusOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0457", vpiMinusOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0458", vpiMinusOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0459", vpiMinusOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0460", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0461", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0462", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0463", vpiUnaryAndOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0464", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0465", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0466", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0467", vpiUnaryNandOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0468", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0469", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0470", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0471", vpiUnaryOrOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0472", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0473", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0474", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0475", vpiUnaryNorOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0476", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0477", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0478", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0479", vpiUnaryXorOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0480", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0481", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0482", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0483", vpiUnaryXNorOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0484", vpiPreIncOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0485", vpiPreIncOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0486", vpiPreIncOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0487", vpiPreIncOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0488", vpiPostIncOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0489", vpiPostIncOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0490", vpiPostIncOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0491", vpiPostIncOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "z"),

        std::make_tuple("UnaryTest_0492", vpiPreDecOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0493", vpiPreDecOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0494", vpiPreDecOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0495", vpiPreDecOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "x"),

        std::make_tuple("UnaryTest_0496", vpiPostDecOp, UhdmType::LogicTypespec, false, "0", UhdmType::LogicTypespec, false, "0"),
        std::make_tuple("UnaryTest_0497", vpiPostDecOp, UhdmType::LogicTypespec, false, "1", UhdmType::LogicTypespec, false, "1"),
        std::make_tuple("UnaryTest_0498", vpiPostDecOp, UhdmType::LogicTypespec, false, "x", UhdmType::LogicTypespec, false, "x"),
        std::make_tuple("UnaryTest_0499", vpiPostDecOp, UhdmType::LogicTypespec, false, "z", UhdmType::LogicTypespec, false, "z")
        // clang-format on
        ));

INSTANTIATE_TEST_SUITE_P(
    PromationLogicBinary, BinaryOperationTest,
    testing::Values(
        // clang-format off
        std::make_tuple("BinaryTest_0001", vpiAddOp, UhdmType::IntTypespec, true, vpiIntConst, "123456789", UhdmType::IntTypespec,       true, vpiIntConst,    "123456789",     UhdmType::IntTypespec,       true, vpiIntConst,    "246913578"),
        std::make_tuple("BinaryTest_0002", vpiAddOp, UhdmType::IntTypespec, true, vpiIntConst, "123456789", UhdmType::LongIntTypespec,   true, vpiIntConst,    "9876543210123", UhdmType::LongIntTypespec,   true, vpiIntConst,    "9876666666912"),
        std::make_tuple("BinaryTest_0003", vpiAddOp, UhdmType::IntTypespec, true, vpiIntConst, "123456789", UhdmType::ShortIntTypespec,  true, vpiIntConst,    "12345",         UhdmType::IntTypespec,       true, vpiIntConst,    "123469134"),
        std::make_tuple("BinaryTest_0004", vpiAddOp, UhdmType::IntTypespec, true, vpiIntConst, "123456789", UhdmType::RealTypespec,      true, vpiRealConst,   "12345.1",       UhdmType::RealTypespec,      true, vpiIntConst,    "123469134.1"),
        std::make_tuple("BinaryTest_0005", vpiAddOp, UhdmType::IntTypespec, true, vpiIntConst, "123456",    UhdmType::ShortRealTypespec, true, vpiRealConst,   "1.5",           UhdmType::ShortRealTypespec, true, vpiRealConst,   "123457.5"),
        std::make_tuple("BinaryTest_0006", vpiAddOp, UhdmType::IntTypespec, true, vpiIntConst, "123456789", UhdmType::LogicTypespec,     true, vpiBinaryConst, "1",             UhdmType::LogicTypespec,     true, vpiBinaryConst, "00000111010110111100110100010110"),
        std::make_tuple("BinaryTest_0007", vpiAddOp, UhdmType::IntTypespec, true, vpiIntConst, "1",         UhdmType::IntegerTypespec,   true, vpiBinaryConst, "1",             UhdmType::IntegerTypespec,   true, vpiIntConst,    "00000000000000000000000000000010"),
        std::make_tuple("BinaryTest_0008", vpiAddOp, UhdmType::IntTypespec, true, vpiIntConst, "123456789", UhdmType::ByteTypespec,      true, vpiIntConst,    "100",           UhdmType::IntTypespec,       true, vpiIntConst,    "123456889"),

        std::make_tuple("BinaryTest_0009", vpiAddOp, UhdmType::LongIntTypespec, true, vpiIntConst, "9876543210123", UhdmType::IntTypespec,       true, vpiIntConst,    "123456789",     UhdmType::LongIntTypespec,   true, vpiIntConst,    "9876666666912"),
        std::make_tuple("BinaryTest_0010", vpiAddOp, UhdmType::LongIntTypespec, true, vpiIntConst, "9876543210123", UhdmType::LongIntTypespec,   true, vpiIntConst,    "9876543210123", UhdmType::LongIntTypespec,   true, vpiIntConst,    "19753086420246"),
        std::make_tuple("BinaryTest_0011", vpiAddOp, UhdmType::LongIntTypespec, true, vpiIntConst, "9876543210123", UhdmType::ShortIntTypespec,  true, vpiIntConst,    "12345",         UhdmType::LongIntTypespec,   true, vpiIntConst,    "9876543222468"),
        std::make_tuple("BinaryTest_0012", vpiAddOp, UhdmType::LongIntTypespec, true, vpiIntConst, "9876543210123", UhdmType::RealTypespec,      true, vpiRealConst,   "12345.0",       UhdmType::RealTypespec,      true, vpiRealConst,   "9876543222468"),
        std::make_tuple("BinaryTest_0013", vpiAddOp, UhdmType::LongIntTypespec, true, vpiIntConst, "987654",        UhdmType::ShortRealTypespec, true, vpiRealConst,   "1.5",           UhdmType::ShortRealTypespec, true, vpiRealConst,   "987655.5"),
        std::make_tuple("BinaryTest_0014", vpiAddOp, UhdmType::LongIntTypespec, true, vpiIntConst, "9876543210123", UhdmType::LogicTypespec,     true, vpiBinaryConst, "1",             UhdmType::LogicTypespec,     true, vpiBinaryConst, "0000000000000000000010001111101110001111110110011000001010001100"),
        std::make_tuple("BinaryTest_0015", vpiAddOp, UhdmType::LongIntTypespec, true, vpiIntConst, "10",            UhdmType::IntegerTypespec,   true, vpiBinaryConst, "1",             UhdmType::IntegerTypespec,   true, vpiBinaryConst, "0000000000000000000000000000000000000000000000000000000000001011"),
        std::make_tuple("BinaryTest_0016", vpiAddOp, UhdmType::LongIntTypespec, true, vpiIntConst, "9876543210123", UhdmType::ByteTypespec,      true, vpiIntConst,    "100",           UhdmType::LongIntTypespec,   true, vpiIntConst,    "9876543210223"),

        std::make_tuple("BinaryTest_0017", vpiAddOp, UhdmType::ShortIntTypespec, true, vpiIntConst, "12345", UhdmType::IntTypespec,       true, vpiIntConst,    "123456789",     UhdmType::IntTypespec,       true, vpiIntConst,    "123469134"),
        std::make_tuple("BinaryTest_0018", vpiAddOp, UhdmType::ShortIntTypespec, true, vpiIntConst, "12345", UhdmType::LongIntTypespec,   true, vpiIntConst,    "9876543210123", UhdmType::LongIntTypespec,   true, vpiIntConst,    "9876543222468"),
        std::make_tuple("BinaryTest_0019", vpiAddOp, UhdmType::ShortIntTypespec, true, vpiIntConst, "12345", UhdmType::ShortIntTypespec,  true, vpiIntConst,    "12345",         UhdmType::ShortIntTypespec,  true, vpiIntConst,    "24690"),
        std::make_tuple("BinaryTest_0020", vpiAddOp, UhdmType::ShortIntTypespec, true, vpiIntConst, "12345", UhdmType::RealTypespec,      true, vpiRealConst,   "12345.0",       UhdmType::RealTypespec,      true, vpiRealConst,   "24690"),
        std::make_tuple("BinaryTest_0021", vpiAddOp, UhdmType::ShortIntTypespec, true, vpiIntConst, "1",     UhdmType::ShortRealTypespec, true, vpiRealConst,   "1.5",           UhdmType::ShortRealTypespec, true, vpiRealConst,   "2.5"),
        std::make_tuple("BinaryTest_0022", vpiAddOp, UhdmType::ShortIntTypespec, true, vpiIntConst, "1",     UhdmType::LogicTypespec,     true, vpiBinaryConst, "1",             UhdmType::LogicTypespec,     true, vpiBinaryConst, "0000000000000010"),
        std::make_tuple("BinaryTest_0023", vpiAddOp, UhdmType::ShortIntTypespec, true, vpiIntConst, "1",     UhdmType::IntegerTypespec,   true, vpiBinaryConst, "1",             UhdmType::IntegerTypespec,   true, vpiBinaryConst, "0000000000000010"),
        std::make_tuple("BinaryTest_0024", vpiAddOp, UhdmType::ShortIntTypespec, true, vpiIntConst, "12345", UhdmType::ByteTypespec,      true, vpiIntConst,    "100",           UhdmType::ShortIntTypespec,  true, vpiIntConst,    "12445"),

        std::make_tuple("BinaryTest_0025", vpiAddOp, UhdmType::RealTypespec, true, vpiRealConst, "1.5", UhdmType::IntTypespec,       true, vpiIntConst,    "1",   UhdmType::RealTypespec, true, vpiRealConst, "2.5"),
        std::make_tuple("BinaryTest_0026", vpiAddOp, UhdmType::RealTypespec, true, vpiRealConst, "1.5", UhdmType::LongIntTypespec,   true, vpiIntConst,    "1",   UhdmType::RealTypespec, true, vpiIntConst,  "2.5"),
        std::make_tuple("BinaryTest_0027", vpiAddOp, UhdmType::RealTypespec, true, vpiRealConst, "1.5", UhdmType::ShortIntTypespec,  true, vpiIntConst,    "1",   UhdmType::RealTypespec, true, vpiIntConst,  "2.5"),
        std::make_tuple("BinaryTest_0028", vpiAddOp, UhdmType::RealTypespec, true, vpiRealConst, "1.5", UhdmType::RealTypespec,      true, vpiRealConst,   "1.0", UhdmType::RealTypespec, true, vpiRealConst, "2.5"),
        std::make_tuple("BinaryTest_0029", vpiAddOp, UhdmType::RealTypespec, true, vpiRealConst, "1.5", UhdmType::ShortRealTypespec, true, vpiRealConst,   "1",   UhdmType::RealTypespec, true, vpiRealConst, "2.5"),
        std::make_tuple("BinaryTest_0030", vpiAddOp, UhdmType::RealTypespec, true, vpiRealConst, "1.5", UhdmType::LogicTypespec,     true, vpiBinaryConst, "1",   UhdmType::RealTypespec, true, vpiRealConst, "2.5"),
        std::make_tuple("BinaryTest_0031", vpiAddOp, UhdmType::RealTypespec, true, vpiRealConst, "1.5", UhdmType::IntegerTypespec,   true, vpiBinaryConst, "1",   UhdmType::RealTypespec, true, vpiRealConst, "2.5"),
        std::make_tuple("BinaryTest_0032", vpiAddOp, UhdmType::RealTypespec, true, vpiRealConst, "1.5", UhdmType::ByteTypespec,      true, vpiIntConst,    "1",   UhdmType::RealTypespec, true, vpiRealConst, "2.5"),

        std::make_tuple("BinaryTest_0033", vpiAddOp, UhdmType::ShortRealTypespec, true, vpiRealConst, "1.5", UhdmType::IntTypespec,       true, vpiIntConst,    "1", UhdmType::ShortRealTypespec, true, vpiRealConst, "2.5"),
        std::make_tuple("BinaryTest_0034", vpiAddOp, UhdmType::ShortRealTypespec, true, vpiRealConst, "1.5", UhdmType::LongIntTypespec,   true, vpiIntConst,    "1", UhdmType::ShortRealTypespec, true, vpiRealConst, "2.5"),
        std::make_tuple("BinaryTest_0035", vpiAddOp, UhdmType::ShortRealTypespec, true, vpiRealConst, "1.5", UhdmType::ShortIntTypespec,  true, vpiIntConst,    "1", UhdmType::ShortRealTypespec, true, vpiRealConst, "2.5"),
        std::make_tuple("BinaryTest_0036", vpiAddOp, UhdmType::ShortRealTypespec, true, vpiRealConst, "1.5", UhdmType::RealTypespec,      true, vpiRealConst,   "1", UhdmType::RealTypespec,      true, vpiRealConst, "2.5"),
        std::make_tuple("BinaryTest_0037", vpiAddOp, UhdmType::ShortRealTypespec, true, vpiRealConst, "1.5", UhdmType::ShortRealTypespec, true, vpiRealConst,   "1", UhdmType::ShortRealTypespec, true, vpiRealConst, "2.5"),
        std::make_tuple("BinaryTest_0038", vpiAddOp, UhdmType::ShortRealTypespec, true, vpiRealConst, "1.5", UhdmType::LogicTypespec,     true, vpiBinaryConst, "1", UhdmType::ShortRealTypespec, true, vpiRealConst, "2.5"),
        std::make_tuple("BinaryTest_0039", vpiAddOp, UhdmType::ShortRealTypespec, true, vpiRealConst, "1.5", UhdmType::IntegerTypespec,   true, vpiBinaryConst, "1", UhdmType::ShortRealTypespec, true, vpiRealConst, "2.5"),
        std::make_tuple("BinaryTest_0040", vpiAddOp, UhdmType::ShortRealTypespec, true, vpiRealConst, "1.5", UhdmType::ByteTypespec,      true, vpiIntConst,    "1", UhdmType::ShortRealTypespec, true, vpiRealConst, "2.5"),

        std::make_tuple("BinaryTest_0041", vpiAddOp, UhdmType::LogicTypespec, true, vpiBinaryConst, "1", UhdmType::IntTypespec,       true, vpiIntConst,    "1",   UhdmType::LogicTypespec,     true, vpiBinaryConst, "00000000000000000000000000000010"),
        std::make_tuple("BinaryTest_0042", vpiAddOp, UhdmType::LogicTypespec, true, vpiBinaryConst, "1", UhdmType::LongIntTypespec,   true, vpiIntConst,    "1",   UhdmType::LogicTypespec,     true, vpiBinaryConst, "0000000000000000000000000000000000000000000000000000000000000010"),
        std::make_tuple("BinaryTest_0043", vpiAddOp, UhdmType::LogicTypespec, true, vpiBinaryConst, "1", UhdmType::ShortIntTypespec,  true, vpiIntConst,    "1",   UhdmType::LogicTypespec,     true, vpiBinaryConst, "0000000000000010"),
        std::make_tuple("BinaryTest_0044", vpiAddOp, UhdmType::LogicTypespec, true, vpiBinaryConst, "1", UhdmType::RealTypespec,      true, vpiRealConst,   "1.5", UhdmType::RealTypespec,      true, vpiRealConst,   "2.5"),
        std::make_tuple("BinaryTest_0045", vpiAddOp, UhdmType::LogicTypespec, true, vpiBinaryConst, "1", UhdmType::ShortRealTypespec, true, vpiRealConst,   "1.5", UhdmType::ShortRealTypespec, true, vpiRealConst,   "2.5"),
        std::make_tuple("BinaryTest_0046", vpiAddOp, UhdmType::LogicTypespec, true, vpiBinaryConst, "1", UhdmType::LogicTypespec,     true, vpiBinaryConst, "1",   UhdmType::LogicTypespec,     true, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0047", vpiAddOp, UhdmType::LogicTypespec, true, vpiBinaryConst, "1", UhdmType::IntegerTypespec,   true, vpiBinaryConst, "1",   UhdmType::LogicTypespec,     true, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0048", vpiAddOp, UhdmType::LogicTypespec, true, vpiBinaryConst, "1", UhdmType::ByteTypespec,      true, vpiIntConst,    "1",   UhdmType::LogicTypespec,     true, vpiBinaryConst, "00000010"),

        std::make_tuple("BinaryTest_0049", vpiAddOp, UhdmType::IntegerTypespec, true, vpiBinaryConst, "1", UhdmType::IntTypespec,       true, vpiIntConst,    "1",   UhdmType::IntegerTypespec,   true, vpiBinaryConst, "00000000000000000000000000000010"),
        std::make_tuple("BinaryTest_0050", vpiAddOp, UhdmType::IntegerTypespec, true, vpiBinaryConst, "1", UhdmType::LongIntTypespec,   true, vpiIntConst,    "1",   UhdmType::IntegerTypespec,   true, vpiBinaryConst, "0000000000000000000000000000000000000000000000000000000000000010"),
        std::make_tuple("BinaryTest_0051", vpiAddOp, UhdmType::IntegerTypespec, true, vpiBinaryConst, "1", UhdmType::ShortIntTypespec,  true, vpiIntConst,    "1",   UhdmType::IntegerTypespec,   true, vpiBinaryConst, "0000000000000010"),
        std::make_tuple("BinaryTest_0052", vpiAddOp, UhdmType::IntegerTypespec, true, vpiBinaryConst, "1", UhdmType::RealTypespec,      true, vpiRealConst,   "1.5", UhdmType::RealTypespec,      true, vpiRealConst,   "2.5"),
        std::make_tuple("BinaryTest_0053", vpiAddOp, UhdmType::IntegerTypespec, true, vpiBinaryConst, "1", UhdmType::ShortRealTypespec, true, vpiRealConst,   "1.5", UhdmType::ShortRealTypespec, true, vpiRealConst,   "2.5"),
        std::make_tuple("BinaryTest_0054", vpiAddOp, UhdmType::IntegerTypespec, true, vpiBinaryConst, "1", UhdmType::LogicTypespec,     true, vpiBinaryConst, "1",   UhdmType::LogicTypespec,     true, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0055", vpiAddOp, UhdmType::IntegerTypespec, true, vpiBinaryConst, "1", UhdmType::IntegerTypespec,   true, vpiBinaryConst, "1",   UhdmType::IntegerTypespec,   true, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0056", vpiAddOp, UhdmType::IntegerTypespec, true, vpiBinaryConst, "1", UhdmType::ByteTypespec,      true, vpiIntConst,    "1",   UhdmType::IntegerTypespec,   true, vpiBinaryConst, "00000010"),

        std::make_tuple("BinaryTest_0057", vpiAddOp, UhdmType::ByteTypespec, true, vpiIntConst, "100", UhdmType::IntTypespec,       true, vpiIntConst,    "123456789",     UhdmType::IntTypespec,       true, vpiIntConst,    "123456889"),
        std::make_tuple("BinaryTest_0058", vpiAddOp, UhdmType::ByteTypespec, true, vpiIntConst, "100", UhdmType::LongIntTypespec,   true, vpiIntConst,    "9876543210123", UhdmType::LongIntTypespec,   true, vpiIntConst,    "9876543210223"),
        std::make_tuple("BinaryTest_0059", vpiAddOp, UhdmType::ByteTypespec, true, vpiIntConst, "100", UhdmType::ShortIntTypespec,  true, vpiIntConst,    "12345",         UhdmType::ShortIntTypespec,  true, vpiIntConst,    "12445"),
        std::make_tuple("BinaryTest_0060", vpiAddOp, UhdmType::ByteTypespec, true, vpiIntConst, "1",   UhdmType::RealTypespec,      true, vpiRealConst,   "1.5",           UhdmType::RealTypespec,      true, vpiRealConst,   "2.5"),
        std::make_tuple("BinaryTest_0061", vpiAddOp, UhdmType::ByteTypespec, true, vpiIntConst, "1",   UhdmType::ShortRealTypespec, true, vpiRealConst,   "1.5",           UhdmType::ShortRealTypespec, true, vpiRealConst,   "2.5"),
        std::make_tuple("BinaryTest_0062", vpiAddOp, UhdmType::ByteTypespec, true, vpiIntConst, "1",   UhdmType::LogicTypespec,     true, vpiBinaryConst, "1",             UhdmType::LogicTypespec,     true, vpiBinaryConst, "00000010"),
        std::make_tuple("BinaryTest_0063", vpiAddOp, UhdmType::ByteTypespec, true, vpiIntConst, "1",   UhdmType::IntegerTypespec,   true, vpiBinaryConst, "1",             UhdmType::IntegerTypespec,   true, vpiBinaryConst, "00000010"),
        std::make_tuple("BinaryTest_0064", vpiAddOp, UhdmType::ByteTypespec, true, vpiIntConst, "1",   UhdmType::ByteTypespec,      true, vpiIntConst,    "1",             UhdmType::ByteTypespec,      true, vpiIntConst,    "2")
        // clang-format on
        ));

INSTANTIATE_TEST_SUITE_P(
    BinaryOperators, BinaryOperationTest,
    testing::Values(
        // clang-format off
        std::make_tuple("BinaryTest_0001", vpiAddOp, UhdmType::IntTypespec, true,  vpiIntConst,  "10",  UhdmType::IntTypespec, true,  vpiIntConst,  "20", UhdmType::IntTypespec, true,  vpiIntConst,  "30"),
        std::make_tuple("BinaryTest_0002", vpiAddOp, UhdmType::IntTypespec, false, vpiUIntConst, "10",  UhdmType::IntTypespec, false, vpiUIntConst, "20", UhdmType::IntTypespec, false, vpiUIntConst, "30"),
        std::make_tuple("BinaryTest_0003", vpiAddOp, UhdmType::IntTypespec, true,  vpiIntConst,  "10",  UhdmType::IntTypespec, true,  vpiIntConst,  "20", UhdmType::IntTypespec, true,  vpiIntConst,  "30"),
        std::make_tuple("BinaryTest_0004", vpiAddOp, UhdmType::IntTypespec, true,  vpiIntConst,  "-20", UhdmType::IntTypespec, true,  vpiIntConst,  "10", UhdmType::IntTypespec, true,  vpiIntConst,  "-10"),

        std::make_tuple("BinaryTest_0005", vpiAddOp, UhdmType::IntTypespec, true, vpiIntConst, "50", UhdmType::IntTypespec, true, vpiIntConst, "-25", UhdmType::IntTypespec, true, vpiIntConst, "25"),
        std::make_tuple("BinaryTest_0006", vpiAddOp, UhdmType::IntTypespec, true, vpiIntConst, "-5", UhdmType::IntTypespec, true, vpiIntConst, "3",   UhdmType::IntTypespec, true, vpiIntConst, "-2"),

        std::make_tuple("BinaryTest_0007", vpiAddOp, UhdmType::LongIntTypespec,  true,  vpiIntConst,  "2147483647", UhdmType::LongIntTypespec,  true,  vpiIntConst,  "1",  UhdmType::LongIntTypespec,  true,  vpiIntConst,  "2147483648"),
        std::make_tuple("BinaryTest_0008", vpiAddOp, UhdmType::LongIntTypespec,  false, vpiUIntConst, "4294967295", UhdmType::LongIntTypespec,  false, vpiUIntConst, "1",  UhdmType::LongIntTypespec,  false, vpiUIntConst, "4294967296"),
        std::make_tuple("BinaryTest_0009", vpiAddOp, UhdmType::ShortIntTypespec, true,  vpiIntConst,  "100",        UhdmType::ShortIntTypespec, true,  vpiIntConst,  "50", UhdmType::ShortIntTypespec, true,  vpiIntConst,  "150"),

        std::make_tuple("BinaryTest_0010", vpiAddOp, UhdmType::ByteTypespec,      true,  vpiIntConst,    "4",        UhdmType::ByteTypespec,      true,  vpiIntConst,    "8",         UhdmType::ByteTypespec,      true,  vpiIntConst,    "12"),
        std::make_tuple("BinaryTest_0012", vpiAddOp, UhdmType::RealTypespec,      true,  vpiRealConst,   "3.14",     UhdmType::RealTypespec,      true,  vpiRealConst,   "2.0",       UhdmType::RealTypespec,      true,  vpiRealConst,   "5.140000000000001"),
        std::make_tuple("BinaryTest_0013", vpiAddOp, UhdmType::ShortRealTypespec, true,  vpiRealConst,   "-1.5",     UhdmType::ShortRealTypespec, true,  vpiRealConst,   "0.75",      UhdmType::ShortRealTypespec, true,  vpiRealConst,   "-0.75"),
        std::make_tuple("BinaryTest_0014", vpiAddOp, UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "01100100", UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "011001000", UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "100101100"),
        std::make_tuple("BinaryTest_0015", vpiAddOp, UhdmType::TimeTypespec,      false, vpiIntConst,    "100",      UhdmType::TimeTypespec,      false, vpiIntConst,    "50",        UhdmType::TimeTypespec,      false, vpiIntConst,    "150"),

        // Binary Sub
        std::make_tuple("BinaryTest_0016", vpiSubOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "20",         UhdmType::IntTypespec,       true,  vpiIntConst,    "10",       UhdmType::IntTypespec,       true,  vpiIntConst,    "10"),
        std::make_tuple("BinaryTest_0017", vpiSubOp, UhdmType::IntTypespec,       false, vpiUIntConst,   "30",         UhdmType::IntTypespec,       false, vpiUIntConst,   "10",       UhdmType::IntTypespec,       false, vpiUIntConst,   "20"),
        std::make_tuple("BinaryTest_0018", vpiSubOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "-10",        UhdmType::IntTypespec,       true,  vpiIntConst,    "20",       UhdmType::IntTypespec,       true,  vpiIntConst,    "-30"),
        std::make_tuple("BinaryTest_0019", vpiSubOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "-50",        UhdmType::IntTypespec,       true,  vpiIntConst,    "-25",      UhdmType::IntTypespec,       true,  vpiIntConst,    "-25"),
        std::make_tuple("BinaryTest_0020", vpiSubOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "5",          UhdmType::IntTypespec,       true,  vpiIntConst,    "-3",       UhdmType::IntTypespec,       true,  vpiIntConst,    "8"),
        std::make_tuple("BinaryTest_0021", vpiSubOp, UhdmType::LongIntTypespec,   true,  vpiIntConst,    "2147483647", UhdmType::LongIntTypespec,   true,  vpiIntConst,    "1",        UhdmType::LongIntTypespec,   true,  vpiIntConst,    "2147483646"),
        std::make_tuple("BinaryTest_0022", vpiSubOp, UhdmType::LongIntTypespec,   false, vpiUIntConst,   "4294967295", UhdmType::LongIntTypespec,   false, vpiUIntConst,   "1",        UhdmType::LongIntTypespec,   false, vpiUIntConst,   "4294967294"),
        std::make_tuple("BinaryTest_0023", vpiSubOp, UhdmType::ShortIntTypespec,  true,  vpiIntConst,    "100",        UhdmType::ShortIntTypespec,  true,  vpiIntConst,    "50",       UhdmType::ShortIntTypespec,  true,  vpiIntConst,    "50"),
        std::make_tuple("BinaryTest_0024", vpiSubOp, UhdmType::ByteTypespec,      true,  vpiIntConst,    "9",          UhdmType::ByteTypespec,      true,  vpiIntConst,    "4",        UhdmType::ByteTypespec,      true,  vpiIntConst,    "5"),
        std::make_tuple("BinaryTest_0026", vpiSubOp, UhdmType::RealTypespec,      true,  vpiRealConst,   "5.14",       UhdmType::RealTypespec,      true,  vpiRealConst,   "2.0",      UhdmType::RealTypespec,      true,  vpiRealConst,   "3.14"),
        std::make_tuple("BinaryTest_0027", vpiSubOp, UhdmType::ShortRealTypespec, true,  vpiRealConst,   "-1.5",       UhdmType::ShortRealTypespec, true,  vpiRealConst,   "0.75",     UhdmType::ShortRealTypespec, true,  vpiRealConst,   "-2.25"),
        std::make_tuple("BinaryTest_0028", vpiSubOp, UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "11001000",   UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "01100100", UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "01100100"),
        std::make_tuple("BinaryTest_0029", vpiSubOp, UhdmType::TimeTypespec,      false, vpiIntConst,    "1000",       UhdmType::TimeTypespec,      false, vpiIntConst,    "250",      UhdmType::TimeTypespec,      false, vpiIntConst,    "750"),

        // Binary Div
        std::make_tuple("BinaryTest_0030", vpiDivOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "20",         UhdmType::IntTypespec,       true,  vpiIntConst,    "10",       UhdmType::IntTypespec,       true,  vpiIntConst,    "2"),
        std::make_tuple("BinaryTest_0031", vpiDivOp, UhdmType::IntTypespec,       false, vpiUIntConst,   "40",         UhdmType::IntTypespec,       false, vpiUIntConst,   "5",        UhdmType::IntTypespec,       false, vpiUIntConst,   "8"),
        std::make_tuple("BinaryTest_0032", vpiDivOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "-30",        UhdmType::IntTypespec,       true,  vpiIntConst,    "10",       UhdmType::IntTypespec,       true,  vpiIntConst,    "-3"),
        std::make_tuple("BinaryTest_0033", vpiDivOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "-50",        UhdmType::IntTypespec,       true,  vpiIntConst,    "-5",       UhdmType::IntTypespec,       true,  vpiIntConst,    "10"),
        std::make_tuple("BinaryTest_0034", vpiDivOp, UhdmType::LongIntTypespec,   true,  vpiIntConst,    "2147483640", UhdmType::LongIntTypespec,   true,  vpiIntConst,    "10",       UhdmType::LongIntTypespec,   true,  vpiIntConst,    "214748364"),
        std::make_tuple("BinaryTest_0035", vpiDivOp, UhdmType::LongIntTypespec,   false, vpiUIntConst,   "4294967295", UhdmType::LongIntTypespec,   false, vpiUIntConst,   "3",        UhdmType::LongIntTypespec,   false, vpiUIntConst,   "1431655765"),
        std::make_tuple("BinaryTest_0036", vpiDivOp, UhdmType::ShortIntTypespec,  true,  vpiIntConst,    "100",        UhdmType::ShortIntTypespec,  true,  vpiIntConst,    "4",        UhdmType::ShortIntTypespec,  true,  vpiIntConst,    "25"),
        std::make_tuple("BinaryTest_0037", vpiDivOp, UhdmType::ByteTypespec,      true,  vpiIntConst,    "9",          UhdmType::ByteTypespec,      true,  vpiIntConst,    "2",        UhdmType::ByteTypespec,      true,  vpiIntConst,    "4"),
        std::make_tuple("BinaryTest_0039", vpiDivOp, UhdmType::RealTypespec,      true,  vpiRealConst,   "10.0",       UhdmType::RealTypespec,      true,  vpiRealConst,   "4.0",      UhdmType::RealTypespec,      true,  vpiRealConst,   "2.5"),
        std::make_tuple("BinaryTest_0040", vpiDivOp, UhdmType::ShortRealTypespec, true,  vpiRealConst,   "-3.0",       UhdmType::ShortRealTypespec, true,  vpiRealConst,   "2.0",      UhdmType::ShortRealTypespec, true,  vpiRealConst,   "-1.5"),
        std::make_tuple("BinaryTest_0041", vpiDivOp, UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "11001000",   UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "01100100", UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "00000010"),
        std::make_tuple("BinaryTest_0042", vpiDivOp, UhdmType::TimeTypespec,      false, vpiIntConst,    "1000",       UhdmType::TimeTypespec,      false, vpiIntConst,    "50",       UhdmType::TimeTypespec,      false, vpiIntConst,    "20"),

        //------------------------- Binary Multiplcation -----------------------
        std::make_tuple("BinaryTest_0051", vpiMultOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "5",       UhdmType::IntTypespec,       true,  vpiIntConst,    "4",    UhdmType::IntTypespec,       true,  vpiIntConst,    "20"),
        std::make_tuple("BinaryTest_0052", vpiMultOp, UhdmType::IntTypespec,       false, vpiUIntConst,   "7",       UhdmType::IntTypespec,       false, vpiUIntConst,   "3",    UhdmType::IntTypespec,       false, vpiUIntConst,   "21"),
        std::make_tuple("BinaryTest_0053", vpiMultOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "-6",      UhdmType::IntTypespec,       true,  vpiIntConst,    "10",   UhdmType::IntTypespec,       true,  vpiIntConst,    "-60"),
        std::make_tuple("BinaryTest_0054", vpiMultOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "-4",      UhdmType::IntTypespec,       true,  vpiIntConst,    "-8",   UhdmType::IntTypespec,       true,  vpiIntConst,    "32"),
        std::make_tuple("BinaryTest_0055", vpiMultOp, UhdmType::LongIntTypespec,   true,  vpiIntConst,    "2000000", UhdmType::LongIntTypespec,   true,  vpiIntConst,    "3000", UhdmType::LongIntTypespec,   true,  vpiIntConst,    "6000000000"),
        std::make_tuple("BinaryTest_0056", vpiMultOp, UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "0100",    UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "0010", UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "1000"),
        std::make_tuple("BinaryTest_0057", vpiMultOp, UhdmType::ShortIntTypespec,  true,  vpiIntConst,    "12",      UhdmType::ShortIntTypespec,  true,  vpiIntConst,    "4",    UhdmType::ShortIntTypespec,  true,  vpiIntConst,    "48"),
        std::make_tuple("BinaryTest_0058", vpiMultOp, UhdmType::ByteTypespec,      true,  vpiIntConst,    "9",       UhdmType::ByteTypespec,      true,  vpiIntConst,    "0",    UhdmType::ByteTypespec,      true,  vpiIntConst,    "0"),
        std::make_tuple("BinaryTest_0060", vpiMultOp, UhdmType::RealTypespec,      true,  vpiRealConst,   "3.0",     UhdmType::RealTypespec,      true,  vpiRealConst,   "2.5",  UhdmType::RealTypespec,      true,  vpiRealConst,   "7.5"),
        std::make_tuple("BinaryTest_0061", vpiMultOp, UhdmType::ShortRealTypespec, true,  vpiRealConst,   "-1.5",    UhdmType::ShortRealTypespec, true,  vpiRealConst,   "2.0",  UhdmType::ShortRealTypespec, true,  vpiRealConst,   "-3"),
        std::make_tuple("BinaryTest_0062", vpiMultOp, UhdmType::TimeTypespec,      false, vpiIntConst,    "100",     UhdmType::TimeTypespec,      false, vpiIntConst,    "5",    UhdmType::TimeTypespec,      false, vpiIntConst,    "500"),

        //------------------------- Binary Mod Operation -----------------------
        std::make_tuple("BinaryTest_0063", vpiModOp, UhdmType::IntTypespec,     true,  vpiIntConst,    "20",         UhdmType::IntTypespec,     true,  vpiIntConst,    "6",    UhdmType::IntTypespec,     true,  vpiIntConst,    "2"),
        std::make_tuple("BinaryTest_0064", vpiModOp, UhdmType::IntTypespec,     false, vpiUIntConst,   "19",         UhdmType::IntTypespec,     false, vpiUIntConst,   "5",    UhdmType::IntTypespec,     false, vpiUIntConst,   "4"),
        std::make_tuple("BinaryTest_0065", vpiModOp, UhdmType::IntTypespec,     true,  vpiIntConst,    "-13",        UhdmType::IntTypespec,     true,  vpiIntConst,    "5",    UhdmType::IntTypespec,     true,  vpiIntConst,    "-3"),
        std::make_tuple("BinaryTest_0066", vpiModOp, UhdmType::IntTypespec,     true,  vpiIntConst,    "-13",        UhdmType::IntTypespec,     true,  vpiIntConst,    "-5",   UhdmType::IntTypespec,     true,  vpiIntConst,    "-3"),
        std::make_tuple("BinaryTest_0067", vpiModOp, UhdmType::LongIntTypespec, true,  vpiIntConst,    "4294967295", UhdmType::LongIntTypespec, true,  vpiIntConst,    "1000", UhdmType::LongIntTypespec, true,  vpiIntConst,    "295"),
        std::make_tuple("BinaryTest_0069", vpiModOp, UhdmType::RealTypespec,    true,  vpiRealConst,   "10.5",       UhdmType::RealTypespec,    true,  vpiRealConst,   "3.0",  UhdmType::RealTypespec,    true,  vpiRealConst,   "1.5"),
        std::make_tuple("BinaryTest_0070", vpiModOp, UhdmType::IntegerTypespec, true,  vpiBinaryConst, "11001001",   UhdmType::IntegerTypespec, true,  vpiBinaryConst, "1010", UhdmType::IntegerTypespec, true,  vpiBinaryConst, "00000001"),
        std::make_tuple("BinaryTest_0071", vpiModOp, UhdmType::TimeTypespec,    false, vpiIntConst,    "105",        UhdmType::TimeTypespec,    false, vpiIntConst,    "10",   UhdmType::TimeTypespec,    false, vpiIntConst,    "5"),

        //------------------------- Binary Exponentiation Operation -----------------------
        std::make_tuple("BinaryTest_0075", vpiPowerOp, UhdmType::IntTypespec,     true,  vpiIntConst,    "2",          UhdmType::IntTypespec,     true,  vpiIntConst,    "8",          UhdmType::IntTypespec,     true,  vpiIntConst,    "256"),
        std::make_tuple("BinaryTest_0076", vpiPowerOp, UhdmType::IntTypespec,     true,  vpiIntConst,    "-2",         UhdmType::IntTypespec,     true,  vpiIntConst,    "3",          UhdmType::IntTypespec,     true,  vpiIntConst,    "-8"),
        std::make_tuple("BinaryTest_0077", vpiPowerOp, UhdmType::IntTypespec,     true,  vpiIntConst,    "5",          UhdmType::IntTypespec,     true,  vpiIntConst,    "0",          UhdmType::IntTypespec,     true,  vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0078", vpiPowerOp, UhdmType::LongIntTypespec, true,  vpiIntConst,    "10",         UhdmType::LongIntTypespec, true,  vpiIntConst,    "6",          UhdmType::LongIntTypespec, true,  vpiIntConst,    "1000000"),
        std::make_tuple("BinaryTest_0080", vpiPowerOp, UhdmType::RealTypespec,    true,  vpiRealConst,   "2.0",        UhdmType::RealTypespec,    true,  vpiRealConst,   "4.0",        UhdmType::RealTypespec,    true,  vpiRealConst,   "16"),
        std::make_tuple("BinaryTest_0081", vpiPowerOp, UhdmType::IntegerTypespec, true,  vpiBinaryConst, "0000000011", UhdmType::IntegerTypespec, true,  vpiBinaryConst, "0000000110", UhdmType::IntegerTypespec, true,  vpiBinaryConst, "1011011001"),
        std::make_tuple("BinaryTest_0082", vpiPowerOp, UhdmType::TimeTypespec,    false, vpiIntConst,    "10",         UhdmType::TimeTypespec,    false, vpiIntConst,    "2",          UhdmType::TimeTypespec,    false, vpiIntConst,    "100"),

        // exponent edge cases
         //std::make_tuple("BinaryTest_0083", vpiPowerOp, UhdmType::ShortRealTypespec, true, vpiRealConst,"1.5", UhdmType::ShortRealTypespec, true, vpiRealConst,"0.0", UhdmType::ShortRealTypespec, true, vpiRealConst,"1"),
        std::make_tuple("BinaryTest_0084", vpiPowerOp, UhdmType::IntTypespec, true, vpiIntConst, "10", UhdmType::IntTypespec, true, vpiIntConst, "-1", UhdmType::IntTypespec, true, vpiIntConst, "0"),  // if your evaluator produces real 0.1 instead, adjust expected

        //-------------------- EQUALITY OPERATOR (==)     ----------------------------
        std::make_tuple("BinaryTest_0085", vpiEqOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "5",          UhdmType::IntTypespec,       true,  vpiIntConst,    "5",          UhdmType::LogicTypespec, true,  vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0086", vpiEqOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "5",          UhdmType::IntTypespec,       true,  vpiIntConst,    "6",          UhdmType::LogicTypespec, true,  vpiIntConst,    "0"),
        std::make_tuple("BinaryTest_0087", vpiEqOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "-10",        UhdmType::IntTypespec,       true,  vpiIntConst,    "-10",        UhdmType::LogicTypespec, true,  vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0088", vpiEqOp, UhdmType::IntTypespec,       true,  vpiIntConst,    "-3",         UhdmType::IntTypespec,       true,  vpiIntConst,    "7",          UhdmType::LogicTypespec, true,  vpiIntConst,    "0"),
        std::make_tuple("BinaryTest_0089", vpiEqOp, UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "0000000011", UhdmType::IntegerTypespec,   true,  vpiBinaryConst, "0000000011", UhdmType::LogicTypespec, true,  vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0091", vpiEqOp, UhdmType::RealTypespec,      true,  vpiRealConst,   "2.5",        UhdmType::RealTypespec,      true,  vpiRealConst,   "2.5",        UhdmType::LogicTypespec, true,  vpiRealConst,   "1"),
        std::make_tuple("BinaryTest_0092", vpiEqOp, UhdmType::ShortRealTypespec, true,  vpiRealConst,   "2.5",        UhdmType::ShortRealTypespec, true,  vpiRealConst,   "3.5",        UhdmType::LogicTypespec, true,  vpiRealConst,   "0"),
        std::make_tuple("BinaryTest_0095", vpiEqOp, UhdmType::TimeTypespec,      false, vpiIntConst,    "100",        UhdmType::TimeTypespec,      false, vpiIntConst,    "100",        UhdmType::LogicTypespec, false, vpiIntConst,    "1"),

        // X/Z behavior — returns 0 (unknown result)

        //-------------------- NOT EQUALITY OPERATOR (!=) ----------------------------
        std::make_tuple("BinaryTest_0100", vpiNeqOp, UhdmType::IntTypespec,     true,  vpiIntConst,  "5",      UhdmType::IntTypespec,     true,  vpiIntConst,  "6",      UhdmType::LogicTypespec, true,  vpiIntConst,  "1"),
        std::make_tuple("BinaryTest_0101", vpiNeqOp, UhdmType::IntTypespec,     true,  vpiIntConst,  "5",      UhdmType::IntTypespec,     true,  vpiIntConst,  "5",      UhdmType::LogicTypespec, true,  vpiIntConst,  "0"),
        std::make_tuple("BinaryTest_0102", vpiNeqOp, UhdmType::IntTypespec,     true,  vpiIntConst,  "-7",     UhdmType::IntTypespec,     true,  vpiIntConst,  "-8",     UhdmType::LogicTypespec, true,  vpiIntConst,  "1"),
        std::make_tuple("BinaryTest_0103", vpiNeqOp, UhdmType::IntTypespec,     true,  vpiIntConst,  "-7",     UhdmType::IntTypespec,     true,  vpiIntConst,  "-7",     UhdmType::LogicTypespec, true,  vpiIntConst,  "0"),
        std::make_tuple("BinaryTest_0104", vpiNeqOp, UhdmType::LongIntTypespec, true,  vpiIntConst,  "123456", UhdmType::LongIntTypespec, true,  vpiIntConst,  "123455", UhdmType::LogicTypespec, true,  vpiIntConst,  "1"),
        std::make_tuple("BinaryTest_0105", vpiNeqOp, UhdmType::RealTypespec,    true,  vpiRealConst, "2.5",    UhdmType::RealTypespec,    true,  vpiRealConst, "2.6",    UhdmType::LogicTypespec, true,  vpiRealConst, "1"),
        std::make_tuple("BinaryTest_0107", vpiNeqOp, UhdmType::TimeTypespec,    false, vpiIntConst,  "55",     UhdmType::TimeTypespec,    false, vpiIntConst,  "55",     UhdmType::LogicTypespec, false, vpiIntConst,  "0"),

        // X/Z behavior — returns 0 (unknown result)

        //-------------------- GREATER THAN OPERATOR (>) ----------------------------
        std::make_tuple("BinaryTest_0112", vpiGtOp, UhdmType::IntTypespec,       true,  vpiIntConst,  "10",   UhdmType::IntTypespec,       true,  vpiIntConst,  "5",    UhdmType::LogicTypespec, true,  vpiIntConst,  "1"),
        std::make_tuple("BinaryTest_0113", vpiGtOp, UhdmType::IntTypespec,       true,  vpiIntConst,  "-5",   UhdmType::IntTypespec,       true,  vpiIntConst,  "-10",  UhdmType::LogicTypespec, true,  vpiIntConst,  "1"),
        std::make_tuple("BinaryTest_0114", vpiGtOp, UhdmType::IntTypespec,       false, vpiUIntConst, "20",   UhdmType::IntTypespec,       false, vpiUIntConst, "30",   UhdmType::LogicTypespec, false, vpiUIntConst, "0"),
        std::make_tuple("BinaryTest_0116", vpiGtOp, UhdmType::ShortIntTypespec,  true,  vpiIntConst,  "120",  UhdmType::ShortIntTypespec,  true,  vpiIntConst,  "100",  UhdmType::LogicTypespec, true,  vpiIntConst,  "1"),
        std::make_tuple("BinaryTest_0117", vpiGtOp, UhdmType::ByteTypespec,      false, vpiUIntConst, "200",  UhdmType::ByteTypespec,      false, vpiUIntConst, "100",  UhdmType::LogicTypespec, false, vpiUIntConst, "1"),
        std::make_tuple("BinaryTest_0118", vpiGtOp, UhdmType::ShortRealTypespec, true,  vpiRealConst, "3.14", UhdmType::ShortRealTypespec, true,  vpiRealConst, "2.72", UhdmType::LogicTypespec, true,  vpiRealConst, "1"),

        //-------------------- LESS THAN OPERATOR (<) ----------------------------
        std::make_tuple("BinaryTest_0119", vpiLtOp, UhdmType::IntTypespec,       true,  vpiIntConst,  "5",    UhdmType::IntTypespec,       true,  vpiIntConst,  "10",  UhdmType::LogicTypespec, true,  vpiIntConst,  "1"),
        std::make_tuple("BinaryTest_0120", vpiLtOp, UhdmType::IntTypespec,       true,  vpiIntConst,  "-20",  UhdmType::IntTypespec,       true,  vpiIntConst,  "-5",  UhdmType::LogicTypespec, true,  vpiIntConst,  "1"),
        std::make_tuple("BinaryTest_0121", vpiLtOp, UhdmType::LongIntTypespec,   true,  vpiIntConst,  "1000", UhdmType::LongIntTypespec,   true,  vpiIntConst,  "5",   UhdmType::LogicTypespec, true,  vpiIntConst,  "0"),
        std::make_tuple("BinaryTest_0123", vpiLtOp, UhdmType::ShortIntTypespec,  true,  vpiIntConst,  "-5",   UhdmType::ShortIntTypespec,  true,  vpiIntConst,  "0",   UhdmType::LogicTypespec, true,  vpiIntConst,  "1"),
        std::make_tuple("BinaryTest_0124", vpiLtOp, UhdmType::ByteTypespec,      false, vpiUIntConst, "5",    UhdmType::ByteTypespec,      false, vpiUIntConst, "10",  UhdmType::LogicTypespec, false, vpiUIntConst, "1"),
        std::make_tuple("BinaryTest_0125", vpiLtOp, UhdmType::ShortRealTypespec, true,  vpiRealConst, "-1.0", UhdmType::ShortRealTypespec, true,  vpiRealConst, "0.0", UhdmType::LogicTypespec, true,  vpiRealConst, "1"),

        //-------------------- GREATER THAN EQUAL TO OPERATOR (>=) ----------------------------
        std::make_tuple("BinaryTest_0126", vpiGeOp, UhdmType::IntTypespec,       true, vpiIntConst,  "10",  UhdmType::IntTypespec,       true, vpiIntConst,  "10",   UhdmType::LogicTypespec, true, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0127", vpiGeOp, UhdmType::IntTypespec,       true, vpiIntConst,  "-5",  UhdmType::IntTypespec,       true, vpiIntConst,  "-10",  UhdmType::LogicTypespec, true, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0128", vpiGeOp, UhdmType::IntegerTypespec,   true, vpiIntConst,  "900", UhdmType::IntegerTypespec,   true, vpiIntConst,  "1000", UhdmType::LogicTypespec, true, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0130", vpiLeOp, UhdmType::ShortIntTypespec,  true, vpiIntConst,  "0",   UhdmType::ShortIntTypespec,  true, vpiIntConst,  "0",    UhdmType::LogicTypespec, true, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0131", vpiLeOp, UhdmType::ShortRealTypespec, true, vpiRealConst, "2.5", UhdmType::ShortRealTypespec, true, vpiRealConst, "2.6",  UhdmType::LogicTypespec, true, vpiRealConst,   "1"),

        //-------------------- LESS THAN EQUAL TO OPERATOR (>=) ----------------------------
        std::make_tuple("BinaryTest_0132", vpiLeOp, UhdmType::IntTypespec,     true, vpiIntConst, "10",  UhdmType::IntTypespec,     true, vpiIntConst, "10",  UhdmType::LogicTypespec, true, vpiIntConst, "1"),
        std::make_tuple("BinaryTest_0133", vpiLeOp, UhdmType::IntTypespec,     true, vpiIntConst, "-50", UhdmType::IntTypespec,     true, vpiIntConst, "-1",  UhdmType::LogicTypespec, true, vpiIntConst, "1"),
        std::make_tuple("BinaryTest_0134", vpiLeOp, UhdmType::LongIntTypespec, true, vpiIntConst, "800", UhdmType::LongIntTypespec, true, vpiIntConst, "400", UhdmType::LogicTypespec, true, vpiIntConst, "0"),

        //-------------------- LOGICAL AND OPERATOR (&&) ----------------------------
        std::make_tuple("BinaryTest_0136", vpiLogAndOp, UhdmType::IntTypespec,       false, vpiIntConst,    "1",    UhdmType::IntTypespec,       false, vpiIntConst,    "1",    UhdmType::LogicTypespec, false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0137", vpiLogAndOp, UhdmType::IntTypespec,       false, vpiIntConst,    "1",    UhdmType::IntTypespec,       false, vpiIntConst,    "0",    UhdmType::LogicTypespec, false, vpiIntConst,    "0"),
        std::make_tuple("BinaryTest_0138", vpiLogAndOp, UhdmType::LongIntTypespec,   false, vpiIntConst,    "-5",   UhdmType::LongIntTypespec,   false, vpiIntConst,    "10",   UhdmType::LogicTypespec, false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0139", vpiLogAndOp, UhdmType::ShortIntTypespec,  false, vpiIntConst,    "0",    UhdmType::ShortIntTypespec,  false, vpiIntConst,    "-3",   UhdmType::LogicTypespec, false, vpiIntConst,    "0"),
        std::make_tuple("BinaryTest_0140", vpiLogAndOp, UhdmType::ByteTypespec,      false, vpiIntConst,    "1",    UhdmType::ByteTypespec,      false, vpiIntConst,    "255",  UhdmType::LogicTypespec, false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0141", vpiLogAndOp, UhdmType::IntegerTypespec,   false, vpiBinaryConst, "0000", UhdmType::IntegerTypespec,   false, vpiBinaryConst, "1111", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0142", vpiLogAndOp, UhdmType::TimeTypespec,      false, vpiIntConst,    "10",   UhdmType::TimeTypespec,      false, vpiIntConst,    "5",    UhdmType::LogicTypespec, false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0143", vpiLogAndOp, UhdmType::RealTypespec,      true, vpiRealConst,    "3.14", UhdmType::RealTypespec,      true,  vpiRealConst,   "0.0",  UhdmType::LogicTypespec, true,  vpiIntConst,    "0"),
        std::make_tuple("BinaryTest_0144", vpiLogAndOp, UhdmType::ShortRealTypespec, true, vpiRealConst,    "1.1",  UhdmType::ShortRealTypespec, true,  vpiRealConst,   "-4.2", UhdmType::LogicTypespec, true,  vpiIntConst,    "1"),

        //-------------------- LOGICAL OR OPERATOR (||) ----------------------------
        std::make_tuple("BinaryTest_0149", vpiLogOrOp, UhdmType::IntTypespec,       false, vpiIntConst,    "1",   UhdmType::IntTypespec,       false, vpiIntConst,    "0",   UhdmType::LogicTypespec, false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0150", vpiLogOrOp, UhdmType::IntTypespec,       false, vpiIntConst,    "0",   UhdmType::IntTypespec,       false, vpiIntConst,    "0",   UhdmType::LogicTypespec, false, vpiIntConst,    "0"),
        std::make_tuple("BinaryTest_0151", vpiLogOrOp, UhdmType::LongIntTypespec,   false, vpiIntConst,    "0",   UhdmType::LongIntTypespec,   false, vpiIntConst,    "-10", UhdmType::LogicTypespec, false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0152", vpiLogOrOp, UhdmType::ShortIntTypespec,  false, vpiIntConst,    "0",   UhdmType::ShortIntTypespec,  false, vpiIntConst,    "99",  UhdmType::LogicTypespec, false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0153", vpiLogOrOp, UhdmType::ByteTypespec,      false, vpiIntConst,    "255", UhdmType::ByteTypespec,      false, vpiIntConst,    "0",   UhdmType::LogicTypespec, false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0154", vpiLogOrOp, UhdmType::IntegerTypespec,   false, vpiBinaryConst, "0",   UhdmType::IntegerTypespec,   false, vpiBinaryConst, "0",   UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0155", vpiLogOrOp, UhdmType::TimeTypespec,      false, vpiIntConst,    "0",   UhdmType::TimeTypespec,      false, vpiIntConst,    "10",  UhdmType::LogicTypespec, false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0156", vpiLogOrOp, UhdmType::RealTypespec,      true, vpiRealConst,    "0.0", UhdmType::RealTypespec,      true,  vpiRealConst,   "1.2", UhdmType::LogicTypespec, true,  vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0157", vpiLogOrOp, UhdmType::ShortRealTypespec, true, vpiRealConst,    "0.0", UhdmType::ShortRealTypespec, true,  vpiRealConst,   "0.0", UhdmType::LogicTypespec, true,  vpiIntConst,    "0"),

        //-------------------- BITWISE AND OPERATOR (&) ----------------------------
        std::make_tuple("BinaryTest_0162", vpiBitAndOp, UhdmType::IntTypespec,      false, vpiIntConst,    "5",    UhdmType::IntTypespec,      false, vpiIntConst,    "3",    UhdmType::IntTypespec,      false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0163", vpiBitAndOp, UhdmType::LongIntTypespec,  false, vpiIntConst,    "15",   UhdmType::LongIntTypespec,  false, vpiIntConst,    "7",    UhdmType::LongIntTypespec,  false, vpiIntConst,    "7"),
        std::make_tuple("BinaryTest_0164", vpiBitAndOp, UhdmType::ShortIntTypespec, false, vpiIntConst,    "2",    UhdmType::ShortIntTypespec, false, vpiIntConst,    "1",    UhdmType::ShortIntTypespec, false, vpiIntConst,    "0"),
        std::make_tuple("BinaryTest_0165", vpiBitAndOp, UhdmType::ByteTypespec,     false, vpiIntConst,    "255",  UhdmType::ByteTypespec,     false, vpiIntConst,    "1",    UhdmType::ByteTypespec,     false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0166", vpiBitAndOp, UhdmType::IntegerTypespec,  false, vpiBinaryConst, "1000", UhdmType::IntegerTypespec,  false, vpiBinaryConst, "0100", UhdmType::IntegerTypespec,  false, vpiBinaryConst, "0000"),
        std::make_tuple("BinaryTest_0167", vpiBitAndOp, UhdmType::TimeTypespec,     false, vpiIntConst,    "10",   UhdmType::TimeTypespec,     false, vpiIntConst,    "3",    UhdmType::TimeTypespec,     false, vpiIntConst,    "2"),

        //-------------------- BITWISE OR OPERATOR (&) ----------------------------
        std::make_tuple("BinaryTest_0174", vpiBitOrOp, UhdmType::IntTypespec,      false, vpiIntConst,    "5",    UhdmType::IntTypespec,      false, vpiIntConst,    "3",    UhdmType::IntTypespec,      false, vpiIntConst,    "7"),
        std::make_tuple("BinaryTest_0175", vpiBitOrOp, UhdmType::LongIntTypespec,  false, vpiIntConst,    "8",    UhdmType::LongIntTypespec,  false, vpiIntConst,    "1",    UhdmType::LongIntTypespec,  false, vpiIntConst,    "9"),
        std::make_tuple("BinaryTest_0176", vpiBitOrOp, UhdmType::ShortIntTypespec, false, vpiIntConst,    "2",    UhdmType::ShortIntTypespec, false, vpiIntConst,    "1",    UhdmType::ShortIntTypespec, false, vpiIntConst,    "3"),
        std::make_tuple("BinaryTest_0177", vpiBitOrOp, UhdmType::ByteTypespec,     false, vpiIntConst,    "128",  UhdmType::ByteTypespec,     false, vpiIntConst,    "1",    UhdmType::ByteTypespec,     false, vpiIntConst,    "129"),
        std::make_tuple("BinaryTest_0178", vpiBitOrOp, UhdmType::IntegerTypespec,  false, vpiBinaryConst, "0100", UhdmType::IntegerTypespec,  false, vpiBinaryConst, "0010", UhdmType::IntegerTypespec,  false, vpiBinaryConst, "0110"),
        std::make_tuple("BinaryTest_0179", vpiBitOrOp, UhdmType::TimeTypespec,     false, vpiIntConst,    "10",   UhdmType::TimeTypespec,     false, vpiIntConst,    "3",    UhdmType::TimeTypespec,     false, vpiIntConst,    "11"),

        //-------------------- BITWISE XOR OPERATOR (^) ----------------------------
        std::make_tuple("BinaryTest_0185", vpiBitXorOp, UhdmType::IntTypespec,      false, vpiIntConst,    "5",    UhdmType::IntTypespec,      false, vpiIntConst,    "3",    UhdmType::IntTypespec,      false, vpiIntConst,    "6"),
        std::make_tuple("BinaryTest_0186", vpiBitXorOp, UhdmType::LongIntTypespec,  false, vpiIntConst,    "12",   UhdmType::LongIntTypespec,  false, vpiIntConst,    "10",   UhdmType::LongIntTypespec,  false, vpiIntConst,    "6"),
        std::make_tuple("BinaryTest_0187", vpiBitXorOp, UhdmType::ShortIntTypespec, false, vpiIntConst,    "7",    UhdmType::ShortIntTypespec, false, vpiIntConst,    "3",    UhdmType::ShortIntTypespec, false, vpiIntConst,    "4"),
        std::make_tuple("BinaryTest_0188", vpiBitXorOp, UhdmType::ByteTypespec,     false, vpiIntConst,    "255",  UhdmType::ByteTypespec,     false, vpiIntConst,    "1",    UhdmType::ByteTypespec,     false, vpiIntConst,    "254"),
        std::make_tuple("BinaryTest_0189", vpiBitXorOp, UhdmType::IntegerTypespec,  false, vpiBinaryConst, "1000", UhdmType::IntegerTypespec,  false, vpiBinaryConst, "0100", UhdmType::IntegerTypespec,  false, vpiBinaryConst, "1100"),

        //-------------------- BITWISE NXOR OPERATOR (~^) ----------------------------
        std::make_tuple("BinaryTest_0194", vpiBitXnorOp, UhdmType::IntTypespec,      false, vpiIntConst,    "5",    UhdmType::IntTypespec,      false, vpiIntConst,    "3",    UhdmType::IntTypespec,      false, vpiIntConst,    "4294967289"),
        std::make_tuple("BinaryTest_0195", vpiBitXnorOp, UhdmType::LongIntTypespec,  false, vpiIntConst,    "12",   UhdmType::LongIntTypespec,  false, vpiIntConst,    "10",   UhdmType::LongIntTypespec,  false, vpiIntConst,    "18446744073709551609"),
        std::make_tuple("BinaryTest_0196", vpiBitXnorOp, UhdmType::ShortIntTypespec, false, vpiIntConst,    "7",    UhdmType::ShortIntTypespec, false, vpiIntConst,    "3",    UhdmType::ShortIntTypespec, false, vpiIntConst,    "65531"),
        std::make_tuple("BinaryTest_0197", vpiBitXnorOp, UhdmType::ByteTypespec,     false, vpiIntConst,    "255",  UhdmType::ByteTypespec,     false, vpiIntConst,    "1",    UhdmType::ByteTypespec,     false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0198", vpiBitXnorOp, UhdmType::IntegerTypespec,  false, vpiBinaryConst, "1000", UhdmType::IntegerTypespec,  false, vpiBinaryConst, "0100", UhdmType::IntegerTypespec,  false, vpiBinaryConst, "0011"),

        //----------------- Operators with negative input ---------------------------
        std::make_tuple("BinaryTest_0204", vpiLogAndOp,  UhdmType::IntTypespec,      false, vpiIntConst,    "-1",                               UhdmType::IntTypespec,      false, vpiIntConst,    "-5",                               UhdmType::LogicTypespec,    false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0205", vpiLogAndOp,  UhdmType::IntegerTypespec,  false, vpiBinaryConst, "11111001",                         UhdmType::IntegerTypespec,  false, vpiBinaryConst, "0",                                UhdmType::LogicTypespec,    false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0206", vpiLogOrOp,   UhdmType::LongIntTypespec,  false, vpiIntConst,    "-3",                               UhdmType::LongIntTypespec,  false, vpiIntConst,    "-8",                               UhdmType::LogicTypespec,    false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0207", vpiLogOrOp,   UhdmType::IntTypespec,      false, vpiIntConst,    "-1",                               UhdmType::IntTypespec,      false, vpiIntConst,    "0",                                UhdmType::LogicTypespec,    false, vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0208", vpiBitAndOp,  UhdmType::IntTypespec,      false, vpiIntConst,    "-5",                               UhdmType::IntTypespec,      false, vpiIntConst,    "-3",                               UhdmType::IntTypespec,      false, vpiUIntConst,   "4294967289"),
        std::make_tuple("BinaryTest_0212", vpiBitXorOp,  UhdmType::LongIntTypespec,  false, vpiIntConst,    "-6",                               UhdmType::LongIntTypespec,  false, vpiIntConst,    "-3",                               UhdmType::LongIntTypespec,  false, vpiIntConst,    "7"),
        std::make_tuple("BinaryTest_0213", vpiBitXorOp,  UhdmType::ShortIntTypespec, false, vpiIntConst,    "-1",                               UhdmType::ShortIntTypespec, false, vpiIntConst,    "-1",                               UhdmType::ShortIntTypespec, false, vpiIntConst,    "0"),
        std::make_tuple("BinaryTest_0214", vpiBitXnorOp, UhdmType::IntTypespec,      false, vpiIntConst,    "-5",                               UhdmType::IntTypespec,      false, vpiIntConst,    "-5",                               UhdmType::IntTypespec,      false, vpiUIntConst,   "4294967295"),
        std::make_tuple("BinaryTest_0210", vpiBitOrOp,   UhdmType::IntegerTypespec,  false, vpiBinaryConst, "11111111111111111111111111111100", UhdmType::IntegerTypespec,  false, vpiBinaryConst, "11111111111111111111111111111110", UhdmType::IntegerTypespec,  false, vpiBinaryConst, "11111111111111111111111111111110"),
        std::make_tuple("BinaryTest_0215", vpiBitXnorOp, UhdmType::IntegerTypespec,  false, vpiBinaryConst, "11111111111111111111111111111110", UhdmType::IntegerTypespec,  false, vpiBinaryConst, "11111111111111111111111111111101", UhdmType::IntegerTypespec,  false, vpiBinaryConst, "11111111111111111111111111111100"),

        //-------------------- SHIFT OPERATORS (<<)----------------------------
        std::make_tuple("BinaryTest_0216", vpiLShiftOp,      UhdmType::IntTypespec, false, vpiIntConst, "0",          UhdmType::IntTypespec, false, vpiIntConst, "4",  UhdmType::IntTypespec, false, vpiIntConst, "0"),
        std::make_tuple("BinaryTest_0217", vpiLShiftOp,      UhdmType::IntTypespec, false, vpiIntConst, "1",          UhdmType::IntTypespec, false, vpiIntConst, "0",  UhdmType::IntTypespec, false, vpiIntConst, "1"),
        std::make_tuple("BinaryTest_0218", vpiLShiftOp,      UhdmType::IntTypespec, false, vpiIntConst, "-1",         UhdmType::IntTypespec, false, vpiIntConst, "1",  UhdmType::IntTypespec, false, vpiIntConst, "4294967294"),
        std::make_tuple("BinaryTest_0219", vpiLShiftOp,      UhdmType::IntTypespec, false, vpiIntConst, "8",          UhdmType::IntTypespec, false, vpiIntConst, "32", UhdmType::IntTypespec, false, vpiIntConst, "0"), // overshift -> 0
        std::make_tuple("BinaryTest_0220", vpiRShiftOp,      UhdmType::IntTypespec, true,  vpiIntConst, "-16",        UhdmType::IntTypespec, true,  vpiIntConst, "2",  UhdmType::IntTypespec, true,  vpiIntConst, "1073741820"),
        std::make_tuple("BinaryTest_0221", vpiRShiftOp,      UhdmType::IntTypespec, false, vpiIntConst, "256",        UhdmType::IntTypespec, false, vpiIntConst, "8",  UhdmType::IntTypespec, false, vpiIntConst, "1"),
        std::make_tuple("BinaryTest_0222", vpiArithLShiftOp, UhdmType::IntTypespec, true,  vpiIntConst, "-2",         UhdmType::IntTypespec, true,  vpiIntConst, "3",  UhdmType::IntTypespec, true,  vpiIntConst, "-16"),
        std::make_tuple("BinaryTest_0223", vpiArithRShiftOp, UhdmType::IntTypespec, true,  vpiIntConst, "-128",       UhdmType::IntTypespec, true,  vpiIntConst, "7",  UhdmType::IntTypespec, true,  vpiIntConst, "-1"),
        std::make_tuple("BinaryTest_0224", vpiLShiftOp,      UhdmType::IntTypespec, false, vpiIntConst, "2147483648", UhdmType::IntTypespec, false, vpiIntConst, "1",  UhdmType::IntTypespec, false, vpiIntConst, "0"),

        std::make_tuple("BinaryTest_0226", vpiRShiftOp, UhdmType::IntTypespec,   false, vpiIntConst,    "5",           UhdmType::IntTypespec,   false, vpiIntConst,    "-1",   UhdmType::IntTypespec,   false, vpiIntConst,    "0"), // negative will be treated as 0
        std::make_tuple("BinaryTest_0227", vpiLShiftOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1x01",        UhdmType::LogicTypespec, false, vpiBinaryConst, "0010", UhdmType::LogicTypespec, false, vpiBinaryConst, "0100"),
        std::make_tuple("BinaryTest_0228", vpiRShiftOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1x01",        UhdmType::LogicTypespec, false, vpiBinaryConst, "0010", UhdmType::LogicTypespec, false, vpiBinaryConst, "001x"),
        std::make_tuple("BinaryTest_0228", vpiLShiftOp, UhdmType::IntTypespec,   false, vpiIntConst,    "2147483647",  UhdmType::IntTypespec,   false, vpiIntConst,    "1",    UhdmType::IntTypespec,   false, vpiIntConst,    "4294967294"),
        std::make_tuple("BinaryTest_0229", vpiRShiftOp, UhdmType::IntTypespec,   true,  vpiIntConst,    "-2147483648", UhdmType::IntTypespec,   true,  vpiIntConst,    "31",   UhdmType::IntTypespec,   true,  vpiIntConst,    "1"),
        std::make_tuple("BinaryTest_0230", vpiLShiftOp, UhdmType::IntTypespec,   false, vpiIntConst,    "1",           UhdmType::IntTypespec,   false, vpiIntConst,    "0",    UhdmType::IntTypespec,   false, vpiIntConst,    "1"),

        // IntegerTypespec
        std::make_tuple("BinaryTest_0231", vpiLShiftOp,      UhdmType::IntegerTypespec, false, vpiBinaryConst, "0",                                                                UhdmType::IntegerTypespec, false, vpiBinaryConst, "101",    UhdmType::IntegerTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0232", vpiLShiftOp,      UhdmType::IntegerTypespec, false, vpiBinaryConst, "1",                                                                UhdmType::IntegerTypespec, false, vpiBinaryConst, "1",      UhdmType::IntegerTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0233", vpiLShiftOp,      UhdmType::IntegerTypespec, true,  vpiBinaryConst, "11111111111111111111111111111111",                                 UhdmType::IntegerTypespec, true,  vpiBinaryConst, "11",     UhdmType::IntegerTypespec, true,  vpiBinaryConst, "11111111111111111111111111111000"),
        std::make_tuple("BinaryTest_0234", vpiRShiftOp,      UhdmType::IntegerTypespec, true,  vpiBinaryConst, "11111111111111111100000000000000",                                 UhdmType::IntegerTypespec, true,  vpiBinaryConst, "1010",   UhdmType::IntegerTypespec, true,  vpiBinaryConst, "00000000001111111111111111110000"),
        std::make_tuple("BinaryTest_0235", vpiRShiftOp,      UhdmType::IntegerTypespec, false, vpiBinaryConst, "10000000000",                                                      UhdmType::IntegerTypespec, false, vpiBinaryConst, "1010",   UhdmType::IntegerTypespec, false, vpiBinaryConst, "00000000001"),
        std::make_tuple("BinaryTest_0236", vpiArithLShiftOp, UhdmType::IntegerTypespec, true,  vpiBinaryConst, "11111111111111111111111111111110",                                 UhdmType::IntegerTypespec, true,  vpiBinaryConst, "100",    UhdmType::IntegerTypespec, true,  vpiBinaryConst, "11111111111111111111111111100000"),
        std::make_tuple("BinaryTest_0237", vpiArithRShiftOp, UhdmType::IntegerTypespec, true,  vpiBinaryConst, "11111111111111111000000000000000",                                 UhdmType::IntegerTypespec, true,  vpiBinaryConst, "10000",  UhdmType::IntegerTypespec, true,  vpiBinaryConst, "00000000000000001111111111111111"),
        std::make_tuple("BinaryTest_0238", vpiLShiftOp,      UhdmType::IntegerTypespec, false, vpiBinaryConst, "1",                                                                UhdmType::IntegerTypespec, false, vpiBinaryConst, "111111", UhdmType::IntegerTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0239", vpiRShiftOp,      UhdmType::IntegerTypespec, false, vpiBinaryConst, "1000000000000000000000000000000000000000000000000000000000000000", UhdmType::IntegerTypespec, false, vpiBinaryConst, "111111", UhdmType::IntegerTypespec, false, vpiBinaryConst, "0000000000000000000000000000000000000000000000000000000000000001"),
        std::make_tuple("BinaryTest_0240", vpiLShiftOp,      UhdmType::IntegerTypespec, false, vpiBinaryConst, "11111111",                                                         UhdmType::IntegerTypespec, false, vpiBinaryConst, "1000",   UhdmType::IntegerTypespec, false, vpiBinaryConst, "00000000"),
        std::make_tuple("BinaryTest_0243", vpiArithRShiftOp, UhdmType::IntegerTypespec, true,  vpiBinaryConst, "10000000000000000000000000000000",                                 UhdmType::IntegerTypespec, true,  vpiBinaryConst, "1",      UhdmType::IntegerTypespec, true,  vpiBinaryConst, "01000000000000000000000000000000"),
        std::make_tuple("BinaryTest_0244", vpiLShiftOp,      UhdmType::IntegerTypespec, false, vpiBinaryConst, "0",                                                                UhdmType::IntegerTypespec, false, vpiBinaryConst, "0",      UhdmType::IntegerTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0281", vpiLShiftOp,      UhdmType::IntegerTypespec, false, vpiBinaryConst, "1x010010",                                                         UhdmType::IntegerTypespec, false, vpiBinaryConst, "11",     UhdmType::IntegerTypespec, false, vpiBinaryConst, "10010000"),

        // LongIntTypespec
        std::make_tuple("BinaryTest_0245", vpiLShiftOp,      UhdmType::LongIntTypespec, false, vpiIntConst, "1",                    UhdmType::LongIntTypespec, false, vpiIntConst, "1",  UhdmType::LongIntTypespec, false, vpiIntConst, "2"),
        std::make_tuple("BinaryTest_0245", vpiLShiftOp,      UhdmType::LongIntTypespec, false, vpiIntConst, "1",                    UhdmType::LongIntTypespec, false, vpiIntConst, "1",  UhdmType::LongIntTypespec, false, vpiIntConst, "2"),
        std::make_tuple("BinaryTest_0246", vpiLShiftOp,      UhdmType::LongIntTypespec, true,  vpiIntConst, "-1",                   UhdmType::LongIntTypespec, true,  vpiIntConst, "2",  UhdmType::LongIntTypespec, true,  vpiIntConst, "-4"),
        std::make_tuple("BinaryTest_0247", vpiLShiftOp,      UhdmType::LongIntTypespec, false, vpiIntConst, "1",                    UhdmType::LongIntTypespec, false, vpiIntConst, "63", UhdmType::LongIntTypespec, false, vpiIntConst, "9223372036854775808"),
        std::make_tuple("BinaryTest_0248", vpiRShiftOp,      UhdmType::LongIntTypespec, true,  vpiIntConst, "-9223372036854775808", UhdmType::LongIntTypespec, true,  vpiIntConst, "63", UhdmType::LongIntTypespec, true,  vpiIntConst, "1"),
        std::make_tuple("BinaryTest_0249", vpiRShiftOp,      UhdmType::LongIntTypespec, false, vpiIntConst, "1024",                 UhdmType::LongIntTypespec, false, vpiIntConst, "10", UhdmType::LongIntTypespec, false, vpiIntConst, "1"),
        std::make_tuple("BinaryTest_0250", vpiArithLShiftOp, UhdmType::LongIntTypespec, true,  vpiIntConst, "-2",                   UhdmType::LongIntTypespec, true,  vpiIntConst, "4",  UhdmType::LongIntTypespec, true,  vpiIntConst, "-32"),
        std::make_tuple("BinaryTest_0251", vpiArithRShiftOp, UhdmType::LongIntTypespec, true,  vpiIntConst, "-128",                 UhdmType::LongIntTypespec, true,  vpiIntConst, "7",  UhdmType::LongIntTypespec, true,  vpiIntConst, "-1"),
        std::make_tuple("BinaryTest_0253", vpiLShiftOp,      UhdmType::LongIntTypespec, false, vpiIntConst, "255",                  UhdmType::LongIntTypespec, false, vpiIntConst, "8",  UhdmType::LongIntTypespec, false, vpiIntConst, "65280"),
        std::make_tuple("BinaryTest_0256", vpiLShiftOp,      UhdmType::LongIntTypespec, false, vpiIntConst, "0",                    UhdmType::LongIntTypespec, false, vpiIntConst, "0",  UhdmType::LongIntTypespec, false, vpiIntConst, "0"),
        std::make_tuple("BinaryTest_0257", vpiRShiftOp,      UhdmType::LongIntTypespec, false, vpiIntConst, "2",                    UhdmType::LongIntTypespec, false, vpiIntConst, "1",  UhdmType::LongIntTypespec, false, vpiIntConst, "1"),

        // ShortIntTypespec
        std::make_tuple("BinaryTest_0258", vpiLShiftOp,      UhdmType::ShortIntTypespec, true,  vpiIntConst,    "-1",     UhdmType::ShortIntTypespec, true,  vpiIntConst, "1",  UhdmType::ShortIntTypespec, true,  vpiIntConst, "-2"),
        std::make_tuple("BinaryTest_0259", vpiLShiftOp,      UhdmType::ShortIntTypespec, true,  vpiIntConst,    "15",     UhdmType::ShortIntTypespec, true,  vpiIntConst, "1",  UhdmType::ShortIntTypespec, true,  vpiIntConst, "30"),
        std::make_tuple("BinaryTest_0260", vpiRShiftOp,      UhdmType::ShortIntTypespec, true,  vpiIntConst,    "-32768", UhdmType::ShortIntTypespec, true,  vpiIntConst, "1",  UhdmType::ShortIntTypespec, true,  vpiIntConst, "16384"),
        std::make_tuple("BinaryTest_0261", vpiRShiftOp,      UhdmType::ShortIntTypespec, false, vpiIntConst,    "255",    UhdmType::ShortIntTypespec, false, vpiIntConst, "4",  UhdmType::ShortIntTypespec, false, vpiIntConst, "15"),
        std::make_tuple("BinaryTest_0262", vpiArithLShiftOp, UhdmType::ShortIntTypespec, true,  vpiIntConst,    "-2",     UhdmType::ShortIntTypespec, true,  vpiIntConst, "8",  UhdmType::ShortIntTypespec, true,  vpiIntConst, "-512"),
        std::make_tuple("BinaryTest_0263", vpiArithRShiftOp, UhdmType::ShortIntTypespec, true,  vpiIntConst,    "-1024",  UhdmType::ShortIntTypespec, true,  vpiIntConst, "6",  UhdmType::ShortIntTypespec, true,  vpiIntConst, "-16"),
        std::make_tuple("BinaryTest_0264", vpiLShiftOp,      UhdmType::ShortIntTypespec, false, vpiBinaryConst, "1010",   UhdmType::ShortIntTypespec, false, vpiIntConst, "2",  UhdmType::ShortIntTypespec, false, vpiIntConst, "40"),
        std::make_tuple("BinaryTest_0265", vpiLShiftOp,      UhdmType::ShortIntTypespec, false, vpiIntConst,    "15",     UhdmType::ShortIntTypespec, false, vpiIntConst, "4",  UhdmType::ShortIntTypespec, false, vpiIntConst, "240"),
        std::make_tuple("BinaryTest_0267", vpiLShiftOp,      UhdmType::ShortIntTypespec, false, vpiIntConst,    "0",      UhdmType::ShortIntTypespec, false, vpiIntConst, "31", UhdmType::ShortIntTypespec, false, vpiIntConst, "0"),
        std::make_tuple("BinaryTest_0268", vpiArithRShiftOp, UhdmType::ShortIntTypespec, true,  vpiIntConst,    "-1",     UhdmType::ShortIntTypespec, true,  vpiIntConst, "15", UhdmType::ShortIntTypespec, true,  vpiIntConst, "-1"),
        std::make_tuple("BinaryTest_0269", vpiRShiftOp,      UhdmType::ShortIntTypespec, false, vpiIntConst,    "255",    UhdmType::ShortIntTypespec, false, vpiIntConst, "4",  UhdmType::ShortIntTypespec, false, vpiIntConst, "15"),
        std::make_tuple("BinaryTest_0271", vpiArithLShiftOp, UhdmType::ShortIntTypespec, false, vpiIntConst,    "2",      UhdmType::ShortIntTypespec, false, vpiIntConst, "3",  UhdmType::ShortIntTypespec, false, vpiIntConst, "16"),
        std::make_tuple("BinaryTest_0272", vpiRShiftOp,      UhdmType::ShortIntTypespec, true,  vpiIntConst,    "-16",    UhdmType::ShortIntTypespec, true,  vpiIntConst, "2",  UhdmType::ShortIntTypespec, true,  vpiIntConst, "16380"),

        // ByteTypespec
        std::make_tuple("BinaryTest_0273", vpiLShiftOp,      UhdmType::ByteTypespec, false, vpiUIntConst,"1",    UhdmType::ByteTypespec, false, vpiUIntConst, "1",  UhdmType::ByteTypespec, false, vpiUIntConst, "2"),
        std::make_tuple("BinaryTest_0274", vpiLShiftOp,      UhdmType::ByteTypespec, false, vpiUIntConst,"255",  UhdmType::ByteTypespec, false, vpiUIntConst, "1",  UhdmType::ByteTypespec, false, vpiUIntConst, "254"),
        std::make_tuple("BinaryTest_0275", vpiRShiftOp,      UhdmType::ByteTypespec, false, vpiUIntConst,"128",  UhdmType::ByteTypespec, false, vpiUIntConst, "7",  UhdmType::ByteTypespec, false, vpiUIntConst, "1"),
        std::make_tuple("BinaryTest_0276", vpiArithRShiftOp, UhdmType::ByteTypespec, true,  vpiIntConst, "-128", UhdmType::ByteTypespec, true,  vpiIntConst,  "7",  UhdmType::ByteTypespec, true,  vpiIntConst,  "-1"),
        std::make_tuple("BinaryTest_0277", vpiArithLShiftOp, UhdmType::ByteTypespec, false, vpiUIntConst,"2",    UhdmType::ByteTypespec, false, vpiUIntConst, "4",  UhdmType::ByteTypespec, false, vpiUIntConst, "32"),
        std::make_tuple("BinaryTest_0278", vpiLShiftOp,      UhdmType::ByteTypespec, false, vpiUIntConst,"0",    UhdmType::ByteTypespec, false, vpiUIntConst, "8",  UhdmType::ByteTypespec, false, vpiUIntConst, "0"),
        std::make_tuple("BinaryTest_0280", vpiLShiftOp,      UhdmType::ByteTypespec, false, vpiUIntConst, "15",  UhdmType::ByteTypespec, false, vpiUIntConst, "4",  UhdmType::ByteTypespec, false, vpiUIntConst, "240"),
        std::make_tuple("BinaryTest_0282", vpiRShiftOp,      UhdmType::ByteTypespec, true,  vpiIntConst, "-2",   UhdmType::ByteTypespec, true,  vpiIntConst,  "1",  UhdmType::ByteTypespec, true,  vpiIntConst,  "-1"),
        std::make_tuple("BinaryTest_0283", vpiArithRShiftOp, UhdmType::ByteTypespec, true,  vpiIntConst, "-1",   UhdmType::ByteTypespec, true,  vpiIntConst,  "8",  UhdmType::ByteTypespec, true,  vpiIntConst,  "-1"),
        std::make_tuple("BinaryTest_0284", vpiArithLShiftOp, UhdmType::ByteTypespec, false, vpiUIntConst,"4",    UhdmType::ByteTypespec, false, vpiUIntConst, "2",  UhdmType::ByteTypespec, false, vpiUIntConst, "16"),
        std::make_tuple("BinaryTest_0285", vpiLShiftOp,      UhdmType::ByteTypespec, false, vpiUIntConst,"1",    UhdmType::ByteTypespec, false, vpiUIntConst, "31", UhdmType::ByteTypespec, false, vpiUIntConst, "0"),
        std::make_tuple("BinaryTest_0286", vpiRShiftOp,      UhdmType::ByteTypespec, false, vpiUIntConst,"0",    UhdmType::ByteTypespec, false, vpiUIntConst, "0",  UhdmType::ByteTypespec, false, vpiUIntConst, "0"),

        //Updated LogicType to binary Format only
        std::make_tuple("BinaryTest_0011", vpiAddOp,         UhdmType::LogicTypespec, true,  vpiBinaryConst, "011111111",                        UhdmType::LogicTypespec, true,  vpiBinaryConst, "000000001",                        UhdmType::LogicTypespec, true,  vpiBinaryConst, "100000000"),
        std::make_tuple("BinaryTest_0025", vpiSubOp,         UhdmType::LogicTypespec, true,  vpiBinaryConst, "100000000",                        UhdmType::LogicTypespec, true,  vpiBinaryConst, "000000001",                        UhdmType::LogicTypespec, true,  vpiBinaryConst, "011111111"),
        std::make_tuple("BinaryTest_0038", vpiDivOp,         UhdmType::LogicTypespec, true,  vpiBinaryConst, "10000",                            UhdmType::LogicTypespec, true,  vpiBinaryConst, "00010",                            UhdmType::LogicTypespec, true,  vpiBinaryConst, "01000"),
        std::make_tuple("BinaryTest_0059", vpiMultOp,        UhdmType::LogicTypespec, true,  vpiBinaryConst, "0011",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "0100",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "1100"),
        std::make_tuple("BinaryTest_0068", vpiModOp,         UhdmType::LogicTypespec, true,  vpiBinaryConst, "11111111",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "00001010",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "00000101"),
        std::make_tuple("BinaryTest_0079", vpiPowerOp,       UhdmType::LogicTypespec, true,  vpiBinaryConst, "00010",                            UhdmType::LogicTypespec, true,  vpiBinaryConst, "00100",                            UhdmType::LogicTypespec, true,  vpiBinaryConst, "10000"),
        std::make_tuple("BinaryTest_0093", vpiEqOp,          UhdmType::LogicTypespec, true,  vpiBinaryConst, "10101010",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "10101010",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0094", vpiEqOp,          UhdmType::LogicTypespec, true,  vpiBinaryConst, "10101010",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "10101011",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0096", vpiEqOp,          UhdmType::LogicTypespec, true,  vpiBinaryConst, "10X1",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "10X1",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "X"),
        std::make_tuple("BinaryTest_0097", vpiEqOp,          UhdmType::LogicTypespec, true,  vpiBinaryConst, "10Z1",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "10Z1",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "X"),
        std::make_tuple("BinaryTest_0098", vpiEqOp,          UhdmType::LogicTypespec, true,  vpiBinaryConst, "10X1",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "1001",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "X"),
        std::make_tuple("BinaryTest_0099", vpiEqOp,          UhdmType::LogicTypespec, true,  vpiBinaryConst, "10Z1",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "1001",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "X"),
        std::make_tuple("BinaryTest_0106", vpiNeqOp,         UhdmType::LogicTypespec, true,  vpiBinaryConst, "11111111",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "11111110",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0108", vpiNeqOp,         UhdmType::LogicTypespec, true,  vpiBinaryConst, "10X1",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "10X1",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "X"),
        std::make_tuple("BinaryTest_0109", vpiNeqOp,         UhdmType::LogicTypespec, true,  vpiBinaryConst, "10Z1",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "10Z1",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "X"),
        std::make_tuple("BinaryTest_0110", vpiNeqOp,         UhdmType::LogicTypespec, true,  vpiBinaryConst, "10X1",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "1001",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "X"),
        std::make_tuple("BinaryTest_0111", vpiNeqOp,         UhdmType::LogicTypespec, true,  vpiBinaryConst, "10Z1",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "1001",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "X"),
        std::make_tuple("BinaryTest_0115", vpiGtOp,          UhdmType::LogicTypespec, true,  vpiBinaryConst, "11111111",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "11111110",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0122", vpiLtOp,          UhdmType::LogicTypespec, true,  vpiBinaryConst, "0001",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "1111",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0129", vpiGeOp,          UhdmType::LogicTypespec, true,  vpiBinaryConst, "11111111",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "11111111",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0135", vpiLeOp,          UhdmType::LogicTypespec, true,  vpiBinaryConst, "1111",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "1111",                             UhdmType::LogicTypespec, true,  vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0145", vpiLogAndOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "1111",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0001",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0146", vpiLogAndOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "0000",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0100",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0158", vpiLogOrOp,       UhdmType::LogicTypespec, false, vpiBinaryConst, "0000",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0100",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0159", vpiLogOrOp,       UhdmType::LogicTypespec, false, vpiBinaryConst, "0000",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0000",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0168", vpiBitAndOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "1010",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "1100",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "1000"),
        std::make_tuple("BinaryTest_0169", vpiBitAndOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "1x0z",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "1111",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "1x0x"),
        std::make_tuple("BinaryTest_0180", vpiBitOrOp,       UhdmType::LogicTypespec, false, vpiBinaryConst, "1010",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0101",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "1111"),
        std::make_tuple("BinaryTest_0180", vpiBitXorOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "1010",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "1100",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0110"),
        std::make_tuple("BinaryTest_0191", vpiBitXorOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "10xz",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0101",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "11xx"),
        std::make_tuple("BinaryTest_0199", vpiBitXnorOp,     UhdmType::LogicTypespec, false, vpiBinaryConst, "1010",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "1100",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "1001"),
        std::make_tuple("BinaryTest_0200", vpiBitXnorOp,     UhdmType::LogicTypespec, false, vpiBinaryConst, "10xz",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0101",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "00xx"),
        std::make_tuple("BinaryTest_0330", vpiLShiftOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "0001",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0001",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0010"),
        std::make_tuple("BinaryTest_0331", vpiLShiftOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "1x01",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0010",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0100"),
        std::make_tuple("BinaryTest_0332", vpiRShiftOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "11110000",                         UhdmType::LogicTypespec, false, vpiBinaryConst, "00000100",                         UhdmType::LogicTypespec, false, vpiBinaryConst, "00001111"),
        std::make_tuple("BinaryTest_0333", vpiArithRShiftOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "10000000",                         UhdmType::LogicTypespec, false, vpiBinaryConst, "00000001",                         UhdmType::LogicTypespec, false, vpiBinaryConst, "01000000"),
        std::make_tuple("BinaryTest_0334", vpiLShiftOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "11111111",                         UhdmType::LogicTypespec, false, vpiBinaryConst, "00001000",                         UhdmType::LogicTypespec, false, vpiBinaryConst, "00000000"),
        std::make_tuple("BinaryTest_0335", vpiRShiftOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "1010",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0010",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0010"),
        std::make_tuple("BinaryTest_0336", vpiLShiftOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "x",                                UhdmType::LogicTypespec, false, vpiBinaryConst, "1",                                UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0337", vpiArithLShiftOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "01100001",                         UhdmType::LogicTypespec, false, vpiBinaryConst, "00000011",                         UhdmType::LogicTypespec, false, vpiBinaryConst, "00001000"),
        std::make_tuple("BinaryTest_0338", vpiArithRShiftOp, UhdmType::LogicTypespec, true,  vpiBinaryConst, "11100000",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "00000011",                         UhdmType::LogicTypespec, true,  vpiBinaryConst, "00011100"),
        std::make_tuple("BinaryTest_0339", vpiRShiftOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "0001",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0001",                             UhdmType::LogicTypespec, false, vpiBinaryConst, "0000"),
        std::make_tuple("BinaryTest_0340", vpiLShiftOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "11111111",                         UhdmType::LogicTypespec, false, vpiBinaryConst, "00001000",                         UhdmType::LogicTypespec, false, vpiBinaryConst, "00000000"),
        std::make_tuple("BinaryTest_0341", vpiRShiftOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "00000001",                         UhdmType::LogicTypespec, false, vpiBinaryConst, "00001000",                         UhdmType::LogicTypespec, false, vpiBinaryConst, "00000000"),
        std::make_tuple("BinaryTest_0342", vpiLShiftOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "10000000000000000000000000000000", UhdmType::LogicTypespec, false, vpiBinaryConst, "00000000000000000000000000000001", UhdmType::LogicTypespec, false, vpiBinaryConst, "00000000000000000000000000000000"),
        std::make_tuple("BinaryTest_0343", vpiRShiftOp,      UhdmType::LogicTypespec, false, vpiBinaryConst, "10000000000000000000000000000000", UhdmType::LogicTypespec, false, vpiBinaryConst, "00000000000000000000000000011111", UhdmType::LogicTypespec, false, vpiBinaryConst, "00000000000000000000000000000001"),

        //-----------------------------------------------------------
        std::make_tuple("BinaryTest_0371", vpiUnaryNandOp, UhdmType::IntTypespec,      true, vpiIntConst, "6", UhdmType::IntTypespec,      true, vpiIntConst, "3", UhdmType::IntTypespec,      true, vpiIntConst, "-3"),
        std::make_tuple("BinaryTest_0372", vpiUnaryNorOp,  UhdmType::IntTypespec,      true, vpiIntConst, "6", UhdmType::IntTypespec,      true, vpiIntConst, "3", UhdmType::IntTypespec,      true, vpiIntConst, "-8"),
        std::make_tuple("BinaryTest_0373", vpiAddOp,       UhdmType::ByteTypespec,     true, vpiIntConst, "4", UhdmType::ByteTypespec,     true, vpiIntConst, "4", UhdmType::ByteTypespec,     true, vpiIntConst, "8"),
        std::make_tuple("BinaryTest_0374", vpiAddOp,       UhdmType::ShortIntTypespec, true, vpiIntConst, "4", UhdmType::ShortIntTypespec, true, vpiIntConst, "4", UhdmType::ShortIntTypespec, true, vpiIntConst, "8"),
        std::make_tuple("BinaryTest_0375", vpiAddOp,       UhdmType::LongIntTypespec,  true, vpiIntConst, "4", UhdmType::LongIntTypespec,  true, vpiIntConst, "4", UhdmType::LongIntTypespec,  true, vpiIntConst, "8"),

        // a -> b   logical imply: 1 -> 0 = 0
        std::make_tuple("BinaryTest_0375", vpiImplyOp, UhdmType::IntTypespec, false, vpiUIntConst, "1", UhdmType::IntTypespec, false, vpiUIntConst, "0", UhdmType::LogicTypespec, false, vpiUIntConst, "0"),

        // a |=> b (Non-overlap imply) : 1 |=> 1 = 1
        std::make_tuple("BinaryTest_0376", vpiNonOverlapImplyOp, UhdmType::IntTypespec, false, vpiUIntConst, "1", UhdmType::IntTypespec, false, vpiUIntConst, "1", UhdmType::LogicTypespec, false, vpiUIntConst, "1"),

        // overlap implication: 1 |-> 1 = 1
        std::make_tuple("BinaryTest_0377", vpiOverlapImplyOp, UhdmType::IntTypespec, false, vpiUIntConst, "1", UhdmType::IntTypespec, false, vpiUIntConst, "1", UhdmType::LogicTypespec, false, vpiUIntConst, "1"),

        //Single bit Testcases
        std::make_tuple("BinaryTest_0378", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0379", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0380", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0381", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0382", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0383", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0384", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0385", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0386", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0387", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0388", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0389", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0390", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0391", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0392", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0393", vpiBitAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0394", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0395", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0396", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0397", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0398", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0399", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0400", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0401", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0402", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0403", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0404", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0405", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0406", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0407", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0408", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0409", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0394", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0395", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0396", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0397", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0398", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0399", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0400", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0401", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0402", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0403", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0404", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0405", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0406", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0407", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0408", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0409", vpiBitOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0410", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0411", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0412", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0413", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0414", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0415", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0416", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0417", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0418", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0419", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0420", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0421", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0422", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0423", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0424", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0425", vpiBitXorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0426", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0427", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0428", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0429", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0430", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0431", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0432", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0433", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0434", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0435", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0436", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0437", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0438", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0439", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0440", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0441", vpiBitXNorOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0442", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0443", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0444", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0445", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0446", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0447", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0448", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0449", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0450", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0451", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0452", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0453", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0454", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0455", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0456", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0457", vpiLeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0458", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0459", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0460", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0461", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0462", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0463", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0464", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0465", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0466", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0467", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0468", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0469", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0470", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0471", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0472", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0473", vpiGeOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0474", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0475", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0476", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0477", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0478", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0479", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0480", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0481", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0482", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0483", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0484", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0485", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0486", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0487", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0488", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0489", vpiLtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0490", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0491", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0492", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0493", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0494", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0495", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0496", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0497", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0498", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0499", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0500", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0501", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0502", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0503", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0504", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0505", vpiGtOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0506", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0507", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0508", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0509", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0510", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0511", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0512", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0513", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0514", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0515", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0516", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0517", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0518", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0519", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0520", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0521", vpiEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0522", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0523", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0524", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0525", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0526", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0527", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0528", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0529", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0530", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0531", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0532", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0533", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0534", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0535", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0536", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0537", vpiCaseEqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),

        std::make_tuple("BinaryTest_0538", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0539", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0540", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0541", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0542", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0543", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0544", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0545", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0546", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0547", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0548", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0549", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0550", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0551", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0552", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0553", vpiNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0554", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0555", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0556", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0557", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0558", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0559", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0560", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0561", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0562", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0563", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0564", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0565", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0566", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0567", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0568", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0569", vpiCaseNeqOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),

        std::make_tuple("BinaryTest_0570", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0571", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0572", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0573", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0574", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0575", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0576", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0577", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0578", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0579", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0580", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0581", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0582", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0583", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0584", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0585", vpiAddOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0586", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0587", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0588", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0589", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0590", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0591", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0592", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0593", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0594", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0595", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0596", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0597", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0598", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0599", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0600", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0601", vpiSubOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0602", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0603", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0604", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0605", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0606", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0607", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0608", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0609", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0610", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0611", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0612", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0613", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0614", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0615", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0616", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0617", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0618", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0619", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0620", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0621", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0622", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0623", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0624", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0625", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0626", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0627", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0628", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0629", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0630", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0631", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0632", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0633", vpiMultOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0634", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0635", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0636", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0637", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0638", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0639", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0640", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0641", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0642", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0643", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0644", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0645", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0646", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0647", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0648", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0649", vpiLogAndOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0650", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "0"),
        std::make_tuple("BinaryTest_0651", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0652", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0653", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0654", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0655", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0656", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0657", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0658", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0659", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0660", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0661", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0662", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "0", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0663", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "1", UhdmType::LogicTypespec, false, vpiBinaryConst, "1"),
        std::make_tuple("BinaryTest_0664", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),
        std::make_tuple("BinaryTest_0665", vpiLogOrOp, UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "z", UhdmType::LogicTypespec, false, vpiBinaryConst, "x"),

        std::make_tuple("BinaryTest_0666", vpiAddOp, UhdmType::ShortRealTypespec, false, vpiRealConst, "1.5", UhdmType::LogicTypespec, false, vpiBinaryConst, "1x1z", UhdmType::ShortRealTypespec, true, vpiRealConst, "11.5")

        // Int Value is more than it can accomadate
        // std::make_tuple("BinaryTest_0225", vpiArithRShiftOp, UhdmType::IntTypespec,     true, vpiIntConst, "2147483648",          UhdmType::IntTypespec,     true, vpiIntConst, "1", UhdmType::IntTypespec,     true, vpiIntConst, "-1073741824"),
        // std::make_tuple("BinaryTest_0255", vpiArithRShiftOp, UhdmType::LongIntTypespec, true, vpiIntConst, "9223372036854775808", UhdmType::LongIntTypespec, true, vpiIntConst, "1", UhdmType::LongIntTypespec, true, vpiIntConst, "-4611686018427387904"),
        //
        // LogicTypespec
        // Reg
        // std::make_tuple("BinaryTest_0345", vpiLShiftOp,      UhdmType::Reg, false, vpiIntConst, "255",          UhdmType::Reg, false, vpiIntConst, "4", UhdmType::Reg, false, vpiIntConst, "240"),
        // std::make_tuple("BinaryTest_0344", vpiLShiftOp,      UhdmType::Reg, false, vpiIntConst, "1",            UhdmType::Reg, false, vpiIntConst, "1", UhdmType::Reg, false, vpiIntConst, "2"),
        // std::make_tuple("BinaryTest_0346", vpiLShiftOp,      UhdmType::Reg, false, vpiIntConst, "15",           UhdmType::Reg, false, vpiIntConst, "8", UhdmType::Reg, false, vpiIntConst, "0"),
        // std::make_tuple("BinaryTest_0347", vpiRShiftOp,      UhdmType::Reg, false, vpiIntConst, "128",          UhdmType::Reg, false, vpiIntConst, "1", UhdmType::Reg, false, vpiIntConst, "64"),
        // std::make_tuple("BinaryTest_0348", vpiArithRShiftOp, UhdmType::Reg, true,  vpiIntConst, "128",          UhdmType::Reg, true,  vpiIntConst, "1", UhdmType::Reg, true,  vpiIntConst, "192"),
        // std::make_tuple("BinaryTest_0349", vpiArithLShiftOp, UhdmType::Reg, false, vpiIntConst, "2",            UhdmType::Reg, false, vpiIntConst, "3", UhdmType::Reg, false, vpiIntConst, "16"),
        // std::make_tuple("BinaryTest_0350", vpiRShiftOp,      UhdmType::Reg, false, vpiIntConst, "8'b1x01_0000", UhdmType::Reg, false, vpiIntConst, "4", UhdmType::Reg, false, vpiIntConst, "x"),
        // std::make_tuple("BinaryTest_0351", vpiLShiftOp,      UhdmType::Reg, false, vpiIntConst, "1'bz",         UhdmType::Reg, false, vpiIntConst, "1", UhdmType::Reg, false, vpiIntConst, "x"),
        // std::make_tuple("BinaryTest_0352", vpiRShiftOp,      UhdmType::Reg, false, vpiIntConst, "255",          UhdmType::Reg, false, vpiIntConst, "8", UhdmType::Reg, false, vpiIntConst, "65280"),
        // std::make_tuple("BinaryTest_0353", vpiLShiftOp,      UhdmType::Reg, true,  vpiIntConst, "-1",           UhdmType::Reg, true,  vpiIntConst, "2", UhdmType::Reg, true,  vpiIntConst, "x"),
        // std::make_tuple("BinaryTest_0354", vpiArithLShiftOp, UhdmType::Reg, true,  vpiIntConst, "-1",           UhdmType::Reg, true,  vpiIntConst, "7", UhdmType::Reg, true,  vpiIntConst, "-128"),
        // std::make_tuple("BinaryTest_0355", vpiRShiftOp,      UhdmType::Reg, false, vpiIntConst, "0",            UhdmType::Reg, false, vpiIntConst, "5", UhdmType::Reg, false, vpiIntConst, "0"),
        // std::make_tuple("BinaryTest_0356", vpiArithRShiftOp, UhdmType::Reg, true,  vpiIntConst, "255",          UhdmType::Reg, true,  vpiIntConst, "4", UhdmType::Reg, true,  vpiIntConst, "255"),
        //
        // Net
        // std::make_tuple("BinaryTest_0357", vpiLShiftOp,      UhdmType::Net, false, vpiIntConst, "1",            UhdmType::Net, false, vpiIntConst, "1",  UhdmType::Net, false, vpiIntConst, "2"),
        // std::make_tuple("BinaryTest_0358", vpiLShiftOp,      UhdmType::Net, false, vpiIntConst, "255",          UhdmType::Net, false, vpiIntConst, "4",  UhdmType::Net, false, vpiIntConst, "240"),
        // std::make_tuple("BinaryTest_0359", vpiLShiftOp,      UhdmType::Net, false, vpiIntConst, "255",          UhdmType::Net, false, vpiIntConst, "8",  UhdmType::Net, false, vpiIntConst, "65280"),
        // std::make_tuple("BinaryTest_0360", vpiRShiftOp,      UhdmType::Net, false, vpiIntConst, "128",          UhdmType::Net, false, vpiIntConst, "1",  UhdmType::Net, false, vpiIntConst, "64"),
        // std::make_tuple("BinaryTest_0361", vpiArithRShiftOp, UhdmType::Net, true,  vpiIntConst, "128",          UhdmType::Net, true,  vpiIntConst, "1",  UhdmType::Net, true,  vpiIntConst, "192"),
        // std::make_tuple("BinaryTest_0362", vpiArithLShiftOp, UhdmType::Net, false, vpiIntConst, "2",            UhdmType::Net, false, vpiIntConst, "3",  UhdmType::Net, false, vpiIntConst, "16"),
        // std::make_tuple("BinaryTest_0363", vpiRShiftOp,      UhdmType::Net, false, vpiIntConst, "8'b1x01_0000", UhdmType::Net, false, vpiIntConst, "4",  UhdmType::Net, false, vpiIntConst, "x"),
        // std::make_tuple("BinaryTest_0364", vpiLShiftOp,      UhdmType::Net, false, vpiIntConst, "1'bz",         UhdmType::Net, false, vpiIntConst, "1",  UhdmType::Net, false, vpiIntConst, "x"),
        // std::make_tuple("BinaryTest_0365", vpiRShiftOp,      UhdmType::Net, false, vpiIntConst, "16711680",     UhdmType::Net, false, vpiIntConst, "8",  UhdmType::Net, false, vpiIntConst, "4278190080"),
        // std::make_tuple("BinaryTest_0366", vpiLShiftOp,      UhdmType::Net, false, vpiIntConst, "0",            UhdmType::Net, false, vpiIntConst, "10", UhdmType::Net, false, vpiIntConst, "0"),
        // std::make_tuple("BinaryTest_0367", vpiRShiftOp,      UhdmType::Net, false, vpiIntConst, "1",            UhdmType::Net, false, vpiIntConst, "-1", UhdmType::Net, false, vpiIntConst, "x"),
        // std::make_tuple("BinaryTest_0368", vpiArithRShiftOp, UhdmType::Net, true,  vpiIntConst, "-1",           UhdmType::Net, true,  vpiIntConst, "4",  UhdmType::Net, true,  vpiIntConst, "x"),
        // std::make_tuple("BinaryTest_0369", vpiLShiftOp,      UhdmType::Net, false, vpiIntConst, "1",            UhdmType::Net, false, vpiIntConst, "0",  UhdmType::Net, false, vpiIntConst, "1"),
        // std::make_tuple("BinaryTest_0370", vpiRShiftOp,      UhdmType::Net, false, vpiIntConst, "255",          UhdmType::Net, false, vpiIntConst, "8",  UhdmType::Net, false, vpiIntConst, "0"),
        //
        // std::make_tuple("BinaryTest_0147", vpiLogAndOp,  UhdmType::Reg, false, vpiIntConst, "255",  UhdmType::Reg, false, vpiIntConst, "1",    UhdmType::Reg, false, vpiIntConst, "1"),
        // std::make_tuple("BinaryTest_0148", vpiLogAndOp,  UhdmType::Net, false, vpiIntConst, "0",    UhdmType::Net, false, vpiIntConst, "170",  UhdmType::Net, false, vpiIntConst, "0"),
        // std::make_tuple("BinaryTest_0170", vpiBitAndOp,  UhdmType::Reg, false, vpiIntConst, "255",  UhdmType::Reg, false, vpiIntConst, "15",   UhdmType::Reg, false, vpiIntConst, "15"),
        // std::make_tuple("BinaryTest_0171", vpiBitAndOp,  UhdmType::Net, false, vpiIntConst, "170",  UhdmType::Net, false, vpiIntConst, "85",   UhdmType::Net, false, vpiIntConst, "0"),
        // std::make_tuple("BinaryTest_0172", vpiBitAndOp,  UhdmType::Net, false, vpiIntConst, "170",  UhdmType::Net, false, vpiIntConst, "85",   UhdmType::Net, false, vpiIntConst, "0"),
        // std::make_tuple("BinaryTest_0173", vpiBitAndOp,  UhdmType::Net, false, vpiIntConst, "1'bz", UhdmType::Net, false, vpiIntConst, "1'b1", UhdmType::Net, false, vpiIntConst, "x"),
        // std::make_tuple("BinaryTest_0160", vpiLogOrOp,   UhdmType::Reg, false, vpiIntConst, "128",  UhdmType::Reg, false, vpiIntConst, "0",    UhdmType::Reg, false, vpiIntConst, "1"),
        // std::make_tuple("BinaryTest_0161", vpiLogOrOp,   UhdmType::Net, false, vpiIntConst, "1'bz", UhdmType::Net, false, vpiIntConst, "1'bz", UhdmType::Net, false, vpiIntConst, "x"),
        // std::make_tuple("BinaryTest_0192", vpiBitXorOp,  UhdmType::Reg, false, vpiIntConst, "255",  UhdmType::Reg, false, vpiIntConst, "15",   UhdmType::Reg, false, vpiIntConst, "240"),
        // std::make_tuple("BinaryTest_0193", vpiBitXorOp,  UhdmType::Net, false, vpiIntConst, "1'bz", UhdmType::Net, false, vpiIntConst, "1'b1", UhdmType::Net, false, vpiIntConst, "x"),
        // std::make_tuple("BinaryTest_0201", vpiBitXnorOp, UhdmType::Reg, false, vpiIntConst, "255",  UhdmType::Reg, false, vpiIntConst, "15",   UhdmType::Reg, false, vpiIntConst, "15"),
        // std::make_tuple("BinaryTest_0202", vpiBitXnorOp, UhdmType::Net, false, vpiIntConst, "170",  UhdmType::Net, false, vpiIntConst, "85",   UhdmType::Net, false, vpiIntConst, "0"),
        // std::make_tuple("BinaryTest_0203", vpiBitXnorOp, UhdmType::Net, false, vpiIntConst, "1'bz", UhdmType::Net, false, vpiIntConst, "1'b0", UhdmType::Net, false, vpiIntConst, "x"),
        // std::make_tuple("BinaryTest_0209", vpiBitAndOp,  UhdmType::Net, false, vpiIntConst, "-1",   UhdmType::Net, false, vpiIntConst, "-2",   UhdmType::Net, false, vpiIntConst, "-2"),
        // std::make_tuple("BinaryTest_0211", vpiBitOrOp,   UhdmType::Reg, false, vpiIntConst, "-8",   UhdmType::Reg, false, vpiIntConst, "-1",   UhdmType::Reg, false, vpiIntConst, "-1"),
        //
        // TimeTypespec
        // std::make_tuple("BinaryTest_0288", vpiLShiftOp,      UhdmType::TimeTypespec, false, vpiTimeConst, "1ns",        UhdmType::TimeTypespec, false, vpiIntConst, "1",  UhdmType::TimeTypespec, false, vpiTimeConst, "2ns"),
        // std::make_tuple("BinaryTest_0287", vpiLShiftOp,      UhdmType::TimeTypespec, false, vpiTimeConst, "0",          UhdmType::TimeTypespec, false, vpiIntConst, "4",  UhdmType::TimeTypespec, false, vpiTimeConst, "0"),
        // std::make_tuple("BinaryTest_0289", vpiLShiftOp,      UhdmType::TimeTypespec, false, vpiTimeConst, "10us",       UhdmType::TimeTypespec, false, vpiIntConst, "8",  UhdmType::TimeTypespec, false, vpiTimeConst, "128us"),
        // std::make_tuple("BinaryTest_0290", vpiRShiftOp,      UhdmType::TimeTypespec, false, vpiTimeConst, "1000ps",     UhdmType::TimeTypespec, false, vpiIntConst, "3",  UhdmType::TimeTypespec, false, vpiTimeConst, "125ps"),
        // std::make_tuple("BinaryTest_0291", vpiLShiftOp,      UhdmType::TimeTypespec, false, vpiTimeConst, "1s",         UhdmType::TimeTypespec, false, vpiIntConst, "64", UhdmType::TimeTypespec, false, vpiTimeConst, "0"), // overshift -> 0
        // std::make_tuple("BinaryTest_0292", vpiRShiftOp,      UhdmType::TimeTypespec, false, vpiTimeConst, "5ns",        UhdmType::TimeTypespec, false, vpiIntConst, "-1", UhdmType::TimeTypespec, false, vpiTimeConst, "x"),
        // std::make_tuple("BinaryTest_0293", vpiArithRShiftOp, UhdmType::TimeTypespec, false, vpiTimeConst, "8ns",        UhdmType::TimeTypespec, false, vpiIntConst, "3",  UhdmType::TimeTypespec, false, vpiTimeConst, "1ns"),
        // std::make_tuple("BinaryTest_0294", vpiArithLShiftOp, UhdmType::TimeTypespec, false, vpiTimeConst, "2ns",        UhdmType::TimeTypespec, false, vpiIntConst, "2",  UhdmType::TimeTypespec, false, vpiTimeConst, "8ns"),
        // std::make_tuple("BinaryTest_0295", vpiLShiftOp,      UhdmType::TimeTypespec, false, vpiTimeConst, "4ps",        UhdmType::TimeTypespec, false, vpiIntConst, "2",  UhdmType::TimeTypespec, false, vpiTimeConst, "16ps"),
        // std::make_tuple("BinaryTest_0296", vpiRShiftOp,      UhdmType::TimeTypespec, false, vpiTimeConst, "0",          UhdmType::TimeTypespec, false, vpiIntConst, "7",  UhdmType::TimeTypespec, false, vpiTimeConst, "0"),
        // std::make_tuple("BinaryTest_0297", vpiLShiftOp,      UhdmType::TimeTypespec, false, vpiTimeConst, "1000000000", UhdmType::TimeTypespec, false, vpiIntConst, "-1", UhdmType::TimeTypespec, false, vpiTimeConst, "x"),
        // std::make_tuple("BinaryTest_0298", vpiArithRShiftOp, UhdmType::TimeTypespec, false, vpiTimeConst, "4294901760", UhdmType::TimeTypespec, false, vpiIntConst, "16", UhdmType::TimeTypespec, false, vpiTimeConst, "4294967295"),
        // std::make_tuple("BinaryTest_0299", vpiLShiftOp,      UhdmType::TimeTypespec, false, vpiTimeConst, "1",          UhdmType::TimeTypespec, false, vpiIntConst, "4",  UhdmType::TimeTypespec, false, vpiTimeConst, "16"),
        // std::make_tuple("BinaryTest_0300", vpiRShiftOp,      UhdmType::TimeTypespec, false, vpiTimeConst, "10ns",       UhdmType::TimeTypespec, false, vpiIntConst, "2",  UhdmType::TimeTypespec, false, vpiTimeConst, "2.5ns"),
        //
        // //---- depends upon simulator, some throw runtime error, X or 0
        // std::make_tuple("BinaryTest_0043", vpiDivOp, UhdmType::IntTypespec,       true,  vpiIntConst,  "10",            UhdmType::IntTypespec,       true,  vpiIntConst,  "0",   UhdmType::IntTypespec,       true,  vpiIntConst,  "DIV_BY_ZERO"),
        // std::make_tuple("BinaryTest_0044", vpiDivOp, UhdmType::IntTypespec,       false, vpiUIntConst, "10",            UhdmType::IntTypespec,       false, vpiUIntConst, "0",   UhdmType::IntTypespec,       false, vpiUIntConst, "DIV_BY_ZERO"),
        // std::make_tuple("BinaryTest_0045", vpiDivOp, UhdmType::LongIntTypespec,   true,  vpiIntConst,  "2147483647",    UhdmType::LongIntTypespec,   true,  vpiIntConst,  "0",   UhdmType::LongIntTypespec,   true,  vpiIntConst,  "DIV_BY_ZERO"),
        // std::make_tuple("BinaryTest_0046", vpiDivOp, UhdmType::IntegerTypespec,   true,  vpiIntConst,  "1000000000000", UhdmType::IntegerTypespec,   true,  vpiIntConst,  "0",   UhdmType::IntegerTypespec,   true,  vpiIntConst,  "DIV_BY_ZERO"),
        // std::make_tuple("BinaryTest_0047", vpiDivOp, UhdmType::LogicTypespec,     true,  vpiIntConst,  "16",            UhdmType::LogicTypespec,     true,  vpiIntConst,  "0",   UhdmType::LogicTypespec,     true,  vpiIntConst,  "DIV_BY_ZERO"),
        // std::make_tuple("BinaryTest_0048", vpiDivOp, UhdmType::RealTypespec,      true,  vpiRealConst, "10.0",          UhdmType::RealTypespec,      true,  vpiRealConst, "0.0", UhdmType::RealTypespec,      true,  vpiRealConst, "DIV_BY_ZERO"),
        // std::make_tuple("BinaryTest_0049", vpiDivOp, UhdmType::ShortRealTypespec, true,  vpiRealConst, "-3.5",          UhdmType::ShortRealTypespec, true,  vpiRealConst, "0.0", UhdmType::ShortRealTypespec, true,  vpiRealConst, "DIV_BY_ZERO"),
        // std::make_tuple("BinaryTest_0050", vpiDivOp, UhdmType::TimeTypespec,      false, vpiIntConst,  "1000",          UhdmType::TimeTypespec,      false, vpiIntConst,  "0",   UhdmType::TimeTypespec,      false, vpiIntConst,  "DIV_BY_ZERO")
        //
        //
        // // MOD BY ZERO CASES
        // std::make_tuple("BinaryTest_0072", vpiModOp, UhdmType::IntTypespec,  true,  vpiIntConst,  "10",  UhdmType::IntTypespec,  true,  vpiIntConst,  "0",   UhdmType::IntTypespec,  true,  vpiIntConst,  "DIV_BY_ZERO"),
        // std::make_tuple("BinaryTest_0073", vpiModOp, UhdmType::RealTypespec, true,  vpiRealConst, "5.5", UhdmType::RealTypespec, true,  vpiRealConst, "0.0", UhdmType::RealTypespec, true,  vpiRealConst, "DIV_BY_ZERO"),
        // std::make_tuple("BinaryTest_0074", vpiModOp, UhdmType::TimeTypespec, false, vpiIntConst,  "55",  UhdmType::TimeTypespec, false, vpiIntConst,  "0",   UhdmType::TimeTypespec, false, vpiIntConst,  "DIV_BY_ZERO")
        // clang-format on
        ));

TEST(ConcatOperator, ConcatOperatorTest) {
  TestObjectProvider provider;
  Serializer& m_serializer = provider.m_serializer;
  Constants& m_constants = provider.m_constants;
  ExprEval m_evaluator(&provider);

  RefObj* const ro1 = m_serializer.make<RefObj>();
  ro1->setName("a");

  RefObj* const ro2 = m_serializer.make<RefObj>();
  ro2->setName("b");

  Operation* const m_operation = m_serializer.make<Operation>();
  m_operation->setOpType(vpiConcatOp);

  AnyCollection* const operands = m_operation->getOperands(true);
  operands->emplace_back(ro1);
  operands->emplace_back(ro2);
  ro1->setParent(m_operation);
  ro2->setParent(m_operation);

  Constant* const ca = m_serializer.make<Constant>();
  Constant* const cb = m_serializer.make<Constant>();

  m_constants.emplace("a", ca);
  m_constants.emplace("b", cb);

  setTypespec(ca, UhdmType::LogicTypespec, false, "1010", m_serializer);
  setTypespec(cb, UhdmType::LogicTypespec, false, "0011", m_serializer);
  ca->setConstType(vpiBinaryConst);
  cb->setConstType(vpiBinaryConst);

  ca->setSize(24);
  cb->setSize(24);

  ro1->setActual(ca);
  ro2->setActual(cb);

  Expr* result = nullptr;
  const bool succeeded = m_evaluator.reduceExpr(m_operation, m_operation, &result, true);
  ASSERT_TRUE(succeeded);
  ASSERT_NE(result, nullptr);

  Constant* const c = any_cast<Constant>(result);
  ASSERT_NE(c, nullptr);

  ASSERT_EQ(c->getValue(), "000000000000000000001010000000000000000000000011");
  ASSERT_EQ(c->getConstType(), vpiBinaryConst);

  Typespec* const t = getTypespec(result);
  ASSERT_NE(t, nullptr);
  ASSERT_EQ(t->getUhdmType(), UhdmType::LogicTypespec);
  ASSERT_FALSE(getSigned(t));
}

TEST(CastOperator, CastIntToReal) {
  TestObjectProvider provider;
  Serializer& m_serializer = provider.m_serializer;
  Constants& m_constants = provider.m_constants;
  ExprEval m_evaluator(&provider);

  // source reference
  RefObj* const ro = m_serializer.make<RefObj>();
  ro->setName("a");

  Operation* const m_operation = m_serializer.make<Operation>();

  AnyCollection* const operands = m_operation->getOperands(true);
  operands->emplace_back(ro);
  ro->setParent(m_operation);

  Constant* const ca = m_serializer.make<Constant>();
  m_constants.emplace("a", ca);

  m_operation->setOpType(vpiCastOp);

  ca->setValue("10");
  setTypespec(ca, UhdmType::IntTypespec, true, "10", m_serializer);
  ca->setConstType(vpiDecConst);
  ca->setSize(32);

  ro->setActual(ca);

  {
    RefTypespec* const rt = m_serializer.make<RefTypespec>();
    m_operation->setTypespec(rt);

    RealTypespec* const ts = m_serializer.make<RealTypespec>();
    rt->setActual(ts);
  }

  Expr* result = nullptr;
  const bool succeeded = m_evaluator.reduceExpr(m_operation, m_operation, &result, true);

  ASSERT_TRUE(succeeded);
  ASSERT_NE(result, nullptr);

  Constant* const c = any_cast<Constant>(result);
  ASSERT_NE(c, nullptr);

  ASSERT_EQ(c->getValue(), "10");
  ASSERT_EQ(c->getConstType(), vpiRealConst);

  Typespec* const t = getTypespec(result);
  ASSERT_NE(t, nullptr);
  ASSERT_EQ(t->getUhdmType(), UhdmType::RealTypespec);
}

TEST(ReplicationOperator, ReplicationOperatorTest) {
  TestObjectProvider provider;
  Serializer& m_serializer = provider.m_serializer;
  Constants& m_constants = provider.m_constants;
  ExprEval m_evaluator(&provider);
  Operation* m_operation = nullptr;

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

  Constant* const ca = m_serializer.make<Constant>();
  Constant* const cb = m_serializer.make<Constant>();

  m_constants.emplace("a", ca);
  m_constants.emplace("b", cb);

  m_operation->setOpType(vpiMultiConcatOp);

  setTypespec(ca, UhdmType::IntTypespec, false, "2", m_serializer);
  setTypespec(cb, UhdmType::LogicTypespec, false, "0011", m_serializer);
  ca->setConstType(vpiIntConst);
  cb->setConstType(vpiBinaryConst);

  ca->setSize(32);
  cb->setSize(24);

  ro1->setActual(ca);
  ro2->setActual(cb);

  Expr* result = nullptr;
  const bool succeeded = m_evaluator.reduceExpr(m_operation, m_operation, &result, true);
  ASSERT_TRUE(succeeded);
  ASSERT_NE(result, nullptr);

  Constant* const c = any_cast<Constant>(result);
  ASSERT_NE(c, nullptr);
  ASSERT_EQ(c->getValue(), "000000000000000000000011000000000000000000000011");
  ASSERT_EQ(c->getConstType(), vpiBinaryConst);

  Typespec* const t = getTypespec(result);
  ASSERT_NE(t, nullptr);
  ASSERT_EQ(t->getUhdmType(), UhdmType::LogicTypespec);
  ASSERT_FALSE(getSigned(t));
}

TEST(ConditionalOperator, ConditionalOperatorTest) {
  TestObjectProvider provider;
  Serializer& m_serializer = provider.m_serializer;
  Constants& m_constants = provider.m_constants;
  ExprEval m_evaluator(&provider);
  Operation* m_operation = nullptr;

  // Condition "a"
  RefObj* const ro1 = m_serializer.make<RefObj>();
  ro1->setName("a");

  // True expression "b"
  RefObj* const ro2 = m_serializer.make<RefObj>();
  ro2->setName("b");

  // False expression "c"
  RefObj* const ro3 = m_serializer.make<RefObj>();
  ro3->setName("c");

  m_operation = m_serializer.make<Operation>();
  AnyCollection* const operands = m_operation->getOperands(true);
  operands->emplace_back(ro1);
  operands->emplace_back(ro2);
  operands->emplace_back(ro3);
  ro1->setParent(m_operation);
  ro2->setParent(m_operation);
  ro3->setParent(m_operation);

  Constant* const ca = m_serializer.make<Constant>();
  Constant* const cb = m_serializer.make<Constant>();
  Constant* const cc = m_serializer.make<Constant>();

  m_constants.emplace("a", ca);
  m_constants.emplace("b", cb);
  m_constants.emplace("c", cc);

  ro1->setActual(ca);
  ro2->setActual(cb);
  ro3->setActual(cc);

  m_operation->setOpType(vpiConditionOp);

  // Setup: condition = 1 (true)
  setTypespec(ca, UhdmType::IntTypespec, false, "1", m_serializer);
  ca->setConstType(vpiIntConst);
  ca->setSize(32);

  setTypespec(cb, UhdmType::LogicTypespec, false, "0101", m_serializer);
  cb->setConstType(vpiBinaryConst);
  cb->setSize(4);

  setTypespec(cc, UhdmType::LogicTypespec, false, "1111", m_serializer);
  cc->setConstType(vpiBinaryConst);
  cc->setSize(4);

  Expr* result = nullptr;
  const bool succeeded = m_evaluator.reduceExpr(m_operation, m_operation, &result, true);
  ASSERT_TRUE(succeeded);
  ASSERT_NE(result, nullptr);

  Constant* const c = any_cast<Constant>(result);
  ASSERT_NE(c, nullptr);
  ASSERT_EQ(c->getValue(), "0101");
  ASSERT_EQ(c->getConstType(), vpiBinaryConst);

  Typespec* const t = getTypespec(result);
  ASSERT_NE(t, nullptr);
  ASSERT_EQ(t->getUhdmType(), UhdmType::LogicTypespec);
  ASSERT_FALSE(getSigned(t));
}

TEST(CaseEqualityOperator, CaseEqualityTrueAndFalse) {
  TestObjectProvider provider;
  Serializer& m_serializer = provider.m_serializer;
  ExprEval m_evaluator(&provider);

  Constant* lhs1 = m_serializer.make<Constant>();
  setTypespec(lhs1, UhdmType::LogicTypespec, false, "1010", m_serializer);
  lhs1->setConstType(vpiBinaryConst);
  lhs1->setSize(4);

  Constant* rhs1 = m_serializer.make<Constant>();
  setTypespec(rhs1, UhdmType::LogicTypespec, false, "1010", m_serializer);
  rhs1->setConstType(vpiBinaryConst);
  rhs1->setSize(4);

  Operation* op1 = m_serializer.make<Operation>();
  op1->setOpType(vpiCaseEqOp);
  AnyCollection* ops1 = op1->getOperands(true);
  ops1->push_back(lhs1);
  ops1->push_back(rhs1);
  lhs1->setParent(op1);
  rhs1->setParent(op1);

  Expr* result1 = nullptr;
  const bool succeeded1 = m_evaluator.reduceExpr(op1, op1, &result1, true);
  ASSERT_TRUE(succeeded1);
  ASSERT_NE(result1, nullptr);

  Constant* const c1 = any_cast<Constant>(result1);
  ASSERT_NE(c1, nullptr);
  ASSERT_EQ(c1->getConstType(), vpiBinaryConst);
  ASSERT_EQ(c1->getValue(), "1");

  Constant* lhs2 = m_serializer.make<Constant>();
  setTypespec(lhs2, UhdmType::LogicTypespec, false, "1x010", m_serializer);
  lhs2->setConstType(vpiBinaryConst);
  lhs2->setSize(5);

  Constant* rhs2 = m_serializer.make<Constant>();
  setTypespec(rhs2, UhdmType::LogicTypespec, false, "10010", m_serializer);
  rhs2->setConstType(vpiBinaryConst);
  rhs2->setSize(5);

  Operation* op2 = m_serializer.make<Operation>();
  op2->setOpType(vpiCaseEqOp);
  AnyCollection* ops2 = op2->getOperands(true);
  ops2->push_back(lhs2);
  ops2->push_back(rhs2);
  lhs2->setParent(op2);
  rhs2->setParent(op2);

  Expr* result2 = nullptr;
  const bool succeeded2 = m_evaluator.reduceExpr(op2, op2, &result2, true);
  ASSERT_TRUE(succeeded2);
  ASSERT_NE(result2, nullptr);

  Constant* const c2 = any_cast<Constant>(result2);
  ASSERT_NE(c2, nullptr);
  ASSERT_EQ(c2->getConstType(), vpiBinaryConst);
  ASSERT_EQ(c2->getValue(), "0");
}

TEST(CaseInequalityOperator, CaseInequalityTrueAndFalse) {
  TestObjectProvider provider;
  Serializer& m_serializer = provider.m_serializer;
  ExprEval m_evaluator(&provider);

  Constant* lhs1 = m_serializer.make<Constant>();
  setTypespec(lhs1, UhdmType::LogicTypespec, false, "1010", m_serializer);
  lhs1->setConstType(vpiBinaryConst);
  lhs1->setSize(4);

  Constant* rhs1 = m_serializer.make<Constant>();
  setTypespec(rhs1, UhdmType::LogicTypespec, false, "1010", m_serializer);
  rhs1->setConstType(vpiBinaryConst);
  rhs1->setSize(4);

  Operation* op1 = m_serializer.make<Operation>();
  op1->setOpType(vpiCaseNeqOp);
  AnyCollection* ops1 = op1->getOperands(true);
  ops1->push_back(lhs1);
  ops1->push_back(rhs1);
  lhs1->setParent(op1);
  rhs1->setParent(op1);

  Expr* result1 = nullptr;
  const bool succeeded1 = m_evaluator.reduceExpr(op1, op1, &result1, true);
  ASSERT_TRUE(succeeded1);
  ASSERT_NE(result1, nullptr);

  Constant* const c1 = any_cast<Constant>(result1);
  ASSERT_NE(c1, nullptr);
  ASSERT_EQ(c1->getConstType(), vpiBinaryConst);
  ASSERT_EQ(c1->getValue(), "0");

  Constant* const lhs2 = m_serializer.make<Constant>();
  setTypespec(lhs2, UhdmType::LogicTypespec, false, "x1001", m_serializer);
  lhs2->setConstType(vpiBinaryConst);
  lhs2->setSize(5);

  Constant* const rhs2 = m_serializer.make<Constant>();
  setTypespec(rhs2, UhdmType::LogicTypespec, false, "10001", m_serializer);
  rhs2->setConstType(vpiBinaryConst);
  rhs2->setSize(5);

  Operation* const op2 = m_serializer.make<Operation>();
  op2->setOpType(vpiCaseNeqOp);

  AnyCollection* const ops2 = op2->getOperands(true);
  ops2->push_back(lhs2);
  ops2->push_back(rhs2);
  lhs2->setParent(op2);
  rhs2->setParent(op2);

  Expr* result2 = nullptr;
  const bool succeeded2 = m_evaluator.reduceExpr(op2, op2, &result2, true);
  ASSERT_TRUE(succeeded2);
  ASSERT_NE(result2, nullptr);

  Constant* const c2 = any_cast<Constant>(result2);
  ASSERT_NE(c2, nullptr);
  ASSERT_EQ(c2->getConstType(), vpiBinaryConst);
  ASSERT_EQ(c2->getValue(), "1");
}

TEST(ReplicationOperator, FillXSingleOperandTest) {
  TestObjectProvider provider;
  Serializer& m_serializer = provider.m_serializer;
  Constants& m_constants = provider.m_constants;
  ExprEval m_evaluator(&provider);
  Operation* m_operation = nullptr;

  RefObj* const ro1 = m_serializer.make<RefObj>();
  ro1->setName("a");

  m_operation = m_serializer.make<Operation>();
  AnyCollection* const operands = m_operation->getOperands(true);
  operands->emplace_back(ro1);
  ro1->setParent(m_operation);

  Constant* const ca = m_serializer.make<Constant>();
  ro1->setActual(ca);

  m_constants.emplace("a", ca);

  m_operation->setOpType(vpiMultiConcatOp);

  setTypespec(ca, UhdmType::LogicTypespec, false, "x", m_serializer);
  ca->setConstType(vpiBinaryConst);
  ca->setSize(8);

  Expr* result = nullptr;
  const bool succeeded = m_evaluator.reduceExpr(m_operation, m_operation, &result, true);
  ASSERT_TRUE(succeeded);
  ASSERT_NE(result, nullptr);

  Constant* const c = any_cast<Constant>(result);
  ASSERT_NE(c, nullptr);
  // Expect 8 X's
  ASSERT_EQ(c->getValue(), "xxxxxxxx");
  ASSERT_EQ(c->getConstType(), vpiBinaryConst);

  Typespec* const t = getTypespec(result);
  ASSERT_NE(t, nullptr);
  ASSERT_EQ(t->getUhdmType(), UhdmType::LogicTypespec);
  ASSERT_FALSE(getSigned(t));
}

TEST(DefaultTaggedPattern, DefaultZeroReplication) {
  TestObjectProvider provider;
  Serializer& serializer = provider.m_serializer;
  ExprEval evaluator(&provider);

  LogicTypespec* const packedTs = serializer.make<LogicTypespec>();
  packedTs->setSigned(false);

  Range* const packedRange = serializer.make<Range>();
  packedRange->setLeftExpr(makeConst("3", serializer));
  packedRange->setRightExpr(makeConst("0", serializer));
  RangeCollection* rc = serializer.makeCollection<Range>();
  rc->push_back(packedRange);
  packedTs->setRanges(rc);

  ArrayTypespec* const arrTs = serializer.make<ArrayTypespec>();

  RefTypespec* const ref = serializer.make<RefTypespec>();
  ref->setActual(packedTs);
  arrTs->setElemTypespec(ref);

  Range* const arrRange = serializer.make<Range>();
  arrRange->setLeftExpr(makeConst("3", serializer));
  arrRange->setRightExpr(makeConst("0", serializer));

  RangeCollection* const rc1 = serializer.makeCollection<Range>();
  rc1->push_back(arrRange);
  arrTs->setRanges(rc1);

  RefTypespec* const refTs = serializer.make<RefTypespec>();
  refTs->setActual(arrTs);

  Constant* const defExpr = serializer.make<Constant>();
  defExpr->setConstType(vpiBinaryConst);
  defExpr->setValue("0");
  defExpr->setSize(4);

  TaggedPattern* const tag = serializer.make<TaggedPattern>();
  tag->setPattern(defExpr);
  tag->setTypespec(refTs);

  std::vector<const Any*> result;
  const bool succeeded = evaluator.reduceTaggedPattern(tag, tag, &result);
  ASSERT_TRUE(succeeded);
  ASSERT_EQ(result.size(), 4);

  for (size_t i = 0; i < 4; i++) {
    auto* c = any_cast<Constant>(result[i]);
    ASSERT_NE(c, nullptr);
    ASSERT_EQ(c->getConstType(), vpiBinaryConst);
    ASSERT_EQ(c->getValue(), "0000");
    ASSERT_EQ(c->getSize(), 4);
  }
}

#else

INSTANTIATE_TEST_SUITE_P(UnaryOperators, UnaryOperationTest, testing::Values(
    // clang-format off
    std::make_tuple("UnaryTest_0186", vpiBitNegOp, UhdmType::LogicTypespec, false,  "1x0z",   UhdmType::LogicTypespec, false,  "0x1x")
    // clang-format on
    ));

INSTANTIATE_TEST_SUITE_P(BinaryOperators, BinaryOperationTest, testing::Values(
    // clang-format off
    std::make_tuple("BinaryTest_0666", vpiAddOp, UhdmType::ShortRealTypespec, false, vpiRealConst, "1.5", UhdmType::LogicTypespec, false, vpiBinaryConst, "1x1z", UhdmType::ShortRealTypespec, true, vpiRealConst, "11.5")
    // clang-format on
    ));
#endif

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
