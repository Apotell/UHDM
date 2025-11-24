// -*- c++ -*-

/*

 Copyright 2019-2020 Alain Dargelas

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
 * File:   Elaborator.h
 * Author: hs
 *
 * Created on November 24, 2025, 10:03 PM
 */

#ifndef UHDM_ELABORATOR_H
#define UHDM_ELABORATOR_H

#include <uhdm/Cloner.h>
#include <uhdm/VpiListener.h>
#include <uhdm/containers.h>

#include <map>
#include <vector>

namespace UHDM {
class Serializer;

class Elaborator final : public VpiListener, public Cloner {
 public:
  explicit Elaborator(Serializer* serializer, bool debug = false,
                      bool muteErrors = false);

  void uniquifyTypespec(bool uniquify) { m_uniquifyTypespec = uniquify; }
  bool uniquifyTypespec() const { return m_uniquifyTypespec; }
  void bindOnly(bool bindOnly) { m_clone = !bindOnly; }
  bool bindOnly() const { return !m_clone; }
  bool isFunctionCall(std::string_view name, const expr* prefix) const;
  bool muteErrors() const { return m_muteErrors; }
  bool isTaskCall(std::string_view name, const expr* prefix) const;
  void ignoreLastInstance(bool ignore) override { m_ignoreLastInstance = ignore; }
  using Cloner::clone;

 private:
  // Bind to a net in the current instance
  any* bindNet(std::string_view name) const;

  // Bind to a net or parameter in the current instance
  any* bindAny(std::string_view name) const;

  // Bind to a param in the current instance
  any* bindParam(std::string_view name) const;

  // Bind to a function or task in the current scope
  any* bindTaskFunc(std::string_view name,
                    const class_var* prefix = nullptr) const;

  void scheduleTaskFuncBinding(tf_call* clone, const class_var* prefix) {
    m_scheduledTfCallBinding.push_back(std::make_pair(clone, prefix));
  }
  void bindScheduledTaskFunc();

  void enterAny(const any* object, vpiHandle handle) final;

  void leaveDesign(const design* object, vpiHandle handle) final;

  void enterModule_inst(const module_inst* object, vpiHandle handle) final;
  void leaveModule_inst(const module_inst* object, vpiHandle handle) final;

  void enterInterface_inst(const interface_inst* object,
                           vpiHandle handle) final;
  void leaveInterface_inst(const interface_inst* object,
                           vpiHandle handle) final;

  void enterPackage(const package* object, vpiHandle handle) final;
  void leavePackage(const package* object, vpiHandle handle) final;

  void enterClass_defn(const class_defn* object, vpiHandle handle) final;
  void leaveClass_defn(const class_defn* object, vpiHandle handle) final;

  void enterGen_scope(const gen_scope* object, vpiHandle handle) final;
  void leaveGen_scope(const gen_scope* object, vpiHandle handle) final;

  void leaveRef_obj(const ref_obj* object, vpiHandle handle) final;
  void leaveBit_select(const bit_select* object, vpiHandle handle) final;
  void leaveIndexed_part_select(const indexed_part_select* object,
                                vpiHandle handle) final;
  void leavePart_select(const part_select* object, vpiHandle handle) final;
  void leaveVar_select(const var_select* object, vpiHandle handle) final;

  void enterFunction(const function* object, vpiHandle handle) final;
  void leaveFunction(const function* object, vpiHandle handle) final;

  void enterTask(const task* object, vpiHandle handle) final;
  void leaveTask(const task* object, vpiHandle handle) final;

  void enterForeach_stmt(const foreach_stmt* object, vpiHandle handle) final;
  void leaveForeach_stmt(const foreach_stmt* object, vpiHandle handle) final;

  void enterFor_stmt(const for_stmt* object, vpiHandle handle) final;
  void leaveFor_stmt(const for_stmt* object, vpiHandle handle) final;

  void enterBegin(const begin* object, vpiHandle handle) final;
  void leaveBegin(const begin* object, vpiHandle handle) final;

  void enterNamed_begin(const named_begin* object, vpiHandle handle) final;
  void leaveNamed_begin(const named_begin* object, vpiHandle handle) final;

  void enterFork_stmt(const fork_stmt* object, vpiHandle handle) final;
  void leaveFork_stmt(const fork_stmt* object, vpiHandle handle) final;

  void enterNamed_fork(const named_fork* object, vpiHandle handle) final;
  void leaveNamed_fork(const named_fork* object, vpiHandle handle) final;

  void enterMethod_func_call(const method_func_call* object,
                             vpiHandle handle) final;
  void leaveMethod_func_call(const method_func_call* object,
                             vpiHandle handle) final;

  void pushVar(any* var);
  void popVar(any* var);

  any* bindClassTypespec(class_typespec* ctps, any* current,
                         std::string_view name, bool& found);

  any* cloneAny(const any* source, any* parent) final;

  constant* clone(const constant* source, any* parent) final;
  cont_assign* clone(const cont_assign* source, any* parent) final;
  function* clone(const function* source, any* parent) final;
  gen_scope_array* clone(const gen_scope_array* source, any* parent) final;
  hier_path* clone(const hier_path* source, any* parent) final;
  sys_func_call* clone(const sys_func_call* source, any* parent) final;
  sys_task_call* clone(const sys_task_call* source, any* parent) final;
  tagged_pattern* clone(const tagged_pattern* source, any* parent) final;
  task* clone(const task* source, any* parent) final;
  tf_call* clone(const func_call* source, any* parent) final;
  tf_call* clone(const method_func_call* source, any* parent) final;
  tf_call* clone(const method_task_call* source, any* parent) final;
  tf_call* clone(const task_call* source, any* parent) final;

  typespec* clone(const array_typespec* source, any* parent) final;
  typespec* clone(const bit_typespec* source, any* parent) final;
  typespec* clone(const byte_typespec* source, any* parent) final;
  typespec* clone(const chandle_typespec* source, any* parent) final;
  typespec* clone(const class_typespec* source, any* parent) final;
  typespec* clone(const enum_typespec* source, any* parent) final;
  typespec* clone(const event_typespec* source, any* parent) final;
  typespec* clone(const import_typespec* source, any* parent) final;
  typespec* clone(const int_typespec* source, any* parent) final;
  typespec* clone(const integer_typespec* source, any* parent) final;
  typespec* clone(const interface_typespec* source, any* parent) final;
  typespec* clone(const logic_typespec* source, any* parent) final;
  typespec* clone(const long_int_typespec* source, any* parent) final;
  typespec* clone(const module_typespec* source, any* parent) final;
  typespec* clone(const packed_array_typespec* source, any* parent) final;
  typespec* clone(const property_typespec* source, any* parent) final;
  typespec* clone(const real_typespec* source, any* parent) final;
  typespec* clone(const sequence_typespec* source, any* parent) final;
  typespec* clone(const short_int_typespec* source, any* parent) final;
  typespec* clone(const short_real_typespec* source, any* parent) final;
  typespec* clone(const string_typespec* source, any* parent) final;
  typespec* clone(const struct_typespec* source, any* parent) final;
  typespec* clone(const time_typespec* source, any* parent) final;
  typespec* clone(const type_parameter* source, any* parent) final;
  typespec* clone(const union_typespec* source, any* parent) final;
  typespec* clone(const unsupported_typespec* source, any* parent) final;
  typespec* clone(const void_typespec* source, any* parent) final;

  // clang-format off
  using Cloner::copy;
//<COPY_DECLARATIONS>
  // clang-format on

 private:
  void enterVariables(const variables* object, vpiHandle handle);

  void enterTask_func(const task_func* object, vpiHandle handle);
  void leaveTask_func(const task_func* object, vpiHandle handle);

  using ComponentMap = std::map<std::string, const BaseClass*, std::less<>>;
  // Instance context stack
  using InstStack = std::vector<std::tuple<const BaseClass*, ComponentMap, ComponentMap,
                                 ComponentMap, ComponentMap>>;
  using ScheduledTfCallBinding = std::vector<std::pair<tf_call*, const class_var*>>;

  bool m_debug = false;
  bool m_muteErrors = false;
  InstStack m_instStack;
  // Flat list of components (modules, udps, interfaces)
  ComponentMap m_flatComponentMap;
  ScheduledTfCallBinding m_scheduledTfCallBinding;
  bool m_inHierarchy = false;
  bool m_uniquifyTypespec = true;
  bool m_clone = true;
  bool m_ignoreLastInstance = false;
  bool m_isInUhdmAllIterator = false;
};
};  // namespace UHDM

#endif  // UHDM_ELABORATOR_H
