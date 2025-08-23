// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <uhdm/uhdm_types.h>

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test_util.h"
#include "uhdm/UhdmComparer.h"
#include "uhdm/uhdm.h"

using namespace uhdm;
using testing::ElementsAre;

class MyUhdmComparer final : public UhdmComparer {
 public:
  enum class TestCase { EQ, NE, SPECIFIC };
  int32_t m_found = 0;

  using UhdmComparer::compare;
  int32_t compare(const BaseClass* lhsObj, bool lhs, const BaseClass* rhsObj,
                  bool rhs, uint32_t vpiRelation, int32_t r) {
    if (lhs != rhs) {
      ++m_found;
      return 0;
    }
    return UhdmComparer::compare(lhsObj, lhs, rhsObj, rhs, vpiRelation, r);
  }
};

static std::vector<Design*> buildModuleProg(
    Serializer* s, MyUhdmComparer::TestCase testSelector) {
  std::vector<Design*> designs;
  for (int32_t i = 0; i < 2; i++) {
    // Design building
    Design* d = s->make<Design>();
    d->setName("design1");

    // Module
    Module* m1 = s->make<Module>();
    m1->setTopModule(true);
    m1->setDefName("M1");
    m1->setFullName("top::M1");
    m1->setParent(d);

    // Module
    Module* m2 = s->make<Module>();
    m2->setDefName("M2");
    m2->setName("u1");
    m2->setParent(m1);

    // Module
    Module* m3 = s->make<Module>();
    m3->setDefName("M3");
    m3->setName("u2");
    if (testSelector == MyUhdmComparer::TestCase::NE) {
      if (i == 0) {
        m3->setParent(m1);
      } else if (i == 1) {
        m3->setParent(m2);
      }
    } else {
      m3->setParent(m1);
    }

    // Instance
    Module* m4 = s->make<Module>();
    m4->setDefName("M4");
    m4->setName("u3");
    if (testSelector == MyUhdmComparer::TestCase::NE) {
      if (i == 0) {
        m4->setInstance(m2);
      } else if (i == 1) {
        m4->setInstance(m3);
      }
    } else {
      m3->setParent(m3);
      m4->setInstance(m3);
    }

    Module* m5 = s->make<Module>();
    if (testSelector == MyUhdmComparer::TestCase::SPECIFIC) {
      // Module
      m5->setDefName("M5");
      m5->setFullName("top::M1");
      m5->setParent(d);
      if (i == 0) {
        m5->setTopModule(true);
      } else if (i == 1) {
        m5->setTopModule(false);
      }
    }

    // Package
    Package* p1 = s->make<Package>();
    p1->setName("P1");
    p1->setDefName("P0");
    p1->setParent(d);

    // Instance items, illustrates the use of groups
    Program* pr1 = s->make<Program>();
    pr1->setDefName("PR1");
    pr1->setParent(d);

    designs.push_back(d);
  }
  return designs;
}

TEST(UhdmComparerTest, Equalitytest) {
  Serializer serializer;
  const std::vector<Design*> designs =
      buildModuleProg(&serializer, MyUhdmComparer::TestCase::EQ);

  MyUhdmComparer comparer;
  EXPECT_EQ(comparer.compare(designs[0], designs[1]), 0);
}

TEST(UhdmComparerTest, NonEqualitytest) {
  Serializer serializer;
  const std::vector<Design*> designs =
      buildModuleProg(&serializer, MyUhdmComparer::TestCase::NE);

  MyUhdmComparer comparer;
  EXPECT_NE(comparer.compare(designs[0], designs[1]), 0);
}

TEST(UhdmComparerTest, SpecificComparer) {
  Serializer serializer;
  const std::vector<Design*> designs =
      buildModuleProg(&serializer, MyUhdmComparer::TestCase::SPECIFIC);

  MyUhdmComparer comparer;
  EXPECT_EQ(comparer.compare(designs[0], designs[0]), 0);
  EXPECT_EQ(comparer.m_found, 0);

  MyUhdmComparer comparer1;
  EXPECT_EQ(comparer1.compare(designs[0], designs[1]), 0);
  EXPECT_EQ(comparer1.m_found, 1);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
