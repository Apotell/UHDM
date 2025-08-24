// -*- c++ -*-

/*

 Copyright 2019-2022 Alain Dargelas

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
 * File:   UhdmAdjuster.h
 * Author: alaindargelas
 *
 * Created on Nov 29, 2022, 9:03 PM
 */

#ifndef UHDM_UHDMADJUSTER_H
#define UHDM_UHDMADJUSTER_H

#include <uhdm/VpiListener.h>

namespace uhdm {
class Serializer;
class UhdmAdjuster final : public VpiListener {
 public:
  UhdmAdjuster(Serializer* serializer, Design* design)
      : m_serializer(serializer), m_design(design) {}

 private:
  void leaveCaseStmt(const CaseStmt* object, vpiHandle handle) final;
  void leaveOperation(const Operation* object, vpiHandle handle) final;
  void leaveSysFuncCall(const SysFuncCall* object, vpiHandle handle) final;
  void leaveFuncCall(const FuncCall* object, vpiHandle handle) final;
  void leaveConstant(const Constant* object, vpiHandle handle) final;
  void enterModule(const Module* object, vpiHandle handle) final;
  void leaveModule(const Module* object, vpiHandle handle) final;
  void enterPackage(const Package* object, vpiHandle handle) final;
  void leavePackage(const Package* object, vpiHandle handle) final;
  void leaveCaseItem(const CaseItem* object, vpiHandle handle) final;
  void enterGenScope(const GenScope* object, vpiHandle handle) final;
  void leaveGenScope(const GenScope* object, vpiHandle handle) final;
  void leaveReturnStmt(const ReturnStmt* object, vpiHandle) final;
  const Any* resize(const Any* object, int32_t maxsize, bool is_unsigned);
  void updateParentWithReducedExpression(const Any* object, const Any* parent);

 private:
  Serializer* m_serializer = nullptr;
  Design* m_design = nullptr;
  const Scope* m_currentInstance = nullptr;
};

}  // namespace uhdm

#endif  // UHDM_UHDMADJUSTER_H
