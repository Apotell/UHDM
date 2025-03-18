/*

 Copyright 2021 Alain Dargelas

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

/*
 * File:   group_membership_test
 * Author: alain
 *
 * Created on Dec 22, 2021, 10:03 PM
 */

#include <iostream>
#include <stack>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "uhdm/uhdm.h"

using namespace uhdm;

//-------------------------------------------

TEST(GroupTest, Membership) {
  Serializer s;
  SequenceInst* inst = s.make<SequenceInst>();
  AnyCollection* exprs = s.makeCollection<Any>();
  Constant* legal = s.make<Constant>();
  exprs->push_back(legal);
  Module* illegal = s.make<Module>();
  inst->setArguments(exprs);
  AnyCollection* all_legal = inst->getArguments();
  EXPECT_EQ(all_legal->size(), 1);

  SequenceInst* inst2 = s.make<SequenceInst>();
  AnyCollection* exprs2 = s.makeCollection<Any>();
  exprs2->push_back(illegal);
  inst2->setArguments(exprs2);
  AnyCollection* all_illegal = inst2->getArguments();
  EXPECT_EQ(all_illegal, nullptr);
}
