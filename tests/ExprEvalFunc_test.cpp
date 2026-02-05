#include "gtest/gtest.h"
#include "uhdm/ExprEval.h"
#include "uhdm/NumUtils.h"
#include "uhdm/Utils.h"
#include "uhdm/uhdm.h"

using namespace uhdm;
using Constants = std::map<std::string_view, Constant *>;

struct MathArg final {
  std::string_view value;
  UhdmType typespecType;  // RealTypespec, LogicTypespec, IntTypespec, etc.
  bool isSigned = false;
};

using MathTestParam = std::tuple<std::string_view,
                                 std::string_view,      // "$sin"
                                 std::vector<MathArg>,  // arguments
                                 std::string_view,      // expected result value
                                 UhdmType,              // expected result typespec
                                 bool                   // expected result signedness (usually false)
                                 >;

class MathSysFuncTest : public testing::TestWithParam<MathTestParam>, public ObjectProvider {
 public:
  Serializer m_serializer;
  ExprEval m_evaluator;
  SysFuncCall *m_call = nullptr;
  std::vector<Constant *> m_args;

  MathSysFuncTest() : m_evaluator(this) {}

  void SetUp() override { m_call = m_serializer.make<SysFuncCall>(); }
  const Any *getObject(std::string_view, const Any *, const Any *, bool = false) final { return nullptr; }
  const TaskFunc *getTaskFunc(std::string_view, const Any *, const Any *, bool = false) final { return nullptr; }
  Any *getValue(std::string_view, const Any *, const Any *, bool = false) final { return nullptr; }

  void TearDown() override {
    m_serializer.purge();
    m_args.clear();
  }
};

static void attachTypespec(Constant *c, UhdmType tsType, bool isSigned, Serializer &s) {
  Typespec *ts = nullptr;

  switch (tsType) {
    case UhdmType::RealTypespec: ts = s.make<RealTypespec>(); break;
    case UhdmType::ShortRealTypespec: ts = s.make<ShortRealTypespec>(); break;
    case UhdmType::LogicTypespec: {
      auto *l = s.make<LogicTypespec>();
      l->setSigned(isSigned);
      ts = l;
      break;
    }
    case UhdmType::IntTypespec: {
      auto *i = s.make<IntTypespec>();
      i->setSigned(isSigned);
      ts = i;
      break;
    }
    case UhdmType::ByteTypespec: {
      auto *b = s.make<ByteTypespec>();
      b->setSigned(isSigned);
      ts = b;
      break;
    }
    default: break;
  }

  if (ts != nullptr) {
    RefTypespec *rt = s.make<RefTypespec>();
    rt->setActual(ts);
    c->setTypespec(rt);
  }
}

TEST_P(MathSysFuncTest, MathFunctions) {
  const auto &[testname, name, argsDesc, expectedValue, expectedTsType, expectedSigned] = GetParam();

  m_call->setName(name);
  AnyCollection *args = m_call->getArguments(true);

  for (const auto &arg : argsDesc) {
    Constant *c = m_serializer.make<Constant>();
    c->setValue(arg.value);
    c->setDecompile(arg.value);
    c->setSize(64);

    int32_t inputConstType = 0;
    if ((arg.typespecType == UhdmType::LogicTypespec) || (arg.typespecType == UhdmType::IntegerTypespec) ||
        (arg.typespecType == UhdmType::BitTypespec)) {
      inputConstType = vpiBinaryConst;
    } else if (arg.typespecType == UhdmType::RealTypespec) {
      inputConstType = vpiRealConst;
    } else if (arg.typespecType == UhdmType::ShortRealTypespec) {
      inputConstType = vpiRealConst;
    } else if (arg.typespecType == UhdmType::TimeTypespec) {
      inputConstType = vpiTimeConst;
    } else {
      inputConstType = vpiDecConst;
    }
    c->setConstType(inputConstType);

    attachTypespec(c, arg.typespecType, arg.isSigned, m_serializer);

    args->push_back(c);
  }

  Expr *result = nullptr;
  bool succeeded = m_evaluator.reduceExpr(m_call, m_call, &result, true);

  ASSERT_TRUE(succeeded);
  ASSERT_NE(result, nullptr);

  Constant *const c = any_cast<Constant>(result);
  ASSERT_NE(c, nullptr);

  double val0 = 0;
  if (!NumUtils::parseDouble(c->getDecompile(), &val0)) val0 = 0;

  double val1 = 0;
  if (!NumUtils::parseDouble(expectedValue, &val1)) val1 = 0;

  ASSERT_EQ(c->getConstType(), vpiRealConst);
  ASSERT_DOUBLE_EQ(val0, val1);

  Typespec *const t = getTypespec(result);
  ASSERT_NE(t, nullptr);
  ASSERT_EQ(t->getUhdmType(), expectedTsType);
  ASSERT_EQ(getSigned(t), expectedSigned);
}

struct ConvArg {
  std::string_view value;
  UhdmType typespecType;
  bool isSigned;
};

using ConvArgs = std::vector<ConvArg>;

using ConvTestParam = std::tuple<std::string_view,  // test name
                                 std::string_view,  // "$itor"
                                 ConvArgs,          // single argument
                                 std::string_view,  // expected value
                                 UhdmType,          // expected typespec
                                 bool>;             // expected signedness

class ConvSysFuncTest : public testing::TestWithParam<ConvTestParam>, public ObjectProvider {
 public:
  Serializer m_serializer;
  ExprEval m_evaluator;
  SysFuncCall *m_call = nullptr;

  ConvSysFuncTest() : m_evaluator(this) {}

  void SetUp() override { m_call = m_serializer.make<SysFuncCall>(); }

  void TearDown() override { m_serializer.purge(); }

  const Any *getObject(std::string_view, const Any *, const Any *, bool) final { return nullptr; }
  const TaskFunc *getTaskFunc(std::string_view, const Any *, const Any *, bool) final { return nullptr; }
  Any *getValue(std::string_view, const Any *, const Any *, bool) final { return nullptr; }
};

TEST_P(ConvSysFuncTest, ConversionFunctions) {
  const auto &[testname, name, argsDesc, expectedValue, expectedTsType, expectedSigned] = GetParam();

  m_call->setName(name);
  AnyCollection *args = m_call->getArguments(true);

  int32_t inputConstType = vpiDecConst;
  for (const auto &argDesc : argsDesc) {
    Constant *c = m_serializer.make<Constant>();
    c->setValue(argDesc.value);
    c->setDecompile(argDesc.value);
    c->setSize(64);

    if (argDesc.typespecType == UhdmType::LogicTypespec || argDesc.typespecType == UhdmType::IntegerTypespec ||
        argDesc.typespecType == UhdmType::BitTypespec) {
      inputConstType = vpiBinaryConst;
      c->setSize(argDesc.value.size());
    } else if (argDesc.typespecType == UhdmType::RealTypespec || argDesc.typespecType == UhdmType::ShortRealTypespec) {
      inputConstType = vpiRealConst;
    } else if (argDesc.typespecType == UhdmType::TimeTypespec) {
      inputConstType = vpiTimeConst;
    } else {
      inputConstType = vpiDecConst;
    }

    c->setConstType(inputConstType);
    attachTypespec(c, argDesc.typespecType, argDesc.isSigned, m_serializer);
    args->push_back(c);
  }

  Expr *result = nullptr;
  ASSERT_TRUE(m_evaluator.reduceExpr(m_call, m_call, &result, true));

  Constant *rc = any_cast<Constant>(result);
  ASSERT_NE(rc, nullptr);

  int32_t expectedConstType = vpiDecConst;

  if ((name == "$itor") || (name == "$bitstoreal") || (name == "$bitstoshortreal")) {
    expectedConstType = vpiRealConst;
  } else if ((name == "$rtoi") || (name == "$realtobits") || (name == "$shortrealtobits")) {
    expectedConstType = vpiIntConst;
  } else if (name == "$cast") {
    if (expectedTsType == UhdmType::RealTypespec || expectedTsType == UhdmType::ShortRealTypespec) {
      expectedConstType = vpiRealConst;
    } else {
      expectedConstType = vpiIntConst;
    }
  } else if ((name == "$signed") || (name == "$unsigned")) {
    expectedConstType = vpiIntConst;
    if (inputConstType == vpiBinaryConst) {
      expectedConstType = vpiBinaryConst;
    }
  }

  ASSERT_EQ(rc->getConstType(), expectedConstType);

  /*------------------ typespec & signedness ------------------*/
  Typespec *ts = getTypespec(rc);
  ASSERT_NE(ts, nullptr);
  ASSERT_EQ(ts->getUhdmType(), expectedTsType);
  ASSERT_EQ(getSigned(ts), expectedSigned);

  /*------------------ numeric value check ------------------*/
  double v0 = 0, v1 = 0;
  if (!NumUtils::parseDouble(rc->getDecompile(), &v0)) v0 = 0;
  if (!NumUtils::parseDouble(expectedValue, &v1)) v1 = 0;
  ASSERT_DOUBLE_EQ(v0, v1);
}

struct DataQueryArg {
  UhdmType baseType;
  bool isSigned = false;
  std::vector<std::pair<int32_t, int32_t>> packedRanges;
  std::vector<std::pair<int32_t, int32_t>> unpackedRanges;
  int32_t arrayType = vpiStaticArray;
};

using DataQueryTestParam = std::tuple<std::string_view,  // test name
                                      std::string_view,  // system function name
                                      DataQueryArg,      // type description
                                      std::string_view,  // expected value
                                      UhdmType,          // expected typespec type
                                      int32_t            // expected const type
                                      >;

static Expr *makeIntConst(Serializer &s, int64_t v) {
  auto *c = s.make<Constant>();
  c->setValue(std::to_string(v));
  c->setDecompile(std::to_string(v));
  c->setConstType(vpiIntConst);
  c->setSize(32);

  // Attach IntTypespec
  auto *ts = s.make<IntTypespec>();
  ts->setSigned(true);
  auto *ref = s.make<RefTypespec>();
  ref->setActual(ts);
  c->setTypespec(ref);

  return c;
}
static Typespec *buildTypespec(Serializer &s, const DataQueryArg &arg) {
  Typespec *base = nullptr;

  switch (arg.baseType) {
    case UhdmType::IntTypespec: {
      auto *t = s.make<IntTypespec>();
      t->setSigned(arg.isSigned);
      base = t;
    } break;

    case UhdmType::LogicTypespec: {
      auto *t = s.make<LogicTypespec>();
      t->setSigned(arg.isSigned);
      base = t;
    } break;

    case UhdmType::ByteTypespec: {
      auto *t = s.make<ByteTypespec>();
      t->setSigned(arg.isSigned);
      base = t;
    } break;

    case UhdmType::LongIntTypespec: {
      auto *t = s.make<LongIntTypespec>();
      t->setSigned(arg.isSigned);
      base = t;
    } break;

    default: return nullptr;
  }

  if (!arg.packedRanges.empty()) {
    if (base->getUhdmType() == UhdmType::LogicTypespec) {
      auto *lt = static_cast<LogicTypespec *>(base);

      auto *ranges = s.makeCollection<Range>();
      for (auto &[l, r] : arg.packedRanges) {
        auto *range = s.make<Range>();
        range->setLeftExpr(makeIntConst(s, l));
        range->setRightExpr(makeIntConst(s, r));
        ranges->push_back(range);
      }
      lt->setRanges(ranges);
    }
  }

  if (!arg.unpackedRanges.empty()) {
    auto *arr = s.make<ArrayTypespec>();
    arr->setArrayType(arg.arrayType);

    for (auto &[l, r] : arg.unpackedRanges) {
      if (arg.arrayType == vpiStaticArray) {
        auto *ranges = arr->getRanges(true);
        auto *range = s.make<Range>();
        range->setLeftExpr(makeIntConst(s, l));
        range->setRightExpr(makeIntConst(s, r));
        ranges->push_back(range);
      }
    }
    auto *rt = s.make<RefTypespec>();
    rt->setActual(base);
    arr->setElemTypespec(rt);
    base = arr;
  }

  return base;
}

static Expr *makeTypedExpr(Serializer &s, Typespec *ts) {
  auto *c = s.make<Constant>();
  c->setValue("0");
  c->setDecompile("0");
  c->setConstType(vpiIntConst);
  c->setSize(32);

  auto *rt = s.make<RefTypespec>();
  rt->setActual(ts);
  c->setTypespec(rt);

  return c;
}

class DataQuerySysFuncTest : public testing::TestWithParam<DataQueryTestParam>, public ObjectProvider {
 public:
  Serializer m_serializer;
  ExprEval m_evaluator;
  SysFuncCall *m_call = nullptr;

  DataQuerySysFuncTest() : m_evaluator(this) {}

  void SetUp() override { m_call = m_serializer.make<SysFuncCall>(); }

  void TearDown() override { m_serializer.purge(); }

  const Any *getObject(std::string_view, const Any *, const Any *, bool) final { return nullptr; }
  const TaskFunc *getTaskFunc(std::string_view, const Any *, const Any *, bool) final { return nullptr; }
  Any *getValue(std::string_view, const Any *, const Any *, bool) final { return nullptr; }
};

TEST_P(DataQuerySysFuncTest, DataQueryFunctions) {
  const auto &[testName, funcName, arg, expectedValue, expectedTsType, expectedConstType] = GetParam();

  m_call->setName(std::string(funcName));
  AnyCollection *args = m_call->getArguments(true);

  Typespec *ts = buildTypespec(m_serializer, arg);
  ASSERT_NE(ts, nullptr);

  Expr *expr = makeTypedExpr(m_serializer, ts);
  args->push_back(expr);

  Expr *result = nullptr;
  ASSERT_TRUE(m_evaluator.reduceExpr(m_call, m_call, &result, true));

  Constant *rc = any_cast<Constant>(result);
  ASSERT_NE(rc, nullptr);

  ASSERT_EQ(rc->getConstType(), expectedConstType);

  EXPECT_EQ(rc->getDecompile(), expectedValue);

  Typespec *rts = getTypespec(rc);
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getUhdmType(), expectedTsType);
}

struct ArrayQueryArg : public DataQueryArg {
  int32_t dim = 1;
};

using ArrayQueryTestParam = std::tuple<std::string_view,  // test name
                                       std::string_view,  // system function
                                       ArrayQueryArg,     // type description
                                       std::string_view,  // expected value
                                       UhdmType,          // expected typespec
                                       int32_t            // expected const type
                                       >;

class ArrayQuerySysFuncTest : public testing::TestWithParam<ArrayQueryTestParam>, public ObjectProvider {
 public:
  Serializer m_serializer;
  ExprEval m_evaluator;
  SysFuncCall *m_call = nullptr;

  ArrayQuerySysFuncTest() : m_evaluator(this) {}

  void SetUp() override { m_call = m_serializer.make<SysFuncCall>(); }
  void TearDown() override { m_serializer.purge(); }

  const Any *getObject(std::string_view, const Any *, const Any *, bool) final { return nullptr; }
  const TaskFunc *getTaskFunc(std::string_view, const Any *, const Any *, bool) final { return nullptr; }
  Any *getValue(std::string_view, const Any *, const Any *, bool) final { return nullptr; }
};

TEST_P(ArrayQuerySysFuncTest, ArrayQueryFunctions) {
  const auto &[testName, funcName, arg, expectedValue, expectedTs, expectedConstType] = GetParam();

  m_call->setName(std::string(funcName));
  AnyCollection *args = m_call->getArguments(true);

  Typespec *ts = buildTypespec(m_serializer, arg);
  ASSERT_NE(ts, nullptr);

  args->push_back(makeTypedExpr(m_serializer, ts));

  if (funcName != "$dimensions" && funcName != "$unpacked_dimensions") {
    args->push_back(makeIntConst(m_serializer, arg.dim));
  }

  Expr *result = nullptr;
  ASSERT_TRUE(m_evaluator.reduceExpr(m_call, m_call, &result, true));

  auto *c = any_cast<Constant>(result);
  ASSERT_NE(c, nullptr);

  EXPECT_EQ(c->getConstType(), expectedConstType);
  EXPECT_EQ(c->getDecompile(), expectedValue);

  Typespec *rts = getTypespec(c);
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getUhdmType(), expectedTs);
}

struct BitVecSysFuncParam {
  const char *testName;
  const char *sysFunc;

  const char *argValue;
  int32_t argConstType;

  const char *maskValue;

  const char *expectedValue;
  int32_t expectedConstType;
};

class BitVectorSysFuncTest : public ::testing::TestWithParam<BitVecSysFuncParam>, public ObjectProvider {
 public:
  Serializer m_serializer;
  ExprEval m_evaluator;
  SysFuncCall *m_call = nullptr;

  BitVectorSysFuncTest() : m_evaluator(this) {}

  void SetUp() override { m_call = m_serializer.make<SysFuncCall>(); }
  void TearDown() override { m_serializer.purge(); }

  const Any *getObject(std::string_view, const Any *, const Any *, bool) final { return nullptr; }
  const TaskFunc *getTaskFunc(std::string_view, const Any *, const Any *, bool) final { return nullptr; }
  Any *getValue(std::string_view, const Any *, const Any *, bool) final { return nullptr; }

  static void attachLogicTypespec(Constant *c, Serializer &s) {
    auto *lts = s.make<LogicTypespec>();
    lts->setSigned(false);

    auto *rt = s.make<RefTypespec>();
    rt->setActual(lts);
    c->setTypespec(rt);
  }
};

TEST_P(BitVectorSysFuncTest, BitVectorFunctions) {
  const auto &param = GetParam();

  m_call->setName(param.sysFunc);
  AnyCollection *args = m_call->getArguments(true);

  Constant *arg0 = m_serializer.make<Constant>();
  arg0->setConstType(param.argConstType);
  arg0->setValue(param.argValue);
  arg0->setDecompile(param.argValue);
  attachLogicTypespec(arg0, m_serializer);
  args->push_back(arg0);

  if (std::string_view(param.sysFunc) == "$countbits") {
    ASSERT_NE(param.maskValue, nullptr);

    Constant *mask = m_serializer.make<Constant>();
    mask->setConstType(vpiBinaryConst);
    mask->setValue(param.maskValue);
    mask->setDecompile(param.maskValue);
    attachLogicTypespec(mask, m_serializer);
    args->push_back(mask);
  }

  Expr *result = nullptr;
  ASSERT_TRUE(m_evaluator.reduceExpr(m_call, m_call, &result, true)) << param.testName;

  auto *c = any_cast<Constant *>(result);
  ASSERT_NE(c, nullptr) << param.testName;

  EXPECT_EQ(c->getConstType(), param.expectedConstType) << param.testName;
  EXPECT_EQ(std::string(c->getDecompile()), std::string(param.expectedValue)) << param.testName;
}

#if 1
INSTANTIATE_TEST_SUITE_P(
    BitVectorSysFuncs, BitVectorSysFuncTest,
    ::testing::Values(
        // clang-format off
        // -------- $countones --------
        BitVecSysFuncParam{"countones_basic", "$countones", "101010", vpiBinaryConst, nullptr, "3", vpiIntConst},
        BitVecSysFuncParam{"countones_xz",    "$countones", "10xz1",  vpiBinaryConst, nullptr, "2", vpiIntConst},

        // -------- $isunknown --------
        BitVecSysFuncParam{"isunknown_no",  "$isunknown", "1010", vpiBinaryConst, nullptr, "0", vpiIntConst},
        BitVecSysFuncParam{"isunknown_yes", "$isunknown", "10x0", vpiBinaryConst, nullptr, "1", vpiIntConst},

        // -------- $onehot --------
        BitVecSysFuncParam{"onehot_true",        "$onehot", "001000", vpiBinaryConst, nullptr, "1", vpiIntConst},
        BitVecSysFuncParam{"onehot_false_multi", "$onehot", "001100", vpiBinaryConst, nullptr, "0", vpiIntConst},
        BitVecSysFuncParam{"onehot_false_x",     "$onehot", "00x000", vpiBinaryConst, nullptr, "0", vpiIntConst},

        // -------- $onehot0 --------
        BitVecSysFuncParam{"onehot0_zero",  "$onehot0", "000000", vpiBinaryConst, nullptr, "1", vpiIntConst},
        BitVecSysFuncParam{"onehot0_one",   "$onehot0", "000100", vpiBinaryConst, nullptr, "1", vpiIntConst},
        BitVecSysFuncParam{"onehot0_multi", "$onehot0", "001100", vpiBinaryConst, nullptr, "0", vpiIntConst},

        // -------- $countbits --------
        BitVecSysFuncParam{"countbits_ones",  "$countbits", "10101", vpiBinaryConst, "1", "3", vpiIntConst},
        BitVecSysFuncParam{"countbits_zeros", "$countbits", "10101", vpiBinaryConst, "0", "2", vpiIntConst},
        BitVecSysFuncParam{"countbits_x",     "$countbits", "10xzx", vpiBinaryConst, "x", "2", vpiIntConst},
        BitVecSysFuncParam{"countbits_z",     "$countbits", "10xzz", vpiBinaryConst, "z", "2", vpiIntConst}
        // clang-format on
    ));

INSTANTIATE_TEST_SUITE_P(
    ArrayQuery20_7, ArrayQuerySysFuncTest,
    ::testing::Values(
        // clang-format off
        // $dimensions
        ArrayQueryTestParam{"AQ_Dimensions_Scalar",         "$dimensions", { UhdmType::LogicTypespec, false, {}, {} },                    "0", UhdmType::IntTypespec, vpiIntConst},
        ArrayQueryTestParam{"AQ_Dimensions_Packed2D",       "$dimensions", { UhdmType::LogicTypespec, false, {{7, 0}, {3, 0}}, {} },      "2", UhdmType::IntTypespec, vpiIntConst},
        ArrayQueryTestParam{"AQ_Dimensions_PackedUnpacked", "$dimensions", { UhdmType::LogicTypespec, false, {{7, 0}}, {{3, 0},{1, 0}} }, "3", UhdmType::IntTypespec, vpiIntConst},

        // $unpacked_dimensions
        ArrayQueryTestParam{"AQ_Unpacked_None", "$unpacked_dimensions", { UhdmType::LogicTypespec, false, {{7, 0}}, {} },         "0", UhdmType::IntTypespec, vpiIntConst},
        ArrayQueryTestParam{"AQ_Unpacked_1D",   "$unpacked_dimensions", { UhdmType::LogicTypespec, false, {}, {{3, 0}} },         "1", UhdmType::IntTypespec, vpiIntConst},
        ArrayQueryTestParam{"AQ_Unpacked_2D",   "$unpacked_dimensions", { UhdmType::LogicTypespec, false, {}, {{3, 0}, {1, 0}} }, "2", UhdmType::IntTypespec, vpiIntConst},

        // $left
        ArrayQueryTestParam{"AQ_Left_Packed",   "$left", { UhdmType::LogicTypespec, false, {{7, 0}}, {}, 1 }, "7", UhdmType::IntTypespec, vpiIntConst},
        ArrayQueryTestParam{"AQ_Left_Unpacked", "$left", { UhdmType::LogicTypespec, false, {}, {{3, 0}}, 1 }, "3", UhdmType::IntTypespec, vpiIntConst},

        // $right
        ArrayQueryTestParam{"AQ_Right_Packed", "$right", { UhdmType::LogicTypespec, false, {{7, 0}}, {}, 1 }, "0", UhdmType::IntTypespec, vpiIntConst},

        // $low / $high
        ArrayQueryTestParam{"AQ_Low_Descending",  "$low",  { UhdmType::LogicTypespec, false, {{7, 0}}, {}, 1 }, "0", UhdmType::IntTypespec, vpiIntConst},
        ArrayQueryTestParam{"AQ_High_Descending", "$high", { UhdmType::LogicTypespec, false, {{7, 0}}, {}, 1 }, "7", UhdmType::IntTypespec, vpiIntConst},

        // $increment
        ArrayQueryTestParam{"AQ_Increment_Down", "$increment", { UhdmType::LogicTypespec, false, {{7,0}}, {}, 1 }, "-1", UhdmType::IntTypespec, vpiIntConst},
        ArrayQueryTestParam{"AQ_Increment_Up",   "$increment", { UhdmType::LogicTypespec, false, {{0,7}}, {}, 1 }, "1",  UhdmType::IntTypespec, vpiIntConst},

         // ---------------- $size ----------------
        ArrayQueryTestParam{"AQ_Size_Packed2D_Dim0",       "$size", { UhdmType::LogicTypespec, false, {{7, 0}, {3, 0}}, {}, {1} }, "8", UhdmType::IntTypespec, vpiIntConst},

        ArrayQueryTestParam{"AQ_Size_Packed2D_Dim1",       "$size", { UhdmType::LogicTypespec, false, {{7, 0}, {3, 0}}, {},       vpiStaticArray, 2 },   "4", UhdmType::IntTypespec, vpiIntConst},
        ArrayQueryTestParam{"AQ_Size_PackedUnpacked_Dim0", "$size", { UhdmType::LogicTypespec, false, {{7, 0}}, {{3, 0}, {1, 0}}, vpiStaticArray, {1} }, "4", UhdmType::IntTypespec, vpiIntConst},
        ArrayQueryTestParam{"AQ_Size_PackedUnpacked_Dim1", "$size", { UhdmType::LogicTypespec, false, {{7, 0}}, {{3, 0}, {1, 0}}, vpiStaticArray, {2} }, "2", UhdmType::IntTypespec, vpiIntConst},
        ArrayQueryTestParam{"AQ_Size_PackedUnpacked_Dim2", "$size", { UhdmType::LogicTypespec, false, {{7, 0}}, {{3, 0}, {1, 0}}, vpiStaticArray, 3 },   "8", UhdmType::IntTypespec, vpiIntConst}
        // clang-format on
    ));

INSTANTIATE_TEST_SUITE_P(
    DataQuery20_6, DataQuerySysFuncTest,
    ::testing::Values(
        // clang-format off
        DataQueryTestParam{"DQ_Bits_Byte",          "$bits", { UhdmType::ByteTypespec,  false, {}, {},             vpiStaticArray }, "8",  UhdmType::IntTypespec, vpiIntConst},
        DataQueryTestParam{"DQ_Bits_Int",           "$bits", { UhdmType::IntTypespec,   true,  {}, {},             vpiStaticArray }, "32", UhdmType::IntTypespec, vpiIntConst},
        DataQueryTestParam{"DQ_Bits_LogicScalar",   "$bits", { UhdmType::LogicTypespec, false, {}, {},             vpiStaticArray }, "1",  UhdmType::IntTypespec, vpiIntConst},
        DataQueryTestParam{"DQ_Bits_LogicPacked",   "$bits", { UhdmType::LogicTypespec, false, {{7,0}}, {},        vpiStaticArray }, "8",  UhdmType::IntTypespec, vpiIntConst},
        DataQueryTestParam{"DQ_Bits_LogicPacked2D", "$bits", { UhdmType::LogicTypespec, false, {{7,0}, {3,0}}, {}, vpiStaticArray }, "32", UhdmType::IntTypespec, vpiIntConst},

        DataQueryTestParam{"DQ_TypeName_Int",         "$typename", { UhdmType::IntTypespec,   true,  {},      {}, vpiStaticArray }, "int",         UhdmType::StringTypespec, vpiStringConst},
        DataQueryTestParam{"DQ_TypeName_LogicPacked", "$typename", { UhdmType::LogicTypespec, false, {{7,0}}, {}, vpiStaticArray }, "logic [7:0]", UhdmType::StringTypespec, vpiStringConst},

        DataQueryTestParam{"DQ_IsUnbounded_Int",               "$isunbounded", { UhdmType::IntTypespec,   false, {}, {},             vpiStaticArray }, "0",UhdmType::IntTypespec,vpiIntConst},
        DataQueryTestParam{"DQ_IsUnbounded_Logic",             "$isunbounded", { UhdmType::LogicTypespec, false, {}, {},             vpiStaticArray }, "0",UhdmType::IntTypespec,vpiIntConst},
        DataQueryTestParam{"DQ_IsUnbounded_FixedArray1D",      "$isunbounded", { UhdmType::LogicTypespec, false, {}, {{3,0}},        vpiStaticArray }, "0",UhdmType::IntTypespec,vpiIntConst},
        DataQueryTestParam{"DQ_IsUnbounded_FixedArray2D",      "$isunbounded", { UhdmType::LogicTypespec, false, {}, {{3,0}, {1,0}}, vpiStaticArray }, "0",UhdmType::IntTypespec,vpiIntConst},
        DataQueryTestParam{"DQ_IsUnbounded_PackedAndUnpacked", "$isunbounded", { UhdmType::LogicTypespec, false, {{7,0}}, {{3,0}},   vpiStaticArray }, "0",UhdmType::IntTypespec,vpiIntConst},

        DataQueryTestParam{"DQ_IsUnbounded_DynamicArray", "$isunbounded", { UhdmType::LogicTypespec, false, {}, {{0,0}}, vpiDynamicArray }, "1", UhdmType::IntTypespec, vpiIntConst},
        DataQueryTestParam{"DQ_IsUnbounded_Queue",        "$isunbounded", { UhdmType::LogicTypespec, false, {}, {{0,0}}, vpiQueueArray },   "1", UhdmType::IntTypespec, vpiIntConst},
        DataQueryTestParam{"DQ_IsUnbounded_Assoc",        "$isunbounded", { UhdmType::LogicTypespec, false, {}, {{0,0}}, vpiAssocArray },   "1", UhdmType::IntTypespec, vpiIntConst}
        // clang-format off
    ));

INSTANTIATE_TEST_SUITE_P(
    MathFunctions, MathSysFuncTest,
    testing::Values(
        // clang-format off
        /* ---------- unary log / exp ---------- */
        MathTestParam{"MathFunc1", "$ln",    {{"1.0",         UhdmType::RealTypespec, false}}, "0",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc2", "$ln",    {{"2.718281828", UhdmType::RealTypespec, false}}, "0.9999999998311266", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc3", "$log10", {{"100.0",       UhdmType::RealTypespec, false}}, "2",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc4", "$log10", {{"1000.0",      UhdmType::RealTypespec, false}}, "3",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc5", "$exp",   {{"0.0",         UhdmType::RealTypespec, false}},  "1",                 UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc6", "$exp",   {{"1.0",         UhdmType::RealTypespec, false}}, "2.718281828459045",  UhdmType::RealTypespec, true},

        /* ---------- sqrt / pow ---------- */
        MathTestParam{"MathFunc7", "$sqrt", {{"4.0", UhdmType::RealTypespec, false}}, "2", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc8", "$pow", {{"2.0", UhdmType::RealTypespec, false}, {"3.0", UhdmType::RealTypespec, false}}, "8", UhdmType::RealTypespec, true},

        /* ---------- floor / ceil ---------- */
        MathTestParam{"MathFunc9",  "$floor", {{"3.7", UhdmType::RealTypespec, false}}, "3", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc10", "$floor", {{"5.9", UhdmType::RealTypespec, false}}, "5", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc11", "$ceil", {{"3.2", UhdmType::RealTypespec, false}}, "4", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc12", "$ceil", {{"5.1", UhdmType::RealTypespec, false}}, "6", UhdmType::RealTypespec, true},

        /* ---------- basic trig ---------- */
        MathTestParam{"MathFunc13", "$sin", {{"0.0",         UhdmType::RealTypespec, false}}, "0",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc14", "$sin", {{"1.570796326", UhdmType::RealTypespec, false}}, "1",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc15", "$cos", {{"0.0",         UhdmType::RealTypespec, false}}, "1",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc16", "$cos", {{"3.141592653", UhdmType::RealTypespec, false}}, "-1", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc17", "$tan", {{"0.0",         UhdmType::RealTypespec, false}}, "0",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc18", "$tan", {{"0.785398163", UhdmType::RealTypespec, false}}, "0.9999999992051033", UhdmType::RealTypespec, true},

        /* ---------- inverse trig ---------- */
        MathTestParam{"MathFunc19", "$asin", {{"0.0", UhdmType::RealTypespec, false}}, "0",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc20", "$asin", {{"0.5", UhdmType::RealTypespec, false}}, "0.5235987755982989", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc21", "$acos", {{"1.0", UhdmType::RealTypespec, false}}, "0",                 UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc22", "$acos", {{"0.5", UhdmType::RealTypespec, false}}, "1.047197551196598", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc23", "$atan", {{"0.0", UhdmType::RealTypespec, false}}, "0",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc24", "$atan", {{"1.0", UhdmType::RealTypespec, false}}, "0.7853981633974483", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc25", "$atan2", {{"0.0", UhdmType::RealTypespec, false},  {"1.0", UhdmType::RealTypespec, false}}, "0",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc26", "$atan2", {{"1.0", UhdmType::RealTypespec, false},  {"1.0", UhdmType::RealTypespec, false}}, "0.7853981633974483", UhdmType::RealTypespec, true},

        /* ---------- hyperbolic ---------- */
        MathTestParam{"MathFunc27", "$sinh", {{"0.0", UhdmType::RealTypespec, false}}, "0",                 UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc28", "$sinh", {{"1.0", UhdmType::RealTypespec, false}}, "1.175201193643801", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc29", "$cosh", {{"0.0", UhdmType::RealTypespec, false}}, "1",                 UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc30", "$cosh", {{"1.0", UhdmType::RealTypespec, false}}, "1.543080634815244", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc31", "$tanh", {{"0.0", UhdmType::RealTypespec, false}}, "0",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc32", "$tanh", {{"1.0", UhdmType::RealTypespec, false}}, "0.7615941559557649", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc33", "$asinh", {{"0.0", UhdmType::RealTypespec, false}}, "0",                 UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc34", "$asinh", {{"1.0", UhdmType::RealTypespec, false}}, "0.881373587019543", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc35", "$acosh", {{"1.0", UhdmType::RealTypespec, false}},  "0",                UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc36", "$acosh", {{"2.0", UhdmType::RealTypespec, false}}, "1.316957896924817", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc37", "$atanh", {{"0.0", UhdmType::RealTypespec, false}}, "0",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc38", "$atanh", {{"0.5", UhdmType::RealTypespec, false}}, "0.5493061443340549", UhdmType::RealTypespec, true},

        /* ---------- hypot ---------- */
        MathTestParam{"MathFunc39", "$hypot", {{"3.0", UhdmType::RealTypespec, false}, {"4.0", UhdmType::RealTypespec, false}}, "5",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc40", "$hypot", {{"6.0", UhdmType::RealTypespec, false}, {"8.0", UhdmType::RealTypespec, false}}, "10", UhdmType::RealTypespec, true},

        /* ---------- clog2 (if supported) ----------*/
        MathTestParam{ "MathFunc41", "$clog2", {{"8.0", UhdmType::RealTypespec, false}}, "3",  UhdmType::RealTypespec,  true},

        //----- Match precision --------
        MathTestParam{"MathFunc001", "$clog2", {{"1",  UhdmType::RealTypespec, false}}, "0", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc002", "$clog2", {{"2",  UhdmType::RealTypespec, false}}, "1", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc003", "$clog2", {{"3",  UhdmType::RealTypespec, false}}, "2", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc004", "$clog2", {{"4",  UhdmType::RealTypespec, false}}, "2", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc005", "$clog2", {{"7",  UhdmType::RealTypespec, false}}, "3", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc006", "$clog2", {{"8",  UhdmType::RealTypespec, false}}, "3", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc007", "$clog2", {{"15", UhdmType::RealTypespec, false}}, "4", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc008", "$clog2", {{"16", UhdmType::RealTypespec, false}}, "4", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc009", "$ln", {{"1.0",         UhdmType::RealTypespec, false}}, "0",                   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc010", "$ln", {{"2.718281828", UhdmType::RealTypespec, false}}, "0.9999999998311266",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc011", "$ln", {{"10.0",        UhdmType::RealTypespec, false}}, "2.302585092994046",   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc012", "$ln", {{"0.5",         UhdmType::RealTypespec, false}}, "-0.6931471805599453", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc013", "$ln", {{"100.0",       UhdmType::RealTypespec, false}}, "4.605170185988092",   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc014", "$ln", {{"0.0",         UhdmType::RealTypespec, false}}, "-inf",                UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc015", "$ln", {{"-1.0",        UhdmType::RealTypespec, true}},  "nan",                 UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc016", "$log10", {{"1.0",   UhdmType::RealTypespec, false}}, "0",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc017", "$log10", {{"10.0",  UhdmType::RealTypespec, false}}, "1",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc018", "$log10", {{"100.0", UhdmType::RealTypespec, false}}, "2",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc019", "$log10", {{"0.1",   UhdmType::RealTypespec, false}}, "-1",                 UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc020", "$log10", {{"5.0",   UhdmType::RealTypespec, false}}, "0.6989700043360189", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc021", "$log10", {{"0.0",   UhdmType::RealTypespec, false}}, "-inf",               UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc022", "$log10", {{"-10.0", UhdmType::RealTypespec, true}},  "nan",                UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc023", "$exp", {{"0.0",   UhdmType::RealTypespec, false}}, "1",                     UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc024", "$exp", {{"1.0",   UhdmType::RealTypespec, false}}, "2.718281828459045",     UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc025", "$exp", {{"2.0",   UhdmType::RealTypespec, false}}, "7.38905609893065",      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc026", "$exp", {{"-1.0",  UhdmType::RealTypespec, true}},  "0.3678794411714423",    UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc027", "$exp", {{"5.0",   UhdmType::RealTypespec, false}}, "148.4131591025766",     UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc028", "$exp", {{"10.0",  UhdmType::RealTypespec, false}}, "22026.46579480672",     UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc029", "$exp", {{"100.0", UhdmType::RealTypespec, false}}, "2.688117141816136e+43", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc030", "$clog2", {{"1024",                UhdmType::RealTypespec, false}}, "10", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc031", "$clog2", {{"1048576",             UhdmType::RealTypespec, false}}, "20", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc032", "$clog2", {{"1073741824",          UhdmType::RealTypespec, false}}, "30", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc033", "$clog2", {{"2147483647",          UhdmType::RealTypespec, false}}, "31", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc034", "$clog2", {{"4294967295",          UhdmType::RealTypespec, false}}, "32", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc035", "$clog2", {{"9223372036854775807", UhdmType::RealTypespec, false}}, "63", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc036", "$ln", {{"1e10",   UhdmType::RealTypespec, false}}, "23.02585092994046",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc037", "$ln", {{"1e20",   UhdmType::RealTypespec, false}}, "46.05170185988091",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc038", "$ln", {{"1e50",   UhdmType::RealTypespec, false}}, "115.1292546497023",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc039", "$ln", {{"1e100",  UhdmType::RealTypespec, false}}, "230.2585092994046",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc040", "$ln", {{"1e308",  UhdmType::RealTypespec, false}}, "709.1962086421661",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc041", "$ln", {{"1e-300", UhdmType::RealTypespec, false}}, "-690.7755278982137", UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc042", "$ln", {{"-1e50",  UhdmType::RealTypespec, true}},  "nan",                UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc043", "$log10", {{"1e10",   UhdmType::RealTypespec, false}}, "10",               UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc044", "$log10", {{"1e50",   UhdmType::RealTypespec, false}}, "50",               UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc045", "$log10", {{"1e100",  UhdmType::RealTypespec, false}}, "100",              UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc046", "$log10", {{"1e308",  UhdmType::RealTypespec, false}}, "308",              UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc047", "$log10", {{"5e200",  UhdmType::RealTypespec, false}}, "200.698970004336", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc048", "$log10", {{"1e-300", UhdmType::RealTypespec, false}}, "-300",             UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc049", "$log10", {{"-1e100", UhdmType::RealTypespec, true}},  "nan",              UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc050", "$exp", {{"10.0",  UhdmType::RealTypespec, false}}, "22026.46579480672",      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc051", "$exp", {{"50.0",  UhdmType::RealTypespec, false}}, "5.184705528587072e+21",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc052", "$exp", {{"100.0", UhdmType::RealTypespec, false}}, "2.688117141816136e+43",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc053", "$exp", {{"500.0", UhdmType::RealTypespec, false}}, "1.403592217852838e+217", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc054", "$exp", {{"700.0", UhdmType::RealTypespec, false}}, "1.014232054735004e+304", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc055", "$exp", {{"709.0", UhdmType::RealTypespec, false}}, "8.218407461554972e+307", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc056", "$exp", {{"710.0", UhdmType::RealTypespec, false}}, "inf",                    UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc057", "$exp", {{"-700.0",UhdmType::RealTypespec, true}},  "9.859676543759771e-305", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc058", "$sqrt", {{"0.0",    UhdmType::RealTypespec, false}}, "0",                 UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc059", "$sqrt", {{"1.0",    UhdmType::RealTypespec, false}}, "1",                 UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc060", "$sqrt", {{"2.0",    UhdmType::RealTypespec, false}}, "1.414213562373095", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc061", "$sqrt", {{"1e10",   UhdmType::RealTypespec, false}}, "100000",            UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc062", "$sqrt", {{"1e100",  UhdmType::RealTypespec, false}}, "1e+50",             UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc063", "$sqrt", {{"1e308",  UhdmType::RealTypespec, false}}, "1e+154",            UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc064", "$sqrt", {{"-1.0",  UhdmType::RealTypespec, true}},  "-nan",              UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc065", "$sqrt", {{"-1e50", UhdmType::RealTypespec, true}},  "-nan",              UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc074", "$floor", {{"0.0",         UhdmType::RealTypespec, false}}, "0",       UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc075", "$floor", {{"1.1",         UhdmType::RealTypespec, false}}, "1",       UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc076", "$floor", {{"-1.1",        UhdmType::RealTypespec, true}},  "-2",      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc077", "$floor", {{"123456.789",  UhdmType::RealTypespec, false}}, "123456",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc078", "$floor", {{"-123456.789", UhdmType::RealTypespec, true}},  "-123457", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc079", "$floor", {{"1e20",        UhdmType::RealTypespec, false}}, "1e+20",   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc080", "$floor", {{"-1e20",       UhdmType::RealTypespec, true}},  "-1e+20",  UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc081", "$ceil", {{"0.0",         UhdmType::RealTypespec, false}}, "0",      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc082", "$ceil", {{"1.1",         UhdmType::RealTypespec, false}}, "2",      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc083", "$ceil", {{"-1.1",        UhdmType::RealTypespec, true}},  "-1",     UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc084", "$ceil", {{"123456.001",  UhdmType::RealTypespec, false}}, "123457", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc085", "$ceil", {{"-123456.001", UhdmType::RealTypespec, true}}, "-123456", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc086", "$ceil", {{"1e20",        UhdmType::RealTypespec, false}}, "1e+20",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc087", "$ceil", {{"-1e20",       UhdmType::RealTypespec, true}},  "-1e+20", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc088", "$asin", {{"0.0",   UhdmType::RealTypespec, false}}, "0",                   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc089", "$asin", {{"1.0",   UhdmType::RealTypespec, false}}, "1.570796326794897",   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc090", "$asin", {{"-1.0",  UhdmType::RealTypespec, true}},  "-1.570796326794897",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc091", "$asin", {{"0.5",   UhdmType::RealTypespec, false}}, "0.5235987755982989",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc092", "$asin", {{"-0.5",  UhdmType::RealTypespec, true}},  "-0.5235987755982989", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc093", "$asin", {{"1e-10", UhdmType::RealTypespec, false}}, "1e-10",               UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc094", "$asin", {{"2.0",   UhdmType::RealTypespec, false}}, "nan",                 UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc095", "$acos", {{"1.0",   UhdmType::RealTypespec, false}}, "0",                 UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc096", "$acos", {{"-1.0",  UhdmType::RealTypespec, true}},  "3.141592653589793", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc097", "$acos", {{"0.0",   UhdmType::RealTypespec, false}}, "1.570796326794897", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc098", "$acos", {{"0.5",   UhdmType::RealTypespec, false}}, "1.047197551196598", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc099", "$acos", {{"-0.5",  UhdmType::RealTypespec, true}},  "2.094395102393196", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc100", "$acos", {{"1e-12", UhdmType::RealTypespec, false}}, "1.570796326793897", UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc101", "$acos", {{"-2.0",  UhdmType::RealTypespec, true}},  "nan",               UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc102", "$atan", {{"0.0",   UhdmType::RealTypespec, false}}, "0",                   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc103", "$atan", {{"1.0",   UhdmType::RealTypespec, false}}, "0.7853981633974483",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc104", "$atan", {{"-1.0",  UhdmType::RealTypespec, true}},  "-0.7853981633974483", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc105", "$atan", {{"1e10",  UhdmType::RealTypespec, false}}, "1.570796326694897",   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc106", "$atan", {{"-1e10", UhdmType::RealTypespec, true}},  "-1.570796326694897",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc107", "$atan", {{"1e-10", UhdmType::RealTypespec, false}}, "1e-10",               UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc108", "$atan2", {{"0.0",   UhdmType::RealTypespec, false}, {"1.0",  UhdmType::RealTypespec, false}}, "0",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc109", "$atan2", {{"1.0",   UhdmType::RealTypespec, false}, {"0.0",  UhdmType::RealTypespec, false}}, "1.570796326794897",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc110", "$atan2", {{"-1.0",  UhdmType::RealTypespec, true }, {"0.0",  UhdmType::RealTypespec, false}}, "-1.570796326794897", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc111", "$atan2", {{"1.0",   UhdmType::RealTypespec, false}, {"1.0",  UhdmType::RealTypespec, false}}, "0.7853981633974483", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc112", "$atan2", {{"-1.0",  UhdmType::RealTypespec, true }, {"-1.0", UhdmType::RealTypespec, true}},  "-2.356194490192345", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc113", "$atan2", {{"1e308", UhdmType::RealTypespec, false}, {"1.0",  UhdmType::RealTypespec, false}}, "1.570796326794897",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc114", "$atan2", {{"0.0",   UhdmType::RealTypespec, false}, {"0.0",  UhdmType::RealTypespec, false}}, "0",                  UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc115", "$hypot", {{"0.0",   UhdmType::RealTypespec, false}, {"0.0",   UhdmType::RealTypespec, false}}, "0",                      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc116", "$hypot", {{"3.0",   UhdmType::RealTypespec, false}, {"4.0",   UhdmType::RealTypespec, false}}, "5",                      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc117", "$hypot", {{"-3.0",  UhdmType::RealTypespec, true }, {"4.0",   UhdmType::RealTypespec, false}}, "5",                      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc118", "$hypot", {{"1e154", UhdmType::RealTypespec, false}, {"1e154", UhdmType::RealTypespec, false}}, "1.414213562373095e+154", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc119", "$hypot", {{"1e308", UhdmType::RealTypespec, false}, {"1.0",   UhdmType::RealTypespec, false}}, "1e+308",                 UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc120", "$hypot", {{"1e308", UhdmType::RealTypespec, false}, {"1e308", UhdmType::RealTypespec, false}}, "1.414213562373095e+308", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc121", "$pow", {{"2.0",   UhdmType::RealTypespec, false}, {"8.0",   UhdmType::RealTypespec, false}}, "256",    UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc122", "$pow", {{"10.0",  UhdmType::RealTypespec, false}, {"0.0",   UhdmType::RealTypespec, false}}, "1",      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc123", "$pow", {{"0.0",   UhdmType::RealTypespec, false}, {"10.0",  UhdmType::RealTypespec, false}}, "0",      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc124", "$pow", {{"-2.0",  UhdmType::RealTypespec, true }, {"3.0",   UhdmType::RealTypespec, false}}, "-8",     UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc125", "$pow", {{"-2.0",  UhdmType::RealTypespec, true }, {"0.5",   UhdmType::RealTypespec, false}}, "-nan",   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc126", "$pow", {{"1e154", UhdmType::RealTypespec, false}, {"2.0",   UhdmType::RealTypespec, false}}, "1e+308", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc127", "$pow", {{"1e200", UhdmType::RealTypespec, false}, {"2.0",   UhdmType::RealTypespec, false}}, "inf",    UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc128", "$sinh", {{"0.0",   UhdmType::RealTypespec, false}}, "0",                      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc129", "$sinh", {{"1.0",   UhdmType::RealTypespec, false}}, "1.175201193643801",      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc131", "$sinh", {{"10.0",  UhdmType::RealTypespec, false}}, "11013.23287470339",      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc132", "$sinh", {{"-10.0", UhdmType::RealTypespec, true}},  "-11013.23287470339",     UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc133", "$sinh", {{"700.0", UhdmType::RealTypespec, false}}, "5.071160273675022e+303", UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc134", "$sinh", {{"710.0", UhdmType::RealTypespec, false}}, "1.116997383080856e+308", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc135", "$cosh", {{"0.0",    UhdmType::RealTypespec, false}}, "1",                      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc136", "$cosh", {{"1.0",    UhdmType::RealTypespec, false}}, "1.543080634815244",      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc137", "$cosh", {{"-1.0",   UhdmType::RealTypespec, true}},  "1.543080634815244",      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc138", "$cosh", {{"10.0",   UhdmType::RealTypespec, false}}, "11013.23292010332",      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc139", "$cosh", {{"700.0",  UhdmType::RealTypespec, false}}, "5.071160273675022e+303", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc140", "$cosh", {{"-700.0", UhdmType::RealTypespec, true}},  "5.071160273675022e+303", UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc141", "$cosh", {{"710.0",  UhdmType::RealTypespec, false}}, "1.116997383080856e+308", UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc142", "$tanh", {{"0.0",    UhdmType::RealTypespec, false}}, "0",                   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc143", "$tanh", {{"1.0",    UhdmType::RealTypespec, false}}, "0.7615941559557649",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc144", "$tanh", {{"-1.0",   UhdmType::RealTypespec, true}},  "-0.7615941559557649", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc145", "$tanh", {{"10.0",   UhdmType::RealTypespec, false}}, "0.9999999958776927",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc146", "$tanh", {{"-10.0",  UhdmType::RealTypespec, true}},  "-0.9999999958776927", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc147", "$tanh", {{"700.0",  UhdmType::RealTypespec, false}}, "1",                   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc148", "$tanh", {{"-700.0", UhdmType::RealTypespec, true}},  "-1",                  UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc149", "$sin", {{"0.0",       UhdmType::RealTypespec, false}}, "0",                      UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc150", "$sin", {{"1.570796",  UhdmType::RealTypespec, false}}, "0.9999999999999466",     UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc151", "$sin", {{"-1.570796", UhdmType::RealTypespec, true}},  "-0.9999999999999466",    UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc152", "$sin", {{"3.141593",  UhdmType::RealTypespec, false}}, "-3.464102066193935e-07", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc153", "$sin", {{"1e10",      UhdmType::RealTypespec, false}}, "-0.4875060250875107",    UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc154", "$sin", {{"-1e10",     UhdmType::RealTypespec, true}},  "0.4875060250875107",     UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc155", "$sin", {{"1e-10",     UhdmType::RealTypespec, false}}, "1e-10",                  UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc156", "$asinh", {{"0.0",   UhdmType::RealTypespec, false}}, "0",                  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc157", "$asinh", {{"1.0",   UhdmType::RealTypespec, false}}, "0.881373587019543",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc158", "$asinh", {{"-1.0",  UhdmType::RealTypespec, true}},  "-0.881373587019543", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc159", "$asinh", {{"10.0",  UhdmType::RealTypespec, false}}, "2.99822295029797",   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc160", "$asinh", {{"-10.0", UhdmType::RealTypespec, true}},  "-2.99822295029797",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc161", "$asinh", {{"1e20",  UhdmType::RealTypespec, false}}, "46.74484904044086",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc162", "$asinh", {{"1e308", UhdmType::RealTypespec, false}}, "709.889355822726",   UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc163", "$cos", {{"0.0",       UhdmType::RealTypespec, false}}, "1",                     UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc164", "$cos", {{"1.570796",  UhdmType::RealTypespec, false}}, "3.267948965381384e-07", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc165", "$cos", {{"3.141593",  UhdmType::RealTypespec, false}}, "-0.99999999999994",     UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc166", "$cos", {{"-3.141593", UhdmType::RealTypespec, true}},  "-0.99999999999994",     UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc167", "$cos", {{"1e10",      UhdmType::RealTypespec, false}}, "0.8731196226768561",    UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc168", "$cos", {{"-1e10",     UhdmType::RealTypespec, true}},  "0.8731196226768561",    UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc169", "$cos", {{"1e-10",     UhdmType::RealTypespec, false}}, "1",                     UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc170", "$acosh", {{"1.0",   UhdmType::RealTypespec, false}}, "0",                 UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc171", "$acosh", {{"2.0",   UhdmType::RealTypespec, false}}, "1.316957896924817", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc172", "$acosh", {{"10.0",  UhdmType::RealTypespec, false}}, "2.993222846126381", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc173", "$acosh", {{"1e10",  UhdmType::RealTypespec, false}}, "23.7189981105004",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc174", "$acosh", {{"1e308", UhdmType::RealTypespec, false}}, "709.889355822726",  UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc175", "$acosh", {{"0.5",   UhdmType::RealTypespec, false}}, "-nan",              UhdmType::RealTypespec, true},
        // MathTestParam{"MathFunc176", "$acosh", {{"-10.0", UhdmType::RealTypespec, true}},  "-nan",              UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc177", "$tan", {{"0.0",       UhdmType::RealTypespec, false}}, "0",                   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc178", "$tan", {{"0.785398",  UhdmType::RealTypespec, false}}, "0.9999996732051568",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc179", "$tan", {{"-0.785398", UhdmType::RealTypespec, true}},  "-0.9999996732051568", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc180", "$tan", {{"1.570796",  UhdmType::RealTypespec, false}}, "3060023.306952844",   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc181", "$tan", {{"-1.570796", UhdmType::RealTypespec, true}},  "-3060023.306952844",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc182", "$tan", {{"1e10",      UhdmType::RealTypespec, false}}, "-0.5583496378112418", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc183", "$tan", {{"1e-10",     UhdmType::RealTypespec, false}}, "1e-10",               UhdmType::RealTypespec, true},

        MathTestParam{"MathFunc184", "$atanh", {{"0.0",   UhdmType::RealTypespec, false}}, "0",                   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc185", "$atanh", {{"0.5",   UhdmType::RealTypespec, false}}, "0.5493061443340548",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc186", "$atanh", {{"-0.5",  UhdmType::RealTypespec, true}},  "-0.5493061443340548", UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc187", "$atanh", {{"0.99",  UhdmType::RealTypespec, false}}, "2.646652412362246",   UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc188", "$atanh", {{"-0.99", UhdmType::RealTypespec, true}},  "-2.646652412362246",  UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc189", "$atanh", {{"1.0",   UhdmType::RealTypespec, false}}, "inf",                 UhdmType::RealTypespec, true},
        MathTestParam{"MathFunc190", "$atanh", {{"-1.0",  UhdmType::RealTypespec, true}},  "-inf",                UhdmType::RealTypespec, true}
        // clang-format on
    ));

INSTANTIATE_TEST_SUITE_P(
    ConvSysFuncs, ConvSysFuncTest,
    testing::Values(
        // clang-format off
        ConvTestParam{"MathFunc191", "$itor", {{"-5",   UhdmType::IntTypespec,   true}},  "-5.0",  UhdmType::RealTypespec, true},
        ConvTestParam{"MathFunc192", "$itor", {{"101",  UhdmType::LogicTypespec, false}}, "5.0",   UhdmType::RealTypespec, true},
        ConvTestParam{"MathFunc193", "$itor", {{"-10",  UhdmType::ByteTypespec,  true}},  "-10.0", UhdmType::RealTypespec, true},
        ConvTestParam{"MathFunc191", "$itor", {{"101",  UhdmType::IntTypespec,   false}}, "101",   UhdmType::RealTypespec, true},
        ConvTestParam{"MathFunc193", "$itor", {{"1010", UhdmType::LogicTypespec, false}}, "10.0",  UhdmType::RealTypespec, true},
                                           
        ConvTestParam{"MathFunc194", "$rtoi", {{"3.9",  UhdmType::RealTypespec,  false}}, "3",  UhdmType::IntTypespec, true},
        ConvTestParam{"MathFunc195", "$rtoi", {{"-3.9", UhdmType::RealTypespec,  true}},  "-3", UhdmType::IntTypespec, true},
        ConvTestParam{"MathFunc196", "$rtoi", {{"4.7",  UhdmType::RealTypespec, false}},  "4",  UhdmType::IntTypespec, true},
        ConvTestParam{"MathFunc197", "$rtoi", {{"16",   UhdmType::ByteTypespec,  false}}, "16", UhdmType::IntTypespec, true},

        // TOCHECK::Arshi
        ConvTestParam{"MathFunc198", "$signed", {{"255",   UhdmType::IntTypespec, false}}, "255",   UhdmType::IntTypespec, true},
        ConvTestParam{"MathFunc199", "$signed", {{"127",   UhdmType::IntTypespec, false}}, "127",   UhdmType::IntTypespec, true},
        ConvTestParam{"MathFunc200", "$signed", {{"32768", UhdmType::IntTypespec, false}}, "32768", UhdmType::IntTypespec, true},
        ConvTestParam{"MathFunc200_1", "$signed", {{"1100", UhdmType::LogicTypespec, false}}, "1100", UhdmType::LogicTypespec, true},

        //TOCHECK::Arshi for output sign
        ConvTestParam{"MathFunc201", "$unsigned", {{"-1",      UhdmType::IntTypespec,   true}},  "4294967295", UhdmType::IntTypespec, false},
        ConvTestParam{"MathFunc202", "$unsigned", {{"255",     UhdmType::IntTypespec,   false}}, "255",        UhdmType::IntTypespec, false},
        ConvTestParam{"MathFunc203", "$unsigned", {{"0",       UhdmType::IntTypespec,   false}}, "0",          UhdmType::IntTypespec, false},
        ConvTestParam{"MathFunc203_1", "$unsigned", {{"-4",    UhdmType::IntTypespec,   true}}, "4294967292",          UhdmType::IntTypespec, false},
        ConvTestParam{"MathFunc209", "$unsigned", {{"11111111", UhdmType::LogicTypespec, true}},   "11111111",        UhdmType::LogicTypespec, false},
        //
        ConvTestParam{"MathFunc204", "$cast", {{"int",  UhdmType::IntTypespec, true},  {"3.9",       UhdmType::RealTypespec,  false}}, "4",    UhdmType::IntTypespec,  true},
        ConvTestParam{"MathFunc205", "$cast", {{"real", UhdmType::RealTypespec, true}, {"10",        UhdmType::IntTypespec,   false}}, "10.0", UhdmType::RealTypespec, true},
        ConvTestParam{"MathFunc206", "$cast", {{"int",  UhdmType::IntTypespec, true},  {"100000000", UhdmType::LogicTypespec, false}}, "256",  UhdmType::IntTypespec,  true},
        ConvTestParam{"MathFunc207", "$cast", {{"int",  UhdmType::IntTypespec, true},  {"-1",        UhdmType::ByteTypespec,  true}},  "-1",   UhdmType::IntTypespec,  true},
        ConvTestParam{"MathFunc208", "$cast", {{"int",  UhdmType::IntTypespec, true},  {"3.1",       UhdmType::RealTypespec,  false}}, "3",    UhdmType::IntTypespec,  true},
        //
        ConvTestParam{"MathFunc210", "$bitstoreal", {{"0011111111110000000000000000000000000000000000000000000000000000", UhdmType::LogicTypespec, false}}, "1.0",           UhdmType::RealTypespec, true},
        ConvTestParam{"MathFunc211", "$bitstoreal", {{"0100000000000000000000000000000000000000000000000000000000000000", UhdmType::LogicTypespec, false}}, "2.0",           UhdmType::RealTypespec, true},
        ConvTestParam{"MathFunc212", "$bitstoreal", {{"1011111111110000000000000000000000000000000000000000000000000000", UhdmType::LogicTypespec, false}}, "-1.0",          UhdmType::RealTypespec, true},
        ConvTestParam{"MathFunc213", "$bitstoreal", {{"0000000000000000000000000000000000000000000000000000000000000000", UhdmType::LogicTypespec, false}}, "0.0",           UhdmType::RealTypespec, true},
        ConvTestParam{"MathFunc214", "$bitstoreal", {{"1000000000000000000000000000000000000000000000000000000000000000", UhdmType::LogicTypespec, false}}, "-0.0",          UhdmType::RealTypespec, true},
        ConvTestParam{"MathFunc215", "$bitstoreal", {{"0111111111110000000000000000000000000000000000000000000000000000", UhdmType::LogicTypespec, false}}, "inf",           UhdmType::RealTypespec, true},
        ConvTestParam{"MathFunc216", "$bitstoreal", {{"1111111111110000000000000000000000000000000000000000000000000000", UhdmType::LogicTypespec, false}}, "-inf",          UhdmType::RealTypespec, true},
        // ConvTestParam{"MathFunc217", "$bitstoreal", {{"0111111111111000000000000000000000000000000000000000000000000000", UhdmType::LogicTypespec, false}}, "nan",           UhdmType::RealTypespec, true},
        ConvTestParam{"MathFunc218", "$bitstoreal", {{"0000000000000000000000000000000000000000000000000000000000000001", UhdmType::LogicTypespec, false}}, "4.940656e-324", UhdmType::RealTypespec, true},
        //
        ConvTestParam{"MathFunc219", "$realtobits", {{"1.0",      UhdmType::RealTypespec, false}}, "4607182418800017408",  UhdmType::LongIntTypespec, false},
        ConvTestParam{"MathFunc220", "$realtobits", {{"2.0",      UhdmType::RealTypespec, false}}, "4611686018427387904",  UhdmType::LongIntTypespec, false},
        ConvTestParam{"MathFunc221", "$realtobits", {{"-1.0",     UhdmType::RealTypespec, true}},  "13830554455654793216", UhdmType::LongIntTypespec, false},
        ConvTestParam{"MathFunc222", "$realtobits", {{"0.0",      UhdmType::RealTypespec, false}}, "0",                    UhdmType::LongIntTypespec, false},
        ConvTestParam{"MathFunc223", "$realtobits", {{"-0.0",     UhdmType::RealTypespec, true}},  "9223372036854775808",  UhdmType::LongIntTypespec, false},
        // ConvTestParam{"MathFunc224", "$realtobits", {{"1.0/0.0",  UhdmType::RealTypespec, false}}, "9218868437227405312",  UhdmType::LongIntTypespec, false},
        // ConvTestParam{"MathFunc225", "$realtobits", {{"-1.0/0.0", UhdmType::RealTypespec, false}}, "18442240474377011200", UhdmType::LongIntTypespec, false},
        // ConvTestParam{"MathFunc226", "$realtobits", {{"0.0/0.0",  UhdmType::RealTypespec, false}}, "9221120237041090560",  UhdmType::LongIntTypespec, false}
        //
        ConvTestParam{"MathFunc227", "$bitstoshortreal", {{"00111111100000000000000000000000", UhdmType::LogicTypespec, false}},   "1.0",          UhdmType::ShortRealTypespec, true},
        ConvTestParam{"MathFunc228", "$bitstoshortreal", {{"01000000000000000000000000000000", UhdmType::LogicTypespec, false}},   "2.0",          UhdmType::ShortRealTypespec, true},
        ConvTestParam{"MathFunc229", "$bitstoshortreal", {{"10111111100000000000000000000000", UhdmType::LogicTypespec, false}},   "-1.0",         UhdmType::ShortRealTypespec, true},
        ConvTestParam{"MathFunc230", "$bitstoshortreal", {{"00000000000000000000000000000000", UhdmType::LogicTypespec,   false}}, "0.0",          UhdmType::ShortRealTypespec, true},
        ConvTestParam{"MathFunc231", "$bitstoshortreal", {{"10000000000000000000000000000000", UhdmType::LogicTypespec, false}},   "-0.0",         UhdmType::ShortRealTypespec, true},
        ConvTestParam{"MathFunc232", "$bitstoshortreal", {{"01111111100000000000000000000000", UhdmType::LogicTypespec, false}},   "inf",          UhdmType::ShortRealTypespec, true},
        ConvTestParam{"MathFunc233", "$bitstoshortreal", {{"11111111100000000000000000000000", UhdmType::LogicTypespec, false}},   "-inf",         UhdmType::ShortRealTypespec, true},
        // ConvTestParam{"MathFunc234", "$bitstoshortreal", {{"01111111110000000000000000000000", UhdmType::LogicTypespec, false}},   "nan",          UhdmType::ShortRealTypespec, true},
        ConvTestParam{"MathFunc235", "$bitstoshortreal", {{"00000000000000000000000000000001", UhdmType::LogicTypespec, false}},   "1.401298e-45", UhdmType::ShortRealTypespec, true},
        //
        ConvTestParam{"MathFunc236", "$shortrealtobits", {{"1.0",  UhdmType::ShortRealTypespec, false}}, "1065353216", UhdmType::IntTypespec, false},
        ConvTestParam{"MathFunc237", "$shortrealtobits", {{"2.0",  UhdmType::ShortRealTypespec, false}}, "1073741824", UhdmType::IntTypespec, false},
        ConvTestParam{"MathFunc238", "$shortrealtobits", {{"-1.0", UhdmType::ShortRealTypespec, false}}, "3212836864", UhdmType::IntTypespec, false},
        ConvTestParam{"MathFunc239", "$shortrealtobits", {{"0.0",  UhdmType::ShortRealTypespec, false}}, "0",          UhdmType::IntTypespec, false},
        ConvTestParam{"MathFunc240", "$shortrealtobits", {{"-0.0", UhdmType::ShortRealTypespec, false}}, "2147483648", UhdmType::IntTypespec, false}
        // ConvTestParam{"MathFunc241", "$shortrealtobits", {{"1.0/0.0",  UhdmType::ShortRealTypespec, false}}, "2139095040", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc242", "$shortrealtobits", {{"-1.0/0.0", UhdmType::ShortRealTypespec, false}}, "4286578688", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc243", "$shortrealtobits", {{"0.0/0.0",  UhdmType::ShortRealTypespec, false}}, "2143289344", UhdmType::IntTypespec, false}
        //
        // ConvTestParam{"MathFunc244", "$bits", {{"byte",        UhdmType::IntTypespec,   false}}, "8",  UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc245", "$bits", {{"shortint",    UhdmType::IntTypespec,   false}}, "16", UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc246", "$bits", {{"int",         UhdmType::IntTypespec,   false}}, "32", UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc247", "$bits", {{"longint",     UhdmType::IntTypespec,   false}}, "64", UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc248", "$bits", {{"logic",       UhdmType::LogicTypespec, false}}, "1",  UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc249", "$bits", {{"logic [3:0]", UhdmType::LogicTypespec, false}}, "4",  UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc250", "$bits", {{"logic [7:0]", UhdmType::LogicTypespec, false}}, "8",  UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc251", "$bits", {{"bit [15:0]",  UhdmType::LogicTypespec, false}}, "16", UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc252", "$bits", {{"real",        UhdmType::RealTypespec,  false}}, "64", UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc253", "$bits", {{"shortreal",   UhdmType::RealTypespec,  false}}, "32", UhdmType::IntTypespec, true},
        //
        // ConvTestParam{"MathFunc256", "$isunbounded", {{"int",            UhdmType::IntTypespec,   false}}, "0", UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc257", "$isunbounded", {{"logic[7:0]",     UhdmType::LogicTypespec, false}}, "0", UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc258", "$isunbounded", {{"real",           UhdmType::RealTypespec,  false}}, "0", UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc259", "$isunbounded", {{"string",         UhdmType::IntTypespec,   false}}, "1", UhdmType::IntTypespec, true},
        // ConvTestParam{"MathFunc260", "$isunbounded", {{"$typename(int)", UhdmType::IntTypespec,   false}}, "1", UhdmType::IntTypespec, true},
        //
        // ConvTestParam{"MathFunc264", "$typename", {{"logic",      UhdmType::LogicTypespec,  false}}, "\"logic\"",     UhdmType::StringTypespec, true},
        // ConvTestParam{"MathFunc265", "$typename", {{"logic[3:0]", UhdmType::LogicTypespec,  false}}, "\"logic\"",     UhdmType::StringTypespec, true},
        // ConvTestParam{"MathFunc266", "$typename", {{"real",       UhdmType::RealTypespec,   false}}, "\"real\"",      UhdmType::StringTypespec, true},
        // ConvTestParam{"MathFunc267", "$typename", {{"shortreal",  UhdmType::RealTypespec,   false}}, "\"shortreal\"", UhdmType::StringTypespec, true},
        // ConvTestParam{"MathFunc268", "$typename", {{"3.14",       UhdmType::RealTypespec,   false}}, "\"real\"",      UhdmType::StringTypespec, true},
        // ConvTestParam{"MathFunc269", "$typename", {{"1+2",        UhdmType::StringTypespec, false}}, "\"int\"",       UhdmType::StringTypespec, true},
        //
        // ConvTestParam{"MathFunc270", "$countbits", {{"0000",     UhdmType::LogicTypespec, false},{"1", UhdmType::LogicTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc271", "$countbits", {{"1111",     UhdmType::LogicTypespec, false},{"1", UhdmType::LogicTypespec, false}}, "4", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc272", "$countbits", {{"1010",     UhdmType::LogicTypespec, false},{"1", UhdmType::LogicTypespec, false}}, "2", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc273", "$countbits", {{"10xz",     UhdmType::LogicTypespec, false},{"1", UhdmType::LogicTypespec, false}}, "1", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc274", "$countbits", {{"11001100", UhdmType::BitTypespec,   false},{"1", UhdmType::LogicTypespec, false}}, "4", UhdmType::IntTypespec, false},
        //
        // ConvTestParam{"MathFunc275", "$countones", {{"0000",     UhdmType::LogicTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc276", "$countones", {{"1111",     UhdmType::LogicTypespec, false}}, "4", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc277", "$countones", {{"1101",     UhdmType::LogicTypespec, false}}, "3", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc278", "$countones", {{"01xz",     UhdmType::LogicTypespec, false}}, "1", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc279", "$countones", {{"11110000", UhdmType::BitTypespec,   false}}, "4", UhdmType::IntTypespec, false},
        //
        // ConvTestParam{"MathFunc280", "$onehot", {{"0001",     UhdmType::LogicTypespec, false}}, "1", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc281", "$onehot", {{"0010",     UhdmType::LogicTypespec, false}}, "1", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc282", "$onehot", {{"0101",     UhdmType::LogicTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc283", "$onehot", {{"0000",     UhdmType::LogicTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc284", "$onehot", {{"0001zz00", UhdmType::BitTypespec,   false}}, "1", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc285", "$onehot", {{"00x1",     UhdmType::LogicTypespec, false}}, "1", UhdmType::IntTypespec, false},
        //
        // ConvTestParam{"MathFunc286", "$onehot0", {{"0000",     UhdmType::LogicTypespec, false}}, "1", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc287", "$onehot0", {{"0001",     UhdmType::LogicTypespec, false}}, "1", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc288", "$onehot0", {{"0011",     UhdmType::LogicTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc289", "$onehot0", {{"0100",     UhdmType::LogicTypespec, false}}, "1", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc290", "$onehot0", {{"00000000", UhdmType::BitTypespec,   false}}, "1", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc291", "$onehot0", {{"0z00",     UhdmType::LogicTypespec, false}}, "1", UhdmType::IntTypespec, false},
        //
        // ConvTestParam{"MathFunc292", "$isunknown", {{"0000",          UhdmType::LogicTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc293", "$isunknown", {{"1111",          UhdmType::LogicTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc294", "$isunknown", {{"10x0",          UhdmType::LogicTypespec, false}}, "1", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc295", "$isunknown", {{"001z",          UhdmType::LogicTypespec, false}}, "1", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc296", "$isunknown", {{"x",             UhdmType::LogicTypespec, false}}, "1", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc297", "$isunknown", {{"4,294,967,295", UhdmType::IntTypespec,   false}}, "0", UhdmType::IntTypespec, false},
        //
        // ConvTestParam{"MathFunc300", "$countbits", {{"255",                        UhdmType::IntTypespec,     false}, {"1", UhdmType::LogicTypespec, false}}, "8",  UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc301", "$countbits", {{"-255",                       UhdmType::IntTypespec,     false}, {"1", UhdmType::LogicTypespec, false}}, "25", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc302", "$countbits", {{"18,446,744,073,709,551,615", UhdmType::LongIntTypespec, false}, {"1", UhdmType::LogicTypespec, false}}, "64", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc303", "$countbits", {{"9,223,372,036,854,775,807",  UhdmType::LongIntTypespec, false}, {"1", UhdmType::LogicTypespec, false}}, "63", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc304", "$countbits", {{"-9,223,372,036,854,775,807", UhdmType::LongIntTypespec, false}, {"1", UhdmType::LogicTypespec, false}}, "2",  UhdmType::IntTypespec, false},
        //
        // ConvTestParam{"MathFunc305", "$countones", {{"255",                        UhdmType::IntTypespec, false}},     "8",  UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc306", "$countones", {{"-255",                       UhdmType::IntTypespec, false}},     "25", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc307", "$countones", {{"18,446,744,073,709,551,615", UhdmType::LongIntTypespec, false}}, "64", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc308", "$countones", {{"9,223,372,036,854,775,807",  UhdmType::LongIntTypespec, false}}, "63", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc309", "$countones", {{"-9,223,372,036,854,775,807", UhdmType::LongIntTypespec, true}},  "2",  UhdmType::IntTypespec, false},
        //
        // ConvTestParam{"MathFunc310", "$onehot", {{"255",                        UhdmType::IntTypespec,     false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc311", "$onehot", {{"-255",                       UhdmType::IntTypespec,     false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc312", "$onehot", {{"18,446,744,073,709,551,615", UhdmType::LongIntTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc313", "$onehot", {{"9,223,372,036,854,775,807",  UhdmType::LongIntTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc314", "$onehot", {{"-9,223,372,036,854,775,807", UhdmType::LongIntTypespec, true}},  "0", UhdmType::IntTypespec, false},
        //
        // ConvTestParam{"MathFunc315", "$onehot0", {{"255",                        UhdmType::IntTypespec,     false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc316", "$onehot0", {{"-255",                       UhdmType::IntTypespec,     false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc317", "$onehot0", {{"18,446,744,073,709,551,615", UhdmType::LongIntTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc318", "$onehot0", {{"9,223,372,036,854,775,807",  UhdmType::LongIntTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc319", "$onehot0", {{"-9,223,372,036,854,775,807", UhdmType::LongIntTypespec, true}},  "0", UhdmType::IntTypespec, false},
        //
        // ConvTestParam{"MathFunc320", "$isunknown", {{"255",                        UhdmType::IntTypespec,     false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc321", "$isunknown", {{"-255",                       UhdmType::IntTypespec,     false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc322", "$isunknown", {{"18,446,744,073,709,551,615", UhdmType::LongIntTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc323", "$isunknown", {{"9,223,372,036,854,775,807",  UhdmType::LongIntTypespec, false}}, "0", UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc324", "$isunknown", {{"-9,223,372,036,854,775,807", UhdmType::LongIntTypespec, true}},  "0", UhdmType::IntTypespec, false},
        //
        // ConvTestParam{"MathFunc325", "$countbits", {{"1010",         UhdmType::LogicTypespec, false},{"1", UhdmType::LogicTypespec, false}}, "2",  UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc326", "$countbits", {{"1010",         UhdmType::LogicTypespec, false},{"0", UhdmType::LogicTypespec, false}}, "2",  UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc327", "$countbits", {{"10x0",         UhdmType::LogicTypespec, false},{"x", UhdmType::LogicTypespec, false}}, "1",  UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc328", "$countbits", {{"10z0",         UhdmType::LogicTypespec, false},{"z", UhdmType::LogicTypespec, false}}, "1",  UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc329", "$countbits", {{"xxxx",         UhdmType::LogicTypespec, false},{"x", UhdmType::LogicTypespec, false}}, "4",  UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc330", "$countbits", {{"zzzz",         UhdmType::LogicTypespec, false},{"z", UhdmType::LogicTypespec, false}}, "4",  UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc331", "$countbits", {{"11001100",     UhdmType::BitTypespec,   false},{"1", UhdmType::BitTypespec,   false}}, "4",  UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc331", "$countbits", {{"11001100",     UhdmType::BitTypespec,   false},{"0", UhdmType::BitTypespec,   false}}, "4",  UhdmType::IntTypespec, false},
        // ConvTestParam{"MathFunc332", "$countbits", {{"4,294,901760", UhdmType::IntTypespec,   false},{"1", UhdmType::LogicTypespec, false}}, "16", UhdmType::IntTypespec, false}
        // clang-format on
    ));
#else
INSTANTIATE_TEST_SUITE_P(DataQuery20_6, DataQuerySysFuncTest,
                         ::testing::Values(DataQueryTestParam{"DQ_TypeName_Int",
                                                              "$typename",
                                                              {UhdmType::IntTypespec, true, {}, {}, vpiStaticArray},
                                                              "int",
                                                              UhdmType::StringTypespec,
                                                              vpiStringConst}));
INSTANTIATE_TEST_SUITE_P(
    ConvSysFuncs, ConvSysFuncTest,
    testing::Values(
        // clang-format off
        ConvTestParam{"MathFunc205", "$cast", {{"real", UhdmType::RealTypespec, true}, {"10",        UhdmType::IntTypespec,   false}}, "10.0", UhdmType::RealTypespec, true}
        // clang-format on
    ));
#endif

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
