// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <algorithm>
#include <iostream>
#include <memory>
#include <stack>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "uhdm/UhdmListener.h"
#include "uhdm/uhdm.h"

using namespace UHDM;
using testing::ElementsAre;

class MyUhdmListener final : public UhdmListener {
 protected:
  void enterModule_inst(const module_inst* object) override {
    CollectLine("Module", object);
    stack_.push(object);
  }

  void leaveModule_inst(const module_inst* object) override {
    ASSERT_EQ(stack_.top(), object);
    stack_.pop();
  }

  void enterPackage(const package* object) override {
    CollectLine("Package", object);
    stack_.push(object);
  }

  void leavePackage(const package* object) override {
    ASSERT_EQ(stack_.top(), object);
    stack_.pop();
  }

  void enterProgram(const program* object) override {
    CollectLine("Program", object);
    stack_.push(object);
  }

  void leaveProgram(const program* object) override {
    ASSERT_EQ(stack_.top(), object);
    stack_.pop();
  }

 public:
  void CollectLine(const std::string& prefix, const BaseClass* object) {
    if (visited.find(object) != visited.end()) return;
    std::string parentName;
    if (object->VpiParent() != nullptr)
      parentName = object->VpiParent()->VpiName();
    if (parentName.empty()) parentName = "-";
    std::string line;
    line.append(prefix)
        .append(": ")
        .append(object->VpiName())
        .append("/")
        .append(object->VpiDefName())
        .append(" parent: ")
        .append(parentName);
    collected_.emplace_back(std::move(line));
  }

  const std::vector<std::string>& collected() const { return collected_; }

 private:
  std::vector<std::string> collected_;
  std::stack<const BaseClass*> stack_;
};

static design* buildModuleProg(Serializer* s) {
  // Design building
  design* d = s->MakeDesign();
  d->VpiName("design1");

  // Module
  module_inst* m1 = s->MakeModule_inst();
  m1->VpiTopModule(true);
  m1->VpiDefName("M1");
  m1->VpiFullName("top::M1");
  m1->VpiParent(d);

  // Module
  module_inst* m2 = s->MakeModule_inst();
  m2->VpiDefName("M2");
  m2->VpiName("u1");
  m2->VpiParent(m1);

  // Module
  module_inst* m3 = s->MakeModule_inst();
  m3->VpiDefName("M3");
  m3->VpiName("u2");
  m3->VpiParent(m1);

  // Instance
  module_inst* m4 = s->MakeModule_inst();
  m4->VpiDefName("M4");
  m4->VpiName("u3");
  m4->VpiParent(m3);
  m4->Instance(m3);

  // Package
  package* p1 = s->MakePackage();
  p1->VpiName("P1");
  p1->VpiDefName("P0");
  p1->VpiParent(d);

  // Instance items, illustrates the use of groups
  program* pr1 = s->MakeProgram();
  pr1->VpiDefName("PR1");
  pr1->VpiParent(d);

  return d;
}

TEST(UhdmListenerTest, ProgramModule) {
  Serializer serializer;
  const UHDM::design* const design = buildModuleProg(&serializer);

  std::unique_ptr<MyUhdmListener> listener(new MyUhdmListener());
  listener->listenDesign(design);
  const std::vector<std::string> expected = {
      "Package: P1/P0 parent: design1",
      "Program: /PR1 parent: design1",
      "Module: /M1 parent: design1",
      "Module: u1/M2 parent: -",
      "Module: u2/M3 parent: -",
      "Module: u3/M4 parent: u2",
  };
  EXPECT_EQ(listener->collected(), expected);
  EXPECT_TRUE(listener->didVisitAll(serializer));
}
