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
