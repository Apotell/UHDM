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

#include <uhdm/BaseClass.h>
#include <uhdm/Cloner.h>

#include <map>
#include <unordered_set>
#include <vector>

namespace UHDM {
class Serializer;

class Elaborator final : public Cloner {
  UHDM_IMPLEMENT_RTTI(Elaborator, Cloner)

 public:
  void uniquifyTypespec(bool uniquify) { uniquifyTypespec_ = uniquify; }
  bool uniquifyTypespec() const { return uniquifyTypespec_; }

  void bindOnly(bool bindOnly) { clone_ = !bindOnly; }
  bool bindOnly() const { return !clone_; }

  void ignoreLastInstance(bool ignore) { ignoreLastInstance_ = ignore; }

  bool muteErrors() const { return muteErrors_; }
  bool isTaskCall(std::string_view name, const expr* prefix) const;
  bool isFunctionCall(std::string_view name, const expr* prefix) const;
  bool isInUhdmAllIterator() const { return uhdmAllIterator_; }

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
    scheduledTfCallBinding_.push_back(std::make_pair(clone, prefix));
  }
  void bindScheduledTaskFunc();

  any* cloneBegin(const begin* source, any* parent) final;
  any* cloneBit_select(const bit_select* source, any* parent) final;
  any* cloneClass_defn(const class_defn* source, any* parent) final;
  any* cloneClass_var(const class_var* source, any* parent) final;
  any* cloneConstant(const constant* source, any* parent) final;
  any* cloneCont_assign(const cont_assign* source, any* parent) final;
  any* cloneDesign(const design* source, any* parent) final;
  any* cloneFor_stmt(const for_stmt* source, any* parent) final;
  any* cloneForeach_stmt(const foreach_stmt* source, any* parent) final;
  any* cloneFork_stmt(const fork_stmt* source, any* parent) final;
  any* cloneFunc_call(const func_call* source, any* parent) final;
  any* cloneFunction(const function* source, any* parent) final;
  any* cloneGen_scope(const gen_scope* source, any* parent) final;
  any* cloneGen_scope_array(const gen_scope_array* source, any* parent) final;
  any* cloneHier_path(const hier_path* source, any* parent) final;
  any* cloneIndexed_part_select(const indexed_part_select* object,
                                any* parent) final;
  any* cloneInterface_inst(const interface_inst* source, any* parent) final;
  any* cloneMethod_func_call(const method_func_call* source, any* parent) final;
  any* cloneMethod_task_call(const method_task_call* source, any* parent) final;
  any* cloneModule_inst(const module_inst* source, any* parent) final;
  any* cloneNamed_begin(const named_begin* source, any* parent) final;
  any* cloneNamed_fork(const named_fork* source, any* parent) final;
  any* clonePackage(const package* source, any* parent) final;
  any* clonePart_select(const part_select* source, any* parent) final;
  any* cloneRef_obj(const ref_obj* source, any* parent) final;
  any* cloneSys_func_call(const sys_func_call* source, any* parent) final;
  any* cloneSys_task_call(const sys_task_call* source, any* parent) final;
  any* cloneTagged_pattern(const tagged_pattern* source, any* parent) final;
  any* cloneTask(const task* source, any* parent) final;
  any* cloneTask_call(const task_call* source, any* parent) final;
  any* cloneVar_select(const var_select* source, any* parent) final;

  void pushVar(any* var);
  void popVar(any* var);

  void elaborate(vpiHandle source, vpiHandle parent);
  void elaborate(const std::vector<vpiHandle>& sources);

  void elaborate(const any* source, any* parent);
  void elaborate(const std::vector<const any*>& sources);

  explicit Elaborator(Serializer* serializer, bool debug = false,
                              bool muteErrors = false)
      : Cloner(serializer), debug_(debug), muteErrors_(muteErrors) {}

 private:
  void elabModule_inst(module_inst* clone);
  void elabClass_defn(class_defn* clone);

  void enterGen_scope(gen_scope* clone);
  void leaveGen_scope(gen_scope* clone);

  void enterMethod_func_call(method_func_call* clone);
  void leaveMethod_func_call(method_func_call* clone);

  void enterTask_func(task_func* clone);
  void leaveTask_func(task_func* clone);

  // Instance context stack
  typedef std::map<std::string_view, const any*, std::less<>> ComponentMap;
  typedef std::vector<std::tuple<const any*, ComponentMap, ComponentMap,
                                 ComponentMap, ComponentMap>>
      InstStack;
  InstStack instStack_;

  // Flat list of components (modules, udps, interfaces)
  ComponentMap flatComponentMap_;

  bool inHierarchy_ = false;
  bool debug_ = false;
  bool muteErrors_ = false;
  bool uniquifyTypespec_ = true;
  bool clone_ = true;
  bool ignoreLastInstance_ = false;
  bool uhdmAllIterator_ = true;
  std::vector<std::pair<tf_call*, const class_var*>> scheduledTfCallBinding_;
};
};  // namespace UHDM

#endif  // UHDM_ELABORATOR_H
