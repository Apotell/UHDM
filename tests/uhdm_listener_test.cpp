// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include <algorithm>
#include <iostream>
#include <memory>
#include <stack>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "uhdm/UhdmListener.h"
#include "uhdm/uhdm.h"

using namespace uhdm;
using testing::ElementsAre;

class MyUhdmListener final : public UhdmListener {
 protected:
  void enterModule(const Module* object, uint32_t vpiRelation) override {
    CollectLine("Module", object);
    stack_.push(object);
  }

  void leaveModule(const Module* object, uint32_t vpiRelation) override {
    ASSERT_EQ(stack_.top(), object);
    stack_.pop();
  }

  void enterPackage(const Package* object, uint32_t vpiRelation) override {
    CollectLine("Package", object);
    stack_.push(object);
  }

  void leavePackage(const Package* object, uint32_t vpiRelation) override {
    ASSERT_EQ(stack_.top(), object);
    stack_.pop();
  }

  void enterProgram(const Program* object, uint32_t vpiRelation) override {
    CollectLine("Program", object);
    stack_.push(object);
  }

  void leaveProgram(const Program* object, uint32_t vpiRelation) override {
    ASSERT_EQ(stack_.top(), object);
    stack_.pop();
  }

 public:
  void CollectLine(const std::string& prefix, const BaseClass* object) {
    if (m_visited.find(object) != m_visited.end()) return;
    std::string parentName;
    if (object->getParent() != nullptr)
      parentName = object->getParent()->getName();
    if (parentName.empty()) parentName = "-";
    std::string line;
    line.append(prefix)
        .append(": ")
        .append(object->getName())
        .append("/")
        .append(object->getDefName())
        .append(" parent: ")
        .append(parentName);
    collected_.emplace_back(std::move(line));
  }

  const std::vector<std::string>& collected() const { return collected_; }

 private:
  std::vector<std::string> collected_;
  std::stack<const BaseClass*> stack_;
};

static Design* buildModuleProg(Serializer* s) {
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
  m3->setParent(m1);

  // Instance
  Module* m4 = s->make<Module>();
  m4->setDefName("M4");
  m4->setName("u3");
  m4->setParent(m3);
  m4->setInstance(m3);

  // Package
  Package* p1 = s->make<Package>();
  p1->setName("P1");
  p1->setDefName("P0");
  p1->setParent(d);

  // Instance items, illustrates the use of groups
  Program* pr1 = s->make<Program>();
  pr1->setDefName("PR1");
  pr1->setParent(d);

  return d;
}

TEST(UhdmListenerTest, ProgramModule) {
  Serializer serializer;
  const Design* const design = buildModuleProg(&serializer);

  std::unique_ptr<MyUhdmListener> listener(new MyUhdmListener());
  listener->listenDesign(design);
  const std::vector<std::string> expected = {
      "Package: P1/P0 parent: design1", "Program: /PR1 parent: design1",
      "Module: /M1 parent: design1",    "Module: u1/M2 parent: -",
      "Module: u2/M3 parent: -",        "Module: u3/M4 parent: u2",
  };
  EXPECT_EQ(listener->collected(), expected);
  EXPECT_TRUE(listener->didVisitAll(serializer));
}
