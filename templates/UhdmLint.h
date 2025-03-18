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
 * File:   UhdmLint.h
 * Author: alaindargelas
 *
 * Created on Jan 3, 2022, 9:03 PM
 */

#ifndef UHDM_UHDMLINT_H
#define UHDM_UHDMLINT_H

#include <uhdm/VpiListener.h>

namespace uhdm {
class Serializer;
class UhdmLint final : public VpiListener {
 public:
  UhdmLint(Serializer* serializer, Design* des)
      : m_serializer(serializer), m_design(des) {}

 private:
  void leaveBitSelect(const BitSelect* object, vpiHandle handle) override;

  void leaveFunction(const Function* object, vpiHandle handle) override;

  void leaveStructTypespec(const StructTypespec* object,
                           vpiHandle handle) override;

  void leaveModule(const Module* object, vpiHandle handle) override;

  void leaveAssignment(const Assignment* object, vpiHandle handle) override;

  void leaveLogicNet(const LogicNet* object, vpiHandle handle) override;

  void leaveEnumTypespec(const EnumTypespec* object, vpiHandle handle) override;

  void leavePropertySpec(const PropertySpec* object, vpiHandle handle) override;

  void leaveSysFuncCall(const SysFuncCall* object, vpiHandle handle) override;

  void leavePort(const Port* object, vpiHandle handle) override;

  void checkMultiContAssign(const std::vector<ContAssign*>* assigns);

  Serializer* m_serializer = nullptr;
  Design* m_design = nullptr;
};

}  // namespace uhdm

#endif  // UHDM_UHDMLINT_H
