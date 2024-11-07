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

#include <uhdm/Elaborator.h>
#include <uhdm/ExprEval.h>
#include <uhdm/clone_tree.h>
#include <uhdm/uhdm.h>

namespace UHDM {
static std::string_view ltrim_until(std::string_view str, char c) {
  auto it = str.find(c);
  if (it != std::string_view::npos) str.remove_prefix(it + 1);
  return str;
}

static void propagateParamAssign(param_assign* pass, const any* target) {
  UHDM_OBJECT_TYPE targetType = target->UhdmType();
  Serializer& s = *pass->GetSerializer();
  switch (targetType) {
    case uhdmclass_defn: {
      class_defn* defn = (class_defn*)target;
      const any* lhs = pass->Lhs();
      const std::string_view name = lhs->VpiName();
      if (VectorOfany* params = defn->Parameters()) {
        for (any* param : *params) {
          if (param->VpiName() == name) {
            VectorOfparam_assign* passigns = defn->Param_assigns();
            if (passigns == nullptr) {
              defn->Param_assigns(s.MakeParam_assignVec());
              passigns = defn->Param_assigns();
            }
            param_assign* pa = s.MakeParam_assign();
            pa->VpiParent(defn);
            pa->Lhs(param);
            pa->Rhs((any*)pass->Rhs());
            passigns->push_back(pa);
          }
        }
      }
      if (const UHDM::extends* ext = defn->Extends()) {
        propagateParamAssign(pass, ext->Class_typespec());
      }
      if (const auto vars = defn->Variables()) {
        for (auto var : *vars) {
          propagateParamAssign(pass, var);
        }
      }
      break;
    }
    case uhdmclass_var: {
      class_var* var = (class_var*)target;
      propagateParamAssign(pass, var->Typespec());
      break;
    }
    case uhdmclass_typespec: {
      class_typespec* defn = (class_typespec*)target;
      const any* lhs = pass->Lhs();
      const std::string_view name = lhs->VpiName();
      if (VectorOfany* params = defn->Parameters()) {
        for (any* param : *params) {
          if (param->VpiName() == name) {
            VectorOfparam_assign* passigns = defn->Param_assigns();
            if (passigns == nullptr) {
              defn->Param_assigns(s.MakeParam_assignVec());
              passigns = defn->Param_assigns();
            }
            param_assign* pa = s.MakeParam_assign();
            pa->VpiParent(defn);
            pa->Lhs(param);
            pa->Rhs((any*)pass->Rhs());
            passigns->push_back(pa);
          }
        }
      }
      if (const class_defn* def = defn->Class_defn()) {
        propagateParamAssign(pass, def);
      }
      break;
    }
    default:
      break;
  }
}

static any* bindClassTypespec(class_typespec* ctps, any* current,
                              std::string_view name, bool& found) {
  any* previous = nullptr;
  const class_defn* defn = ctps->Class_defn();
  while (defn) {
    if (defn->Variables()) {
      for (variables* var : *defn->Variables()) {
        if (var->VpiName() == name) {
          if (ref_obj* ro = any_cast<ref_obj*>(current)) {
            ro->Actual_group(var);
          }
          previous = var;
          found = true;
          break;
        }
      }
    }
    if (defn->Named_events()) {
      for (named_event* event : *defn->Named_events()) {
        if (event->VpiName() == name) {
          if (ref_obj* ro = any_cast<ref_obj*>(current)) {
            ro->Actual_group(event);
          }
          previous = event;
          found = true;
          break;
        }
      }
    }
    if (defn->Task_funcs()) {
      for (task_func* tf : *defn->Task_funcs()) {
        if (tf->VpiName() == name) {
          if (ref_obj* ro = any_cast<ref_obj*>(current)) {
            ro->Actual_group(tf);
          } else if (current->UhdmType() == uhdmmethod_func_call) {
            if (tf->UhdmType() == uhdmfunction)
              ((method_func_call*)current)->Function((function*)tf);
          } else if (current->UhdmType() == uhdmmethod_task_call) {
            if (tf->UhdmType() == uhdmtask)
              ((method_task_call*)current)->Task((task*)tf);
          }
          previous = tf;
          found = true;
          break;
        }
      }
    }
    if (found) break;

    const class_defn* base_defn = nullptr;
    if (const extends* ext = defn->Extends()) {
      if (const ref_typespec* rt = ext->Class_typespec()) {
        if (const class_typespec* tp = rt->Actual_typespec<class_typespec>()) {
          base_defn = tp->Class_defn();
        }
      }
    }
    defn = base_defn;
  }
  return previous;
}

void Elaborator::elabClass_defn(class_defn* clone) {
  if (!clone_) return;
  //<CLASS_ELABORATOR_LISTENER>
}

void Elaborator::enterTask_func(task_func* clone) {
  // Collect instance elaborated nets
  ComponentMap varMap;
  if (clone->Variables()) {
    for (variables* var : *clone->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (clone->Io_decls()) {
    for (io_decl* decl : *clone->Io_decls()) {
      varMap.emplace(decl->VpiName(), decl);
    }
  }
  varMap.emplace(clone->VpiName(), clone->Return());

  if (const any* parent = clone->VpiParent()) {
    if (parent->UhdmType() == uhdmclass_defn) {
      const class_defn* defn = (const class_defn*)parent;
      while (defn) {
        if (defn->Variables()) {
          for (any* var : *defn->Variables()) {
            varMap.emplace(var->VpiName(), var);
          }
        }

        const class_defn* base_defn = nullptr;
        if (const extends* ext = defn->Extends()) {
          if (const ref_typespec* rt = ext->Class_typespec()) {
            if (const class_typespec* ctps = rt->Actual_typespec<class_typespec>()) {
              base_defn = ctps->Class_defn();
            }
          }
        }
        defn = base_defn;
      }
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(clone, varMap, paramMap, funcMap, modMap);
}

void Elaborator::leaveTask_func(task_func* clone) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
  }
}

void Elaborator::enterGen_scope(gen_scope* clone) {
  // Collect instance elaborated nets

  ComponentMap netMap;
  if (clone->Nets()) {
    for (net* net : *clone->Nets()) {
      netMap.emplace(net->VpiName(), net);
    }
  }
  if (clone->Array_nets()) {
    for (array_net* net : *clone->Array_nets()) {
      netMap.emplace(net->VpiName(), net);
    }
  }

  if (clone->Variables()) {
    for (variables* var : *clone->Variables()) {
      netMap.emplace(var->VpiName(), var);
      if (var->UhdmType() == uhdmenum_var) {
        enum_var* evar = (enum_var*)var;
        enum_typespec* etps = (enum_typespec*)evar->Typespec();
        for (auto c : *etps->Enum_consts()) {
          netMap.emplace(c->VpiName(), c);
        }
      }
    }
  }

  // Collect instance parameters, defparams
  ComponentMap paramMap;
  if (clone->Parameters()) {
    for (any* param : *clone->Parameters()) {
      paramMap.emplace(param->VpiName(), param);
    }
  }
  if (clone->Def_params()) {
    for (def_param* param : *clone->Def_params()) {
      paramMap.emplace(param->VpiName(), param);
    }
  }

  ComponentMap funcMap;
  ComponentMap modMap;

  // Collect gen_scope
  if (clone->Gen_scope_arrays()) {
    for (gen_scope_array* gsa : *clone->Gen_scope_arrays()) {
      for (gen_scope* gs : *gsa->Gen_scopes()) {
        modMap.emplace(gsa->VpiName(), gs);
      }
    }
  }
  instStack_.emplace_back(clone, netMap, paramMap, funcMap, modMap);
}

void Elaborator::leaveGen_scope(gen_scope* clone) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
  }
}

void Elaborator::enterMethod_func_call(method_func_call* clone) {
  // TODO(HS): Follow up - This function never gets called!
  ComponentMap netMap;
  if (clone->Tf_call_args()) {
    for (auto arg : *clone->Tf_call_args()) {
      netMap.emplace(arg->VpiName(), arg);
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(clone, netMap, paramMap, funcMap, modMap);
}

void Elaborator::leaveMethod_func_call(method_func_call* clone) {
  // TODO(HS): Follow up - This function never gets called!
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
  }
}

void Elaborator::elabModule_inst(module_inst* clone) {
  bool topLevelModule = clone->VpiTopModule();
  const std::string_view instName = clone->VpiName();
  const std::string_view defName = clone->VpiDefName();
  bool flatModule =
      instName.empty() && ((clone->VpiParent() == 0) ||
                           ((clone->VpiParent() != 0) &&
                            (clone->VpiParent()->VpiType() != vpiModule)));
  // false when it is a module in a hierachy tree
  if (debug_) {
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << ", Top:" << topLevelModule
              << std::endl;
  }

  if (flatModule) {
    // Flat list of module (unelaborated)
    flatComponentMap_.emplace(clone->VpiDefName(), clone);
  } else {
    // Do not elab modules used in hier_path base, that creates a loop
    if (inCallstackOfType(uhdmhier_path)) {
      return;
    }
    if (!clone_) return;
    // Hierachical module list (elaborated)
    inHierarchy_ = true;
    ComponentMap::iterator itrDef = flatComponentMap_.find(defName);
    // Check if Module instance has a definition
    if (itrDef != flatComponentMap_.end()) {
      const any* comp = (*itrDef).second;
      if (comp->VpiType() != vpiModule) return;
      module_inst* defMod = (module_inst*)comp;
      //<MODULE_ELABORATOR_LISTENER>
    }
  }
}

void Elaborator::bindScheduledTaskFunc() {
  for (auto& call_prefix : scheduledTfCallBinding_) {
    tf_call* call = call_prefix.first;
    const class_var* prefix = call_prefix.second;
    if (call->UhdmType() == uhdmfunc_call) {
      if (function* f =
              any_cast<function*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((func_call*)call)->Function(f);
      }
    } else if (call->UhdmType() == uhdmtask_call) {
      if (task* f = any_cast<task*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((task_call*)call)->Task(f);
      }
    } else if (call->UhdmType() == uhdmmethod_func_call) {
      if (function* f =
              any_cast<function*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((method_func_call*)call)->Function(f);
      }
    } else if (call->UhdmType() == uhdmmethod_task_call) {
      if (task* f = any_cast<task*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((method_task_call*)call)->Task(f);
      }
    }
  }
  scheduledTfCallBinding_.clear();
}

any* Elaborator::bindNet(std::string_view name) const {
  for (InstStack::const_reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    if (ignoreLastInstance_) {
      if (i == instStack_.rbegin()) continue;
    }
    const ComponentMap& netMap = std::get<1>(*i);
    ComponentMap::const_iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      const any* p = netItr->second;
      if (const ref_obj* r = any_cast<const ref_obj*>(p)) {
        p = r->Actual_group();
      }
      return const_cast<any*>(p);
    }
  }
  return nullptr;
}

// Bind to a net or parameter in the current instance
any* Elaborator::bindAny(std::string_view name) const {
  for (InstStack::const_reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    if (ignoreLastInstance_) {
      if (i == instStack_.rbegin()) continue;
    }
    const ComponentMap& netMap = std::get<1>(*i);
    ComponentMap::const_iterator netItr = netMap.find(name);
    if (netItr != netMap.end()) {
      const any* p = netItr->second;
      if (const ref_obj* r = any_cast<const ref_obj*>(p)) {
        p = r->Actual_group();
      }
      return const_cast<any*>(p);
    }

    const ComponentMap& paramMap = std::get<2>(*i);
    ComponentMap::const_iterator paramItr = paramMap.find(name);
    if (paramItr != paramMap.end()) {
      const any* p = paramItr->second;
      if (const ref_obj* r = any_cast<const ref_obj*>(p)) {
        p = r->Actual_group();
      }
      return const_cast<any*>(p);
    }

    const ComponentMap& modMap = std::get<4>(*i);
    ComponentMap::const_iterator modItr = modMap.find(name);
    if (modItr != modMap.end()) {
      const any* p = modItr->second;
      if (const ref_obj* r = any_cast<const ref_obj*>(p)) {
        p = r->Actual_group();
      }
      return const_cast<any*>(p);
    }
  }
  return nullptr;
}

// Bind to a param in the current instance
any* Elaborator::bindParam(std::string_view name) const {
  for (InstStack::const_reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    if (ignoreLastInstance_) {
      if (i == instStack_.rbegin()) continue;
    }
    const ComponentMap& paramMap = std::get<2>(*i);
    ComponentMap::const_iterator paramItr = paramMap.find(name);
    if (paramItr != paramMap.end()) {
      const any* p = paramItr->second;
      if (const ref_obj* r = any_cast<const ref_obj*>(p)) {
        p = r->Actual_group();
      }
      return const_cast<any*>(p);
    }
  }
  return nullptr;
}

// Bind to a function or task in the current scope
any* Elaborator::bindTaskFunc(std::string_view name,
                              const class_var* prefix) const {
  for (InstStack::const_reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    if (ignoreLastInstance_) {
      if (i == instStack_.rbegin()) continue;
    }
    const ComponentMap& funcMap = std::get<3>(*i);
    ComponentMap::const_iterator funcItr = funcMap.find(name);
    if (funcItr != funcMap.end()) {
      const any* p = funcItr->second;
      if (const ref_obj* r = any_cast<const ref_obj*>(p)) {
        p = r->Actual_group();
      }
      return const_cast<any*>(p);
    }
  }
  if (prefix) {
    const typespec* tps = nullptr;
    if (const ref_typespec* rt = prefix->Typespec()) {
      tps = rt->Actual_typespec();
    }
    if (tps && tps->UhdmType() == uhdmclass_typespec) {
      const class_defn* defn = ((const class_typespec*)tps)->Class_defn();
      while (defn) {
        if (defn->Task_funcs()) {
          for (task_func* tf : *defn->Task_funcs()) {
            if (tf->VpiName() == name) return tf;
          }
        }

        const class_defn* base_defn = nullptr;
        if (const extends* ext = defn->Extends()) {
          if (const ref_typespec* rt = ext->Class_typespec()) {
            if (const class_typespec* ctps =
                    rt->Actual_typespec<class_typespec>()) {
              base_defn = ctps->Class_defn();
            }
          }
        }
        defn = base_defn;
      }
    }
  }
  return nullptr;
}

bool Elaborator::isFunctionCall(std::string_view name,
                                const expr* prefix) const {
  for (InstStack::const_reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    const ComponentMap& funcMap = std::get<3>(*i);
    ComponentMap::const_iterator funcItr = funcMap.find(name);
    if (funcItr != funcMap.end()) {
      return (funcItr->second->UhdmType() == uhdmfunction);
    }
  }
  if (prefix) {
    if (const ref_obj* ref = any_cast<const ref_obj*>(prefix)) {
      if (const class_var* vprefix = ref->Actual_group<class_var>()) {
        if (const any* func = bindTaskFunc(name, vprefix)) {
          return (func->UhdmType() == uhdmfunction);
        }
      }
    }
  }
  return true;
}

bool Elaborator::isTaskCall(std::string_view name, const expr* prefix) const {
  for (InstStack::const_reverse_iterator i = instStack_.rbegin();
       i != instStack_.rend(); ++i) {
    const ComponentMap& funcMap = std::get<3>(*i);
    ComponentMap::const_iterator funcItr = funcMap.find(name);
    if (funcItr != funcMap.end()) {
      return (funcItr->second->UhdmType() == uhdmtask);
    }
  }
  if (prefix) {
    if (const ref_obj* ref = any_cast<const ref_obj*>(prefix)) {
      if (const class_var* vprefix = ref->Actual_group<class_var>()) {
        if (const any* task = bindTaskFunc(name, vprefix)) {
          return (task->UhdmType() == uhdmtask);
        }
      }
    }
  }
  return true;
}

void Elaborator::pushVar(any* var) {
  ComponentMap netMap;
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  netMap.emplace(var->VpiName(), var);
  instStack_.emplace_back(var, netMap, paramMap, funcMap, modMap);
}

void Elaborator::popVar(any* var) {
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == var)) {
    instStack_.pop_back();
  }
}

void Elaborator::elaborate(vpiHandle source, vpiHandle parent) {
  cloneAny((any*)((const uhdm_handle*)source)->object,
           (any*)(parent ? ((const uhdm_handle*)parent)->object : nullptr));
}

void Elaborator::elaborate(const std::vector<vpiHandle>& sources) {
  for (vpiHandle vh : sources) {
    cloneAny((any*)((const uhdm_handle*)vh)->object, nullptr);
  }
}

void Elaborator::elaborate(const any* source, any* parent) {
  cloneAny(source, parent);
}

void Elaborator::elaborate(const std::vector<const any*>& sources) {
  for (const any* source : sources) {
    cloneAny(source, nullptr);
  }
}
}  // namespace UHDM
