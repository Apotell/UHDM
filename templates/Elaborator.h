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
 * Author: alaindargelas
 *
 * Created on May 6, 2020, 10:03 PM
 */

#ifndef UHDM_ELABORATOR_H
#define UHDM_ELABORATOR_H

#include <uhdm/VpiListener.h>
#include <uhdm/containers.h>

#include <map>
#include <unordered_set>
#include <vector>

namespace UHDM {

class Elaborator;
class Serializer;

class Elaborator final : public VpiListener {
 public:
  explicit Elaborator(Serializer* serializer, bool debug = false,
                      bool muteErrors = false);

  template <typename T>
  T* clone(const T* source, any* parent) {
    return (T*)DeepCloneAny((const any *)source, parent);
  }

  void uniquifyTypespec(bool uniquify) { m_uniquifyTypespec = uniquify; }
  bool uniquifyTypespec() const { return m_uniquifyTypespec; }
  void bindOnly(bool bindOnly) { m_clone = !bindOnly; }
  bool bindOnly() const { return !m_clone; }
  bool isFunctionCall(std::string_view name, const expr* prefix) const;
  bool muteErrors() const { return m_muteErrors; }
  bool isTaskCall(std::string_view name, const expr* prefix) const;
  void ignoreLastInstance(bool ignore) override { m_ignoreLastInstance = ignore; }

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
  void elabModule_inst(const module_inst* object, vpiHandle handle);
  void leaveModule_inst(const module_inst* object, vpiHandle handle) final;

  void enterInterface_inst(const interface_inst* object,
                           vpiHandle handle) final;
  void leaveInterface_inst(const interface_inst* object,
                           vpiHandle handle) final;

  void enterPackage(const package* object, vpiHandle handle) final;
  void leavePackage(const package* object, vpiHandle handle) final;

  void enterClass_defn(const class_defn* object, vpiHandle handle) final;
  void elabClass_defn(const class_defn* object, vpiHandle handle);
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

  any* DeepCloneAny(const any* source, any* parent);

  template <typename T>
  std::vector<T*>* Clone(const std::vector<T*>* source);
  template<typename T>
  std::vector<T*>* DeepClone(const std::vector<T*>* source, any* parent);

  template<typename T>
  T* DeepClone(const T* source, any* parent);
  sys_func_call* DeepClone(const sys_func_call* source, any* parent);
  sys_task_call* DeepClone(const sys_task_call* source, any* parent);
  tf_call* DeepClone(const method_func_call* source, any* parent);
  constant* DeepClone(const constant* source, any* parent);
  tagged_pattern* DeepClone(const tagged_pattern* source, any* parent);
  tf_call* DeepClone(const method_task_call* source, any* parent);
  tf_call* DeepClone(const func_call* source, any* parent);
  tf_call* DeepClone(const task_call* source, any* parent);
  gen_scope_array* DeepClone(const gen_scope_array* source, any* parent);
  function* DeepClone(const function* source, any* parent);
  task* DeepClone(const task* source, any* parent);
  cont_assign* DeepClone(const cont_assign* source, any* parent);
  hier_path* DeepClone(const hier_path* source, any* parent);

  // clang-format off
  void DeepCopy(const any* source, any* target);
//<COPY_ANY_DECLARATIONS>
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

  Serializer* const m_serializer = nullptr;
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
