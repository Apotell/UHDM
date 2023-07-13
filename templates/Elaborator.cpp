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
      if (const class_typespec* tp = ext->Class_typespec()) {
        base_defn = tp->Class_defn();
      }
    }
    defn = base_defn;
  }
  return previous;
}

any* Elaborator::cloneBegin(const begin* source, any* parent) {
  begin* const clone = m_serializer->MakeBegin();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);

  ComponentMap varMap;
  if (clone->Array_vars()) {
    for (variables* var : *clone->Array_vars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (clone->Variables()) {
    for (variables* var : *clone->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(clone, varMap, paramMap, funcMap, modMap);

  source->DeepCopy(clone, parent, this);

  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
  }

  return clone;
}

any* Elaborator::cloneBit_select(const bit_select* source, any* parent) {
  any* const clone = basetype_t::cloneBit_select(source, parent);
  if (any* res = bindAny(clone->VpiName())) {
    static_cast<bit_select*>(clone)->Actual_group(res);
  }
  return clone;
}

void Elaborator::elabClass_defn(class_defn* clone) {
  if (!clone_) return;
//<CLASS_ELABORATOR_LISTENER>
}

any* Elaborator::cloneClass_defn(const class_defn* source, any* parent) {
  class_defn* const clone = const_cast<class_defn*>(source);

  ComponentMap varMap;
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;

  const class_defn* defn = clone;
  while (defn != nullptr) {
    // Collect instance elaborated nets
    if (defn->Variables()) {
      for (variables* var : *defn->Variables()) {
        varMap.emplace(var->VpiName(), var);
        if (var->UhdmType() == uhdmenum_var) {
          enum_var* evar = (enum_var*)var;
          enum_typespec* etps = (enum_typespec*)evar->Typespec();
          for (auto c : *etps->Enum_consts()) {
            varMap.emplace(c->VpiName(), c);
          }
        }
      }
    }

    if (defn->Named_events()) {
      for (named_event* var : *defn->Named_events()) {
        varMap.emplace(var->VpiName(), var);
      }
    }

    // Collect instance parameters, defparams
    if (defn->Parameters()) {
      for (any* param : *defn->Parameters()) {
        paramMap.emplace(param->VpiName(), param);
      }
    }

    // Collect func and task declaration
    if (defn->Task_funcs()) {
      for (task_func* tf : *defn->Task_funcs()) {
        if (funcMap.find(tf->VpiName()) == funcMap.end()) {
          // Bind to overriden function in sub-class
          funcMap.emplace(tf->VpiName(), tf);
        }
      }
    }

    const class_defn* base_defn = nullptr;
    if (const extends* ext = defn->Extends()) {
      if (const class_typespec* ctps = ext->Class_typespec()) {
        base_defn = ctps->Class_defn();
      }
    }
    defn = base_defn;
  }

  // Push class defn context on the stack
  // Class context is going to be pushed in case of:
  //   - imbricated classes
  //   - inheriting classes (Through the extends relation)
  instStack_.emplace_back(clone, varMap, paramMap, funcMap, modMap);
  if (muteErrors_ == false) {
    elabClass_defn(clone);
  }

  bindScheduledTaskFunc();

  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
  }

  return clone;
}

any* Elaborator::cloneClass_var(const class_var* source, any* parent) {
  any* const clone = basetype_t::cloneClass_var(source, parent);
  if (!inHierarchy_)
    return clone;  // Only do class var propagation while in elaboration

  class_var* cv = (class_var*)clone;
  if (typespec* ctps = cv->Typespec()) {
    ctps = (typespec*)cloneAny(ctps, cv);
    cv->Typespec(ctps);

    if (class_typespec* cctps = any_cast<class_typespec>(ctps)) {
      if (VectorOfparam_assign* params = cctps->Param_assigns()) {
        for (param_assign* pass : *params) {
          propagateParamAssign(pass, cctps->Class_defn());
        }
      }
    }
  }
  return clone;
}

any* Elaborator::cloneConstant(const constant* source, any* parent) {
  if (uniquifyTypespec() || (source->VpiSize() == -1)) {
    constant* const clone = m_serializer->MakeConstant();
    const uint32_t id = clone->UhdmId();
    *clone = *source;
    clone->UhdmId(id);
    clone->VpiParent(parent);
    if (auto obj = source->Typespec())
      clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));
    return clone;
  } else {
    return const_cast<constant*>(source);
  }
}

any* Elaborator::cloneCont_assign(const cont_assign* source, any* parent) {
  cont_assign* const clone = m_serializer->MakeCont_assign();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);
  clone->VpiParent(parent);
  if (auto obj = source->Delay())
    clone->Delay(static_cast<expr*>(cloneAny(obj, clone)));
  expr* lhs = nullptr;
  if (auto obj = source->Lhs()) {
    lhs = static_cast<expr*>(cloneAny(obj, clone));
    clone->Lhs(lhs);
  }
  if (auto obj = source->Rhs()) {
    expr* rhs = static_cast<expr*>(cloneAny(obj, clone));
    clone->Rhs(rhs);
    if (ref_obj* ref = any_cast<ref_obj*>(lhs)) {
      if (struct_var* stv = ref->Actual_group<struct_var>()) {
        ExprEval eval(muteErrors_);
        if (expr* res = eval.flattenPatternAssignments(*m_serializer,
                                                       stv->Typespec(), rhs)) {
          if (res->UhdmType() == uhdmoperation) {
            ((operation*)rhs)->Operands(((operation*)res)->Operands());
          }
        }
      }
    }
  }
  if (auto vec = source->Cont_assign_bits()) {
    auto clone_vec = m_serializer->MakeCont_assign_bitVec();
    clone_vec->reserve(vec->size());
    clone->Cont_assign_bits(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<cont_assign_bit*>(cloneAny(obj, clone)));
    }
  }

  return clone;
}

any* Elaborator::cloneDesign(const design* source, any* parent) {
  design* const clone = const_cast<design*>(source);
  clone->VpiElaborated(true);

  if (auto vec = source->Include_file_infos()) {
    auto clone_vec = m_serializer->MakeInclude_file_infoVec();
    clone_vec->reserve(vec->size());
    clone->Include_file_infos(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<include_file_info*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->AllPackages()) {
    auto clone_vec = m_serializer->MakePackageVec();
    clone_vec->reserve(vec->size());
    clone->AllPackages(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<package*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->TopPackages()) {
    uhdmAllIterator_ = false;
    auto clone_vec = m_serializer->MakePackageVec();
    clone_vec->reserve(vec->size());
    clone->TopPackages(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<package*>(cloneAny(obj, clone)));
    }
    uhdmAllIterator_ = true;
  }
  if (auto vec = source->AllClasses()) {
    auto clone_vec = m_serializer->MakeClass_defnVec();
    clone_vec->reserve(vec->size());
    clone->AllClasses(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<class_defn*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->AllInterfaces()) {
    auto clone_vec = m_serializer->MakeInterface_instVec();
    clone_vec->reserve(vec->size());
    clone->AllInterfaces(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<interface_inst*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->AllUdps()) {
    auto clone_vec = m_serializer->MakeUdp_defnVec();
    clone_vec->reserve(vec->size());
    clone->AllUdps(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<udp_defn*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->AllPrograms()) {
    auto clone_vec = m_serializer->MakeProgramVec();
    clone_vec->reserve(vec->size());
    clone->AllPrograms(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<program*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->AllModules()) {
    auto clone_vec = m_serializer->MakeModule_instVec();
    clone_vec->reserve(vec->size());
    clone->AllModules(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<module_inst*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Typespecs()) {
    auto clone_vec = m_serializer->MakeTypespecVec();
    clone_vec->reserve(vec->size());
    clone->Typespecs(clone_vec);
    for (auto obj : *vec) {
      if (uniquifyTypespec()) {
        clone_vec->emplace_back(static_cast<typespec*>(cloneAny(obj, clone)));
      } else {
        clone_vec->emplace_back(obj);
      }
    }
  }
  if (auto vec = source->Let_decls()) {
    auto clone_vec = m_serializer->MakeLet_declVec();
    clone_vec->reserve(vec->size());
    clone->Let_decls(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<let_decl*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Task_funcs()) {
    auto clone_vec = m_serializer->MakeTask_funcVec();
    clone_vec->reserve(vec->size());
    clone->Task_funcs(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<task_func*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Parameters()) {
    auto clone_vec = m_serializer->MakeAnyVec();
    clone_vec->reserve(vec->size());
    clone->Parameters(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<any*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Param_assigns()) {
    auto clone_vec = m_serializer->MakeParam_assignVec();
    clone_vec->reserve(vec->size());
    clone->Param_assigns(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<param_assign*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->TopModules()) {
    uhdmAllIterator_ = false;
    auto clone_vec = m_serializer->MakeModule_instVec();
    clone_vec->reserve(vec->size());
    clone->TopModules(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<module_inst*>(cloneAny(obj, clone)));
    }
    uhdmAllIterator_ = true;
  }

  return clone;
}

any* Elaborator::cloneFor_stmt(const for_stmt* source, any* parent) {
  for_stmt* const clone = m_serializer->MakeFor_stmt();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);

  ComponentMap varMap;
  if (clone->Array_vars()) {
    for (variables* var : *clone->Array_vars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (clone->Variables()) {
    for (variables* var : *clone->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (clone->VpiForInitStmts()) {
    for (any* stmt : *clone->VpiForInitStmts()) {
      if (stmt->UhdmType() == uhdmassign_stmt) {
        assign_stmt* astmt = (assign_stmt*)stmt;
        const any* lhs = astmt->Lhs();
        if (lhs->UhdmType() != uhdmref_var) {
          varMap.emplace(lhs->VpiName(), lhs);
        }
      }
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(clone, varMap, paramMap, funcMap, modMap);

  source->DeepCopy(clone, parent, this);

  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
  }

  return clone;
}

any* Elaborator::cloneForeach_stmt(const foreach_stmt* source, any* parent) {
  foreach_stmt* const clone = m_serializer->MakeForeach_stmt();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);

  ComponentMap varMap;
  if (clone->Array_vars()) {
    for (variables* var : *clone->Array_vars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (clone->Variables()) {
    for (variables* var : *clone->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (clone->VpiLoopVars()) {
    for (any* var : *clone->VpiLoopVars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(clone, varMap, paramMap, funcMap, modMap);

  source->DeepCopy(clone, parent, this);

  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
  }

  return clone;
}

any* Elaborator::cloneFork_stmt(const fork_stmt* source, any* parent) {
  fork_stmt* const clone = m_serializer->MakeFork_stmt();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);

  ComponentMap varMap;
  if (clone->Array_vars()) {
    for (variables* var : *clone->Array_vars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (clone->Variables()) {
    for (variables* var : *clone->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(clone, varMap, paramMap, funcMap, modMap);

  source->DeepCopy(clone, parent, this);

  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
  }

  return clone;
}

any* Elaborator::cloneFunc_call(const func_call* source, any* parent) {
  bool is_function = isFunctionCall(source->VpiName(), nullptr);
  tf_call* tf_call_clone = nullptr;
  if (is_function) {
    func_call *const clone = m_serializer->MakeFunc_call();
    tf_call_clone = clone;
    const uint32_t id = clone->UhdmId();
    *clone = *source;
    clone->UhdmId(id);
    clone->VpiParent(parent);
    scheduleTaskFuncBinding(clone, nullptr);
    if (auto obj = source->Scope())
      clone->Scope(static_cast<scope*>(cloneAny(obj, clone)));
    if (auto vec = source->Tf_call_args()) {
      auto clone_vec = m_serializer->MakeAnyVec();
      clone_vec->reserve(vec->size());
      clone->Tf_call_args(clone_vec);
      for (auto obj : *vec) {
        clone_vec->emplace_back(cloneAny(obj, clone));
      }
    }
    if (auto obj = source->Typespec())
      clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));
  } else {
    task_call *const clone = m_serializer->MakeTask_call();
    tf_call_clone = clone;
    //*clone = *source;
    clone->VpiParent(parent);
    clone->VpiName(source->VpiName());
    clone->VpiFile(source->VpiFile());
    clone->VpiLineNo(source->VpiLineNo());
    clone->VpiColumnNo(source->VpiColumnNo());
    clone->VpiEndLineNo(source->VpiEndLineNo());
    clone->VpiEndColumnNo(source->VpiEndColumnNo());
    clone->Tf_call_args(source->Tf_call_args());
    scheduleTaskFuncBinding(clone, nullptr);
    if (auto obj = source->Scope())
      clone->Scope(static_cast<scope*>(cloneAny(obj, clone)));
    if (auto vec = source->Tf_call_args()) {
      auto clone_vec = m_serializer->MakeAnyVec();
      clone_vec->reserve(vec->size());
      clone->Tf_call_args(clone_vec);
      for (auto obj : *vec) {
        clone_vec->emplace_back(cloneAny(obj, clone));
      }
    }
    if (auto obj = source->Typespec())
      clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));
  }

  return tf_call_clone;
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
          if (const class_typespec* ctps = ext->Class_typespec()) {
            base_defn = ctps->Class_defn();
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

any* Elaborator::cloneFunction(const function* source, any* parent) {
  function* const clone = m_serializer->MakeFunction();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);
  clone->VpiParent(parent);
  if (auto obj = source->Left_range())
    clone->Left_range(static_cast<expr*>(cloneAny(obj, clone)));
  if (auto obj = source->Right_range())
    clone->Right_range(static_cast<expr*>(cloneAny(obj, clone)));
  if (auto obj = source->Return()) clone->Return((variables*)obj);
  if (auto obj = source->Instance()) clone->Instance((instance*)obj);
  if (instance* inst = any_cast<instance*>(parent)) clone->Instance(inst);
  if (auto obj = source->Class_defn())
    clone->Class_defn(static_cast<clocking_block*>(cloneAny(obj, clone)));
  if (auto vec = source->Io_decls()) {
    auto clone_vec = m_serializer->MakeIo_declVec();
    clone_vec->reserve(vec->size());
    clone->Io_decls(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<io_decl*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Variables()) {
    auto clone_vec = m_serializer->MakeVariablesVec();
    clone_vec->reserve(vec->size());
    clone->Variables(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<variables*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Parameters()) {
    auto clone_vec = m_serializer->MakeAnyVec();
    clone_vec->reserve(vec->size());
    clone->Parameters(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(cloneAny(obj, clone));
    }
  }
  if (auto vec = source->Scopes()) {
    auto clone_vec = m_serializer->MakeScopeVec();
    clone_vec->reserve(vec->size());
    clone->Scopes(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<scope*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Typespecs()) {
    auto clone_vec = m_serializer->MakeTypespecVec();
    clone_vec->reserve(vec->size());
    clone->Typespecs(clone_vec);
    for (auto obj : *vec) {
      if (uniquifyTypespec()) {
        clone_vec->emplace_back(static_cast<typespec*>(cloneAny(obj, clone)));
      } else {
        clone_vec->emplace_back(obj);
      }
    }
  }
  enterTask_func(clone);
  if (auto vec = source->Concurrent_assertions()) {
    auto clone_vec = m_serializer->MakeConcurrent_assertionsVec();
    clone_vec->reserve(vec->size());
    clone->Concurrent_assertions(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<concurrent_assertions*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Property_decls()) {
    auto clone_vec = m_serializer->MakeProperty_declVec();
    clone_vec->reserve(vec->size());
    clone->Property_decls(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<property_decl*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Sequence_decls()) {
    auto clone_vec = m_serializer->MakeSequence_declVec();
    clone_vec->reserve(vec->size());
    clone->Sequence_decls(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<sequence_decl*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Named_events()) {
    auto clone_vec = m_serializer->MakeNamed_eventVec();
    clone_vec->reserve(vec->size());
    clone->Named_events(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<named_event*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Named_event_arrays()) {
    auto clone_vec = m_serializer->MakeNamed_event_arrayVec();
    clone_vec->reserve(vec->size());
    clone->Named_event_arrays(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<named_event_array*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Virtual_interface_vars()) {
    auto clone_vec = m_serializer->MakeVirtual_interface_varVec();
    clone_vec->reserve(vec->size());
    clone->Virtual_interface_vars(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<virtual_interface_var*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Logic_vars()) {
    auto clone_vec = m_serializer->MakeLogic_varVec();
    clone_vec->reserve(vec->size());
    clone->Logic_vars(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<logic_var*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Array_vars()) {
    auto clone_vec = m_serializer->MakeArray_varVec();
    clone_vec->reserve(vec->size());
    clone->Array_vars(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<array_var*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Array_var_mems()) {
    auto clone_vec = m_serializer->MakeArray_varVec();
    clone_vec->reserve(vec->size());
    clone->Array_var_mems(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<array_var*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Param_assigns()) {
    auto clone_vec = m_serializer->MakeParam_assignVec();
    clone_vec->reserve(vec->size());
    clone->Param_assigns(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<param_assign*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Let_decls()) {
    auto clone_vec = m_serializer->MakeLet_declVec();
    clone_vec->reserve(vec->size());
    clone->Let_decls(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<let_decl*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Attributes()) {
    auto clone_vec = m_serializer->MakeAttributeVec();
    clone_vec->reserve(vec->size());
    clone->Attributes(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<attribute*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Instance_items()) {
    auto clone_vec = m_serializer->MakeAnyVec();
    clone_vec->reserve(vec->size());
    clone->Instance_items(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(cloneAny(obj, clone));
    }
  }
  if (auto obj = source->Stmt()) clone->Stmt(cloneAny(obj, clone));
  leaveTask_func(clone);
  return clone;
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

any* Elaborator::cloneGen_scope(const gen_scope* source, any* parent) {
  gen_scope* const clone = m_serializer->MakeGen_scope();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);
  enterGen_scope(clone);
  source->DeepCopy(clone, parent, this);
  leaveGen_scope(clone);
  return clone;
}

any* Elaborator::cloneGen_scope_array(const gen_scope_array* source,
                                      any* parent) {
  gen_scope_array* const clone = m_serializer->MakeGen_scope_array();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);
  clone->VpiParent(parent);
  if (auto obj = source->Gen_var())
    clone->Gen_var(static_cast<gen_var*>(cloneAny(obj, clone)));
  if (auto vec = source->Gen_scopes()) {
    auto clone_vec = m_serializer->MakeGen_scopeVec();
    clone_vec->reserve(vec->size());
    clone->Gen_scopes(clone_vec);
    for (auto obj : *vec) {
      enterGen_scope(obj);
      clone_vec->emplace_back(static_cast<gen_scope*>(cloneAny(obj, clone)));
      leaveGen_scope(obj);
    }
  }
  if (auto obj = source->VpiInstance())
    clone->VpiInstance(cloneAny(obj, clone));

  return clone;
}

any* Elaborator::cloneHier_path(const hier_path* source, any* parent) {
  hier_path* const clone = m_serializer->MakeHier_path();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);
  clone->VpiParent(parent);

  if (auto vec = source->Path_elems()) {
    auto clone_vec = m_serializer->MakeAnyVec();
    clone_vec->reserve(vec->size());
    clone->Path_elems(clone_vec);

    any* previous = nullptr;
    for (auto obj : *vec) {
      any* current = cloneAny(obj, clone);
      clone_vec->emplace_back(current);
      bool found = false;
      if (ref_obj* ref = any_cast<ref_obj*>(current)) {
        if (current->VpiName() == "this") {
          const any* tmp = current;
          while (tmp) {
            if (tmp->UhdmType() == uhdmclass_defn) {
              ref->Actual_group((any*)tmp);
              found = true;
              break;
            }
            tmp = tmp->VpiParent();
          }
        } else if (current->VpiName() == "super") {
          const any* tmp = current;
          while (tmp) {
            if (tmp->UhdmType() == uhdmclass_defn) {
              class_defn* def = (class_defn*)tmp;
              if (const extends* ext = def->Extends()) {
                if (const class_typespec* ctps = ext->Class_typespec()) {
                  ref->Actual_group((any*)ctps->Class_defn());
                  found = true;
                  break;
                }
              }
              break;
            }
            tmp = tmp->VpiParent();
          }
        }
      }
      if (previous) {
        std::string_view name = obj->VpiName();
        if (name.empty() || name.find('[') == 0) {
          if (ref_obj* ro = any_cast<ref_obj*>(obj)) {
            if (const any* actual = ro->Actual_group()) {
              name = actual->VpiName();
            }
            //  a[i][j]
            if (previous->UhdmType() == uhdmbit_select) {
              bit_select* prev = (bit_select*)previous;
              ro->Actual_group((any*)prev->Actual_group());
              found = true;
            }
          }
        }
        std::string nameIndexed(name);
        if (obj->UhdmType() == uhdmbit_select) {
          bit_select* bs = static_cast<bit_select*>(obj);
          const expr* index = bs->VpiIndex();
          std::string_view indexName = index->VpiDecompile();
          if (!indexName.empty()) {
            nameIndexed.append("[").append(indexName).append("]");
          }
        }
        if (ref_obj* pro = any_cast<ref_obj*>(previous)) {
          const any* actual = pro->Actual_group();
          if ((actual == nullptr) && (previous->VpiName() == "$root")) {
            actual = currentDesign();
          }
          if (actual) {
            UHDM_OBJECT_TYPE actual_type = actual->UhdmType();
            switch (actual_type) {
              case uhdmdesign: {
                design* scope = (design*)actual;
                if (scope->TopModules()) {
                  for (auto m : *scope->TopModules()) {
                    const std::string_view modName = m->VpiName();
                    if (modName == name || modName == nameIndexed ||
                        modName == std::string("work@").append(name)) {
                      found = true;
                      previous = m;
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(m);
                      }
                      break;
                    }
                  }
                }
                break;
              }
              case uhdmgen_scope: {
                gen_scope* scope = (gen_scope*)actual;
                if (obj->UhdmType() == uhdmmethod_func_call) {
                  method_func_call* call = (method_func_call*)current;
                  if (scope->Task_funcs()) {
                    for (auto tf : *scope->Task_funcs()) {
                      if (tf->VpiName() == name) {
                        call->Function(any_cast<function*>(tf));
                        previous = (any*)call->Function();
                        found = true;
                        break;
                      }
                    }
                  }
                } else if (obj->UhdmType() == uhdmmethod_task_call) {
                  method_task_call* call = (method_task_call*)current;
                  if (scope->Task_funcs()) {
                    for (auto tf : *scope->Task_funcs()) {
                      if (tf->VpiName() == name) {
                        call->Task(any_cast<task*>(tf));
                        found = true;
                        previous = (any*)call->Task();
                        break;
                      }
                    }
                  }
                } else {
                  if (!found && scope->Modules()) {
                    for (auto m : *scope->Modules()) {
                      if (m->VpiName() == name || m->VpiName() == nameIndexed) {
                        found = true;
                        previous = m;
                        if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                          cro->Actual_group(m);
                        }
                        break;
                      }
                    }
                  }
                  if (!found && scope->Nets()) {
                    for (auto m : *scope->Nets()) {
                      if (m->VpiName() == name || m->VpiName() == nameIndexed) {
                        found = true;
                        previous = m;
                        if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                          cro->Actual_group(m);
                        }
                        break;
                      }
                    }
                  }
                  if (!found && scope->Array_nets()) {
                    for (auto m : *scope->Array_nets()) {
                      if (m->VpiName() == name || m->VpiName() == nameIndexed) {
                        found = true;
                        previous = m;
                        if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                          cro->Actual_group(m);
                        }
                        break;
                      }
                    }
                  }
                  if (!found && scope->Variables()) {
                    for (auto m : *scope->Variables()) {
                      if (m->VpiName() == name || m->VpiName() == nameIndexed) {
                        found = true;
                        previous = m;
                        if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                          cro->Actual_group(m);
                        }
                        break;
                      }
                    }
                  }
                  if (!found && scope->Gen_scope_arrays()) {
                    for (auto gsa : *scope->Gen_scope_arrays()) {
                      if (gsa->VpiName() == name ||
                          gsa->VpiName() == nameIndexed) {
                        if (!gsa->Gen_scopes()->empty()) {
                          auto gs = gsa->Gen_scopes()->front();
                          if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                            cro->Actual_group(gs);
                          }
                          previous = gs;
                          found = true;
                        }
                      }
                    }
                  }
                }
                break;
              }
              case uhdmmodport: {
                modport* mp = (modport*)actual;
                if (mp->Io_decls()) {
                  for (io_decl* decl : *mp->Io_decls()) {
                    if (decl->VpiName() == name) {
                      found = true;
                      previous = decl;
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(decl);
                      }
                    }
                  }
                }
                break;
              }
              case uhdmnamed_event: {
                if (name == "triggered") {
                  // Builtin
                  found = true;
                }
                break;
              }
              case uhdmarray_net: {
                array_net* anet = (array_net*)actual;
                VectorOfnet* vars = anet->Nets();
                if (vars && vars->size()) {
                  actual = vars->at(0);
                  actual_type = actual->UhdmType();
                }
                if (name == "size" || name == "exists" || name == "find" ||
                    name == "max" || name == "min") {
                  func_call* call = m_serializer->MakeFunc_call();
                  call->VpiName(name);
                  call->VpiParent(clone);
                  if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                    cro->Actual_group(call);
                  }
                  // Builtin method
                  found = true;
                  previous = (any*)call;
                } else if (name == "") {
                  // One of the Index(es)
                  found = true;
                }
                break;
              }
              case uhdmarray_var: {
                array_var* avar = (array_var*)actual;
                VectorOfvariables* vars = avar->Variables();
                if (vars && vars->size()) {
                  actual = vars->at(0);
                  actual_type = actual->UhdmType();
                }
                if (name == "size" || name == "exists" || name == "find" ||
                    name == "max" || name == "min") {
                  func_call* call = m_serializer->MakeFunc_call();
                  call->VpiName(name);
                  call->VpiParent(clone);
                  if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                    cro->Actual_group(call);
                  }
                  // Builtin method
                  found = true;
                  previous = (any*)call;
                } else if (name == "") {
                  // One of the Index(es)
                  found = true;
                }
                break;
              }
              case uhdmpacked_array_var: {
                packed_array_var* avar = (packed_array_var*)actual;
                VectorOfany* vars = avar->Elements();
                if (vars && vars->size()) {
                  actual = vars->at(0);
                  actual_type = actual->UhdmType();
                }
                if (name == "size" || name == "exists" || name == "exists" ||
                    name == "max" || name == "min") {
                  func_call* call = m_serializer->MakeFunc_call();
                  call->VpiName(name);
                  call->VpiParent(clone);
                  if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                    cro->Actual_group(call);
                  }
                  // Builtin method
                  found = true;
                  previous = (any*)call;
                }
                if (found == false) {
                  if (const typespec* tps = avar->Typespec()) {
                    UHDM_OBJECT_TYPE ttype = tps->UhdmType();
                    if (ttype == uhdmpacked_array_typespec) {
                      packed_array_typespec* ptps = (packed_array_typespec*)tps;
                      tps = (typespec*)ptps->Elem_typespec();
                      ttype = tps->UhdmType();
                    } else if (ttype == uhdmarray_typespec) {
                      array_typespec* ptps = (array_typespec*)tps;
                      tps = (typespec*)ptps->Elem_typespec();
                      ttype = tps->UhdmType();
                    }
                    if (ttype == uhdmstring_typespec) {
                      found = true;
                    } else if (ttype == uhdmclass_typespec) {
                      class_typespec* ctps = (class_typespec*)tps;
                      any* tmp = bindClassTypespec(ctps, current, name, found);
                      if (found) {
                        previous = tmp;
                      }
                    } else if (ttype == uhdmstruct_typespec) {
                      struct_typespec* stpt = (struct_typespec*)tps;
                      for (typespec_member* member : *stpt->Members()) {
                        if (member->VpiName() == name) {
                          if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                            cro->Actual_group(member);
                          }
                          previous = member;
                          found = true;
                          break;
                        }
                      }
                      if (name == "name") {
                        // Builtin introspection
                        found = true;
                      }
                    } else if (ttype == uhdmenum_typespec) {
                      if (name == "name") {
                        // Builtin introspection
                        found = true;
                      }
                    } else if (ttype == uhdmunion_typespec) {
                      union_typespec* stpt = (union_typespec*)tps;
                      for (typespec_member* member : *stpt->Members()) {
                        if (member->VpiName() == name) {
                          if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                            cro->Actual_group(member);
                          }
                          previous = member;
                          found = true;
                          break;
                        }
                      }
                      if (name == "name") {
                        // Builtin introspection
                        found = true;
                      }
                    }
                  }
                }
                break;
              }
              case uhdmpacked_array_net: {
                packed_array_net* avar = (packed_array_net*)actual;
                VectorOfany* vars = avar->Elements();
                if (vars && vars->size()) {
                  actual = vars->at(0);
                  actual_type = actual->UhdmType();
                }
                if (name == "size" || name == "exists" || name == "exists" ||
                    name == "max" || name == "min") {
                  func_call* call = m_serializer->MakeFunc_call();
                  call->VpiName(name);
                  call->VpiParent(clone);
                  if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                    cro->Actual_group(call);
                  }
                  // Builtin method
                  found = true;
                  previous = (any*)call;
                }
                break;
              }
              case uhdmnamed_begin: {
                named_begin* begin = (named_begin*)actual;
                if (!found && begin->Variables()) {
                  for (auto m : *begin->Variables()) {
                    if (m->VpiName() == name || m->VpiName() == nameIndexed) {
                      found = true;
                      previous = m;
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(m);
                      }
                      break;
                    }
                  }
                }
                if (!found && begin->Array_vars()) {
                  for (auto m : *begin->Array_vars()) {
                    if (m->VpiName() == name || m->VpiName() == nameIndexed) {
                      found = true;
                      previous = m;
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(m);
                      }
                      break;
                    }
                  }
                }
                break;
              }
              case uhdmnamed_fork: {
                named_fork* begin = (named_fork*)actual;
                if (!found && begin->Variables()) {
                  for (auto m : *begin->Variables()) {
                    if (m->VpiName() == name || m->VpiName() == nameIndexed) {
                      found = true;
                      previous = m;
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(m);
                      }
                      break;
                    }
                  }
                }
                if (!found && begin->Array_vars()) {
                  for (auto m : *begin->Array_vars()) {
                    if (m->VpiName() == name || m->VpiName() == nameIndexed) {
                      found = true;
                      previous = m;
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(m);
                      }
                      break;
                    }
                  }
                }
                break;
              }
              default:
                break;
            }

            switch (actual_type) {
              case uhdmclocking_block: {
                clocking_block* block = (clocking_block*)actual;
                if (block->Clocking_io_decls()) {
                  for (clocking_io_decl* decl : *block->Clocking_io_decls()) {
                    if (decl->VpiName() == name) {
                      found = true;
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(decl);
                      }
                    }
                  }
                }
                break;
              }
              case uhdmmodule_inst: {
                module_inst* mod = (module_inst*)actual;
                if (!found && mod->Variables()) {
                  for (variables* var : *mod->Variables()) {
                    if (var->VpiName() == name) {
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(var);
                      }
                      previous = var;
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && mod->Nets()) {
                  for (nets* n : *mod->Nets()) {
                    if (n->VpiName() == name) {
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(n);
                      }
                      previous = n;
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && mod->Modules()) {
                  for (auto m : *mod->Modules()) {
                    if (m->VpiName() == name || m->VpiName() == nameIndexed) {
                      found = true;
                      previous = m;
                      break;
                    }
                  }
                }
                if (!found && mod->Gen_scope_arrays()) {
                  for (auto gsa : *mod->Gen_scope_arrays()) {
                    if (gsa->VpiName() == name ||
                        gsa->VpiName() == nameIndexed) {
                      if (!gsa->Gen_scopes()->empty()) {
                        auto gs = gsa->Gen_scopes()->front();
                        if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                          cro->Actual_group(gs);
                        }
                        previous = gs;
                        found = true;
                      }
                    }
                  }
                }
                break;
              }
              case uhdmclass_var: {
                if (const typespec* tps = ((class_var*)actual)->Typespec()) {
                  UHDM_OBJECT_TYPE ttype = tps->UhdmType();
                  if (ttype == uhdmclass_typespec) {
                    class_typespec* ctps = (class_typespec*)tps;
                    any* tmp = bindClassTypespec(ctps, current, name, found);
                    if (found) {
                      previous = tmp;
                    }
                  } else if (ttype == uhdmstruct_typespec) {
                    struct_typespec* stpt = (struct_typespec*)tps;
                    for (typespec_member* member : *stpt->Members()) {
                      if (member->VpiName() == name) {
                        if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                          cro->Actual_group(member);
                        }
                        previous = member;
                        found = true;
                        break;
                      }
                    }
                  }
                }
                if (current->UhdmType() == uhdmmethod_func_call) {
                  found = true;
                }
                break;
              }
              case uhdmstruct_net:
              case uhdmstruct_var: {
                struct_typespec* stpt = nullptr;
                if (actual->UhdmType() == uhdmstruct_net) {
                  stpt = (struct_typespec*)((struct_net*)actual)->Typespec();
                } else if (actual->UhdmType() == uhdmstruct_var) {
                  stpt = (struct_typespec*)((struct_var*)actual)->Typespec();
                }
                if (stpt && stpt->Members()) {
                  for (typespec_member* member : *stpt->Members()) {
                    if (member->VpiName() == name) {
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(member);
                      }
                      previous = member;
                      found = true;
                      break;
                    }
                  }
                }
                break;
              }
              case uhdmunion_var: {
                union_typespec* stpt =
                    (union_typespec*)((union_var*)actual)->Typespec();
                if (stpt) {
                  for (typespec_member* member : *stpt->Members()) {
                    if (member->VpiName() == name) {
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(member);
                      }
                      previous = member;
                      found = true;
                      break;
                    }
                  }
                }
                break;
              }
              case uhdminterface_inst: {
                interface_inst* interf = (interface_inst*)actual;
                if (!found && interf->Variables()) {
                  for (variables* var : *interf->Variables()) {
                    if (var->VpiName() == name) {
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(var);
                      }
                      previous = var;
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && interf->Task_funcs()) {
                  for (auto tf : *interf->Task_funcs()) {
                    if (tf->VpiName() == name) {
                      previous = any_cast<function*>(tf);
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && interf->Modports()) {
                  for (modport* mport : *interf->Modports()) {
                    if (mport->VpiName() == name) {
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(mport);
                      }
                      previous = mport;
                      found = true;
                      break;
                    }
                    if (mport->Io_decls()) {
                      for (io_decl* decl : *mport->Io_decls()) {
                        if (decl->VpiName() == name) {
                          any* actual_decl = decl;
                          if (any* exp = decl->Expr()) {
                            actual_decl = exp;
                          }
                          if (actual_decl->UhdmType() == uhdmref_obj) {
                            ref_obj* ref = (ref_obj*)actual_decl;
                            if (const any* act = ref->Actual_group()) {
                              actual_decl = (any*)act;
                            }
                          }
                          if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                            cro->Actual_group(actual_decl);
                          }
                          previous = actual_decl;
                          found = true;
                          break;
                        }
                      }
                    }
                    if (found) break;
                  }
                }
                if (!found && interf->Nets()) {
                  for (nets* n : *interf->Nets()) {
                    if (n->VpiName() == name) {
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(n);
                      }
                      previous = n;
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && interf->Ports()) {
                  for (port* p : *interf->Ports()) {
                    if (p->VpiName() == name) {
                      if (any* ref = p->Low_conn()) {
                        if (ref_obj* nref = any_cast<ref_obj*>(ref)) {
                          any* n = nref->Actual_group();
                          if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                            cro->Actual_group(n);
                          }
                          previous = n;
                          found = true;
                          break;
                        }
                      }
                    }
                  }
                }
                break;
              }
              case uhdmarray_var: {
                if (current->UhdmType() == uhdmmethod_func_call)
                  found = true;
                else if (current->UhdmType() == uhdmbit_select)
                  found = true;
                break;
              }
              case uhdmstring_var: {
                if (current->UhdmType() == uhdmmethod_func_call)
                  found = true;
                else if (current->UhdmType() == uhdmbit_select)
                  found = true;
                break;
              }
              case uhdmclass_typespec: {
                class_typespec* ctps = (class_typespec*)actual;
                any* tmp = bindClassTypespec(ctps, current, name, found);
                if (found) {
                  previous = tmp;
                }
                break;
              }
              case uhdmio_decl: {
                io_decl* decl = (io_decl*)actual;
                if (const typespec* tps = decl->Typespec()) {
                  UHDM_OBJECT_TYPE ttype = tps->UhdmType();
                  if (ttype == uhdmstring_typespec) {
                    found = true;
                  } else if (ttype == uhdmclass_typespec) {
                    class_typespec* ctps = (class_typespec*)tps;
                    any* tmp = bindClassTypespec(ctps, current, name, found);
                    if (found) {
                      previous = tmp;
                    }
                  } else if (ttype == uhdmstruct_typespec) {
                    struct_typespec* stpt = (struct_typespec*)tps;
                    for (typespec_member* member : *stpt->Members()) {
                      if (member->VpiName() == name) {
                        if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                          cro->Actual_group(member);
                        }
                        previous = member;
                        found = true;
                        break;
                      }
                    }
                    if (name == "name") {
                      // Builtin introspection
                      found = true;
                    }
                  } else if (ttype == uhdmenum_typespec) {
                    if (name == "name") {
                      // Builtin introspection
                      found = true;
                    }
                  } else if (ttype == uhdmunion_typespec) {
                    union_typespec* stpt = (union_typespec*)tps;
                    for (typespec_member* member : *stpt->Members()) {
                      if (member->VpiName() == name) {
                        if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                          cro->Actual_group(member);
                        }
                        previous = member;
                        found = true;
                        break;
                      }
                    }
                    if (name == "name") {
                      // Builtin introspection
                      found = true;
                    }
                  }
                }
                if (decl->Ranges()) {
                  if (current->UhdmType() == uhdmmethod_func_call)
                    found = true;
                  else if (current->UhdmType() == uhdmbit_select)
                    found = true;
                }
                // TODO: class method support
                if (current->UhdmType() == uhdmmethod_func_call) found = true;
                break;
              }
              case uhdmparameter: {
                parameter* param = (parameter*)actual;
                if (const typespec* tps = param->Typespec()) {
                  UHDM_OBJECT_TYPE ttype = tps->UhdmType();
                  if (ttype == uhdmpacked_array_typespec) {
                    packed_array_typespec* ptps = (packed_array_typespec*)tps;
                    tps = (typespec*)ptps->Elem_typespec();
                    ttype = tps->UhdmType();
                  } else if (ttype == uhdmarray_typespec) {
                    array_typespec* ptps = (array_typespec*)tps;
                    tps = (typespec*)ptps->Elem_typespec();
                    ttype = tps->UhdmType();
                  }
                  if (ttype == uhdmstring_typespec) {
                    found = true;
                  } else if (ttype == uhdmclass_typespec) {
                    class_typespec* ctps = (class_typespec*)tps;
                    any* tmp = bindClassTypespec(ctps, current, name, found);
                    if (found) {
                      previous = tmp;
                    }
                  } else if (ttype == uhdmstruct_typespec) {
                    struct_typespec* stpt = (struct_typespec*)tps;
                    for (typespec_member* member : *stpt->Members()) {
                      if (member->VpiName() == name) {
                        if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                          cro->Actual_group(member);
                        }
                        previous = member;
                        found = true;
                        break;
                      }
                    }
                    if (name == "name") {
                      // Builtin introspection
                      found = true;
                    }
                  } else if (ttype == uhdmenum_typespec) {
                    if (name == "name") {
                      // Builtin introspection
                      found = true;
                    }
                  } else if (ttype == uhdmunion_typespec) {
                    union_typespec* stpt = (union_typespec*)tps;
                    for (typespec_member* member : *stpt->Members()) {
                      if (member->VpiName() == name) {
                        if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                          cro->Actual_group(member);
                        }
                        previous = member;
                        found = true;
                        break;
                      }
                    }
                    if (name == "name") {
                      // Builtin introspection
                      found = true;
                    }
                  }
                }
                if (param->Ranges()) {
                  if (current->UhdmType() == uhdmmethod_func_call)
                    found = true;
                  else if (current->UhdmType() == uhdmbit_select)
                    found = true;
                }
                break;
              }
              case uhdmoperation: {
                operation* op = (operation*)actual;
                if (op->VpiOpType() != vpiAssignmentPatternOp) {
                  break;
                }
                const typespec* tps = op->Typespec();
                if (!tps) {
                  break;
                }
                const struct_typespec* stps =
                    any_cast<const struct_typespec*>(tps);
                if (!stps) {
                  break;
                }
                std::vector<std::string_view> fieldNames;
                std::vector<const typespec*> fieldTypes;
                for (typespec_member* memb : *stps->Members()) {
                  fieldNames.emplace_back(memb->VpiName());
                  fieldTypes.emplace_back(memb->Typespec());
                }
                std::vector<any*> tmp(fieldNames.size());
                VectorOfany* orig = op->Operands();
                any* defaultOp = nullptr;
                any* res = nullptr;
                int32_t index = 0;
                for (auto oper : *orig) {
                  if (oper->UhdmType() == uhdmtagged_pattern) {
                    tagged_pattern* tp = (tagged_pattern*)oper;
                    const typespec* ttp = tp->Typespec();
                    const std::string_view tname = ttp->VpiName();
                    bool oper_found = false;
                    if (tname == "default") {
                      defaultOp = oper;
                      oper_found = true;
                    }
                    for (uint32_t i = 0; i < fieldNames.size(); i++) {
                      if (tname == fieldNames[i]) {
                        tmp[i] = oper;
                        oper_found = true;
                        res = tmp[i];
                        break;
                      }
                    }
                    if (oper_found == false) {
                      for (uint32_t i = 0; i < fieldTypes.size(); i++) {
                        if (ttp->UhdmType() == fieldTypes[i]->UhdmType()) {
                          tmp[i] = oper;
                          oper_found = true;
                          res = tmp[i];
                          break;
                        }
                      }
                    }
                  } else {
                    if (index < (int32_t)tmp.size()) {
                      tmp[index] = oper;
                      found = true;
                      res = tmp[index];
                    }
                  }
                  index++;
                }
                if (res == nullptr) {
                  if (defaultOp) {
                    res = defaultOp;
                  }
                }
                previous = res;
                break;
              }
              case uhdmref_var: {
                found = true;
                // TODO: class var support
                break;
              }
              default:
                // TODO: class method support
                if (current->UhdmType() == uhdmmethod_func_call) found = true;
                break;
            }
            if (!found) {
              if (!muteErrors() && !isInUhdmAllIterator()) {
                const std::string errMsg(source->VpiName());
                m_serializer->GetErrorHandler()(
                    ErrorType::UHDM_UNRESOLVED_HIER_PATH, errMsg, source,
                    nullptr);
              }
            }
          } else {
            if (!muteErrors() && !isInUhdmAllIterator()) {
              if (previous->UhdmType() == uhdmbit_select) {
                break;
              }
              const std::string errMsg(source->VpiName());
              m_serializer->GetErrorHandler()(
                  ErrorType::UHDM_UNRESOLVED_HIER_PATH, errMsg, source,
                  nullptr);
            }
          }
        } else if (previous->UhdmType() == uhdmtypespec_member) {
          typespec_member* member = (typespec_member*)previous;
          if (const typespec* tps = member->Typespec()) {
            UHDM_OBJECT_TYPE ttype = tps->UhdmType();
            if (ttype == uhdmpacked_array_typespec) {
              packed_array_typespec* ptps = (packed_array_typespec*)tps;
              tps = (typespec*)ptps->Elem_typespec();
              ttype = tps->UhdmType();
            } else if (ttype == uhdmarray_typespec) {
              array_typespec* ptps = (array_typespec*)tps;
              tps = (typespec*)ptps->Elem_typespec();
              ttype = tps->UhdmType();
            }
            if (ttype == uhdmstruct_typespec) {
              struct_typespec* stpt = (struct_typespec*)tps;
              for (typespec_member* tsmember : *stpt->Members()) {
                if (tsmember->VpiName() == name) {
                  if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                    cro->Actual_group(tsmember);
                    previous = tsmember;
                    found = true;
                    break;
                  }
                }
              }
            } else if (ttype == uhdmunion_typespec) {
              union_typespec* stpt = (union_typespec*)tps;
              for (typespec_member* tsmember : *stpt->Members()) {
                if (tsmember->VpiName() == name) {
                  if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                    cro->Actual_group(tsmember);
                    previous = tsmember;
                    found = true;
                    break;
                  }
                }
              }
            } else if (ttype == uhdmstring_typespec) {
              if (name == "len") {
                found = true;
              }
            }
          }
        } else if (previous->UhdmType() == uhdmarray_var) {
          array_var* avar = (array_var*)previous;
          VectorOfvariables* vars = avar->Variables();
          variables* actual = nullptr;
          if (vars && vars->size()) {
            actual = vars->at(0);
            UHDM_OBJECT_TYPE actual_type = actual->UhdmType();
            switch (actual_type) {
              case uhdmstruct_net:
              case uhdmstruct_var: {
                struct_typespec* stpt = nullptr;
                if (actual->UhdmType() == uhdmstruct_net) {
                  stpt = (struct_typespec*)((struct_net*)actual)->Typespec();
                } else if (actual->UhdmType() == uhdmstruct_var) {
                  stpt = (struct_typespec*)((struct_var*)actual)->Typespec();
                }
                if (stpt) {
                  for (typespec_member* member : *stpt->Members()) {
                    if (member->VpiName() == name) {
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(member);
                      }
                      previous = member;
                      found = true;
                      break;
                    }
                  }
                }
                break;
              }
              default:
                break;
            }
          }
        } else if (previous->UhdmType() == uhdmstruct_var ||
                   previous->UhdmType() == uhdmstruct_net) {
          struct_typespec* stpt = nullptr;
          if (previous->UhdmType() == uhdmstruct_net) {
            stpt = (struct_typespec*)((struct_net*)previous)->Typespec();
          } else if (previous->UhdmType() == uhdmstruct_var) {
            stpt = (struct_typespec*)((struct_var*)previous)->Typespec();
          }
          if (stpt) {
            for (typespec_member* member : *stpt->Members()) {
              if (member->VpiName() == name) {
                if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                  cro->Actual_group(member);
                }
                previous = member;
                found = true;
                break;
              }
            }
          }
        } else if (previous->UhdmType() == uhdmmodule_inst) {
          module_inst* mod = (module_inst*)previous;
          if (mod->Variables()) {
            for (variables* var : *mod->Variables()) {
              if (var->VpiName() == name) {
                if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                  cro->Actual_group(var);
                }
                previous = var;
                found = true;
                break;
              }
            }
          }

          if (!found && mod->Nets()) {
            for (nets* n : *mod->Nets()) {
              if (n->VpiName() == name) {
                if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                  cro->Actual_group(n);
                }
                previous = n;
                found = true;
                break;
              }
            }
          }
          if (!found && mod->Modules()) {
            for (auto m : *mod->Modules()) {
              if (m->VpiName() == name || m->VpiName() == nameIndexed) {
                found = true;
                previous = m;
                break;
              }
            }
          }
          break;
        } else if (previous->UhdmType() == uhdmgen_scope) {
          gen_scope* scope = (gen_scope*)previous;
          if (obj->UhdmType() == uhdmmethod_func_call) {
            method_func_call* call = (method_func_call*)current;
            if (scope->Task_funcs()) {
              for (auto tf : *scope->Task_funcs()) {
                if (tf->VpiName() == name) {
                  call->Function(any_cast<function*>(tf));
                  previous = (any*)call->Function();
                  found = true;
                  break;
                }
              }
            }
          } else if (obj->UhdmType() == uhdmmethod_task_call) {
            method_task_call* call = (method_task_call*)current;
            if (scope->Task_funcs()) {
              for (auto tf : *scope->Task_funcs()) {
                if (tf->VpiName() == name) {
                  call->Task(any_cast<task*>(tf));
                  found = true;
                  previous = (any*)call->Task();
                  break;
                }
              }
            }
          } else {
            if (scope->Modules()) {
              for (auto m : *scope->Modules()) {
                if (m->VpiName() == name || m->VpiName() == nameIndexed) {
                  found = true;
                  previous = m;
                  if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                    cro->Actual_group(m);
                  }
                  break;
                }
              }
            }
          }
        }
      }
      if (!found) previous = current;
    }
  }
  if (auto vec = source->VpiUses()) {
    auto clone_vec = m_serializer->MakeAnyVec();
    clone_vec->reserve(vec->size());
    clone->VpiUses(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(cloneAny(obj, clone));
    }
  }
  if (uniquifyTypespec()) {
    if (auto obj = source->Typespec())
      clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));
  } else {
    if (auto obj = source->Typespec())
      clone->Typespec(const_cast<typespec*>(obj));
  }

  return clone;
}

any* Elaborator::cloneIndexed_part_select(const indexed_part_select* source,
                                          any* parent) {
  any* const clone = basetype_t::cloneIndexed_part_select(source, parent);
  if (any* res = bindAny(clone->VpiName())) {
    static_cast<indexed_part_select*>(clone)->Actual_group(res);
  }
  return clone;
}

any* Elaborator::cloneInterface_inst(const interface_inst* source,
                                     any* parent) {
  interface_inst* const clone = m_serializer->MakeInterface_inst();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);

  const std::string_view instName = clone->VpiName();
  const std::string_view defName = clone->VpiDefName();
  bool flatModule =
      instName.empty() && ((clone->VpiParent() == 0) ||
                           ((clone->VpiParent() != 0) &&
                            (clone->VpiParent()->VpiType() != vpiModule)));
  // false when it is an interface in a hierachy tree
  if (debug_)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    flatComponentMap_.emplace(clone->VpiDefName(), clone);
  } else {
    // Hierachical module list (elaborated)
    inHierarchy_ = true;

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

    if (clone->Interfaces()) {
      for (interface_inst* inter : *clone->Interfaces()) {
        netMap.emplace(inter->VpiName(), inter);
      }
    }
    if (clone->Interface_arrays()) {
      for (interface_array* inter : *clone->Interface_arrays()) {
        for (instance* interf : *inter->Instances())
          netMap.emplace(interf->VpiName(), interf);
      }
    }
    if (clone->Named_events()) {
      for (named_event* var : *clone->Named_events()) {
        netMap.emplace(var->VpiName(), var);
      }
    }

    // Collect instance parameters, defparams
    ComponentMap paramMap;
    if (clone->Param_assigns()) {
      for (param_assign* passign : *clone->Param_assigns()) {
        paramMap.insert(ComponentMap::value_type(passign->Lhs()->VpiName(),
                                                 passign->Rhs()));
      }
    }
    if (clone->Parameters()) {
      for (any* param : *clone->Parameters()) {
        ComponentMap::iterator itr = paramMap.find(param->VpiName());
        if ((itr != paramMap.end()) && ((*itr).second == nullptr)) {
          paramMap.erase(itr);
        }
        paramMap.emplace(param->VpiName(), param);
      }
    }

    if (clone->Ports()) {
      for (ports* port : *clone->Ports()) {
        if (const ref_obj* ro = port->Low_conn<ref_obj>()) {
          if (const any* actual = ro->Actual_group()) {
            if (actual->UhdmType() == uhdminterface_inst) {
              netMap.emplace(port->VpiName(), actual);
            } else if (actual->UhdmType() == uhdmmodport) {
              // If the interface of the modport is not yet in the map
              netMap.emplace(port->VpiName(), actual);
            }
          }
        }
      }
    }

    // Collect func and task declaration
    ComponentMap funcMap;
    if (clone->Task_funcs()) {
      for (task_func* var : *clone->Task_funcs()) {
        funcMap.emplace(var->VpiName(), var);
      }
    }

    // Check if Module instance has a definition, collect enums
    ComponentMap::iterator itrDef = flatComponentMap_.find(defName);
    if (itrDef != flatComponentMap_.end()) {
      const any* comp = (*itrDef).second;
      int32_t compType = comp->VpiType();
      switch (compType) {
        case vpiModule: {
          module_inst* defMod = (module_inst*)comp;
          if (defMod->Typespecs()) {
            for (typespec* tps : *defMod->Typespecs()) {
              if (tps->UhdmType() == uhdmenum_typespec) {
                enum_typespec* etps = (enum_typespec*)tps;
                for (enum_const* econst : *etps->Enum_consts()) {
                  paramMap.emplace(econst->VpiName(), econst);
                }
              }
            }
          }
        }
      }
    }

    // Collect gen_scope
    if (clone->Gen_scope_arrays()) {
      for (gen_scope_array* gsa : *clone->Gen_scope_arrays()) {
        for (gen_scope* gs : *gsa->Gen_scopes()) {
          netMap.emplace(gsa->VpiName(), gs);
        }
      }
    }
    ComponentMap modMap;

    if (const clocking_block* block = clone->Default_clocking()) {
      modMap.emplace(block->VpiName(), block);
    }

    if (const clocking_block* block = clone->Global_clocking()) {
      modMap.emplace(block->VpiName(), block);
    }

    if (clone->Clocking_blocks()) {
      for (clocking_block* block : *clone->Clocking_blocks()) {
        modMap.emplace(block->VpiName(), block);
      }
    }

    // Push instance context on the stack
    instStack_.emplace_back(clone, netMap, paramMap, funcMap, modMap);

    // Check if Module instance has a definition
    if (itrDef != flatComponentMap_.end()) {
      const any* comp = (*itrDef).second;
      int32_t compType = comp->VpiType();
      switch (compType) {
        case vpiInterface: {
          //  interface* defMod = (interface*)comp;
          if (clone_) {
            // Don't activate yet  <INTERFACE//regexp
            // trap//_ELABORATOR_LISTENER> We need to enter/leave modports and
            // perform binding so not to loose the binding performed loosely
            // during Surelog elab
          }
          break;
        }
        default:
          break;
      }
    }
  }

  source->DeepCopy(clone, parent, this);

  bindScheduledTaskFunc();
  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
  }

  return clone;
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

any* Elaborator::cloneMethod_func_call(const method_func_call* source,
                                       any* parent) {
  const expr* prefix = source->Prefix();
  if (prefix) {
    prefix = static_cast<expr*>(
        cloneAny(prefix, const_cast<method_func_call*>(source)));
  }
  bool is_function = isFunctionCall(source->VpiName(), prefix);
  tf_call* tf_call_clone = nullptr;
  if (is_function) {
    method_func_call* const clone = m_serializer->MakeMethod_func_call();
    tf_call_clone = clone;
    const uint32_t id = clone->UhdmId();
    *clone = *source;
    clone->UhdmId(id);
    clone->VpiParent(parent);
    // enterMethod_func_call(clone);
    if (auto obj = source->Prefix())
      clone->Prefix(static_cast<expr*>(cloneAny(obj, clone)));
    const ref_obj* ref = clone->Prefix<ref_obj>();
    const class_var* varprefix = nullptr;
    if (ref) varprefix = ref->Actual_group<class_var>();
    scheduleTaskFuncBinding(clone, varprefix);
    any* pushedVar = nullptr;
    if (auto vec = source->Tf_call_args()) {
      auto clone_vec = m_serializer->MakeAnyVec();
      clone_vec->reserve(vec->size());
      clone->Tf_call_args(clone_vec);
      for (auto obj : *vec) {
        any* arg = cloneAny(obj, clone);
        // CB callbacks_to_append[$];
        // unique_callbacks_to_append = callbacks_to_append.unique( cb_ )
        // with ( cb_.get_inst_id );
        if (parent->UhdmType() == uhdmhier_path) {
          hier_path* phier = (hier_path*)parent;
          any* last = phier->Path_elems()->back();
          if (ref_obj* last_ref = any_cast<ref_obj*>(last)) {
            if (const any* actual = last_ref->Actual_group()) {
              if (ref_obj* refarg = any_cast<ref_obj*>(arg)) {
                bool override = false;
                if (const any* act = refarg->Actual_group()) {
                  if (act->VpiName() == obj->VpiName()) {
                    override = true;
                  }
                } else {
                  override = true;
                }
                if (override) {
                  if (actual->UhdmType() == uhdmarray_var) {
                    array_var* arr = (array_var*)actual;
                    if (!arr->Variables()->empty()) {
                      variables* var = arr->Variables()->front();
                      if (variables* varclone =
                              static_cast<variables*>(cloneAny(
                                  var, const_cast<any*>(obj->VpiParent())))) {
                        varclone->VpiName(obj->VpiName());
                        actual = varclone;
                        pushVar(varclone);
                        pushedVar = varclone;
                      }
                    }
                  }
                  refarg->Actual_group((any*)actual);
                }
              }
            }
          }
        }
        clone_vec->emplace_back(arg);
      }
    }
    if (auto obj = source->With()) clone->With(cloneAny(obj, clone));
    if (pushedVar) popVar(pushedVar);
    if (auto obj = source->Scope())
      clone->Scope(static_cast<scope*>(cloneAny(obj, clone)));
    if (auto obj = source->Typespec())
      clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));
  } else {
    method_task_call* const clone = m_serializer->MakeMethod_task_call();
    tf_call_clone = clone;
    //*clone = *source;
    clone->VpiName(source->VpiName());
    clone->VpiParent(parent);
    clone->VpiFile(source->VpiFile());
    clone->VpiLineNo(source->VpiLineNo());
    clone->VpiColumnNo(source->VpiColumnNo());
    clone->VpiEndLineNo(source->VpiEndLineNo());
    clone->VpiEndColumnNo(source->VpiEndColumnNo());
    clone->Tf_call_args(source->Tf_call_args());
    // enterMethod_func_call(clone);
    if (auto obj = source->Prefix())
      clone->Prefix(static_cast<expr*>(cloneAny(obj, clone)));
    const ref_obj* ref = any_cast<const ref_obj*>(clone->Prefix());
    const class_var* varprefix = nullptr;
    if (ref) varprefix = any_cast<const class_var*>(ref->Actual_group());
    scheduleTaskFuncBinding(clone, varprefix);
    if (auto obj = source->With()) clone->With(cloneAny(obj, clone));
    if (auto obj = source->Scope())
      clone->Scope(static_cast<scope*>(cloneAny(obj, clone)));
    if (auto vec = source->Tf_call_args()) {
      auto clone_vec = m_serializer->MakeAnyVec();
      clone_vec->reserve(vec->size());
      clone->Tf_call_args(clone_vec);
      for (auto obj : *vec) {
        clone_vec->emplace_back(cloneAny(obj, clone));
      }
    }
    if (auto obj = source->Typespec())
      clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));
  }

  // leaveMethod_func_call(clone);
  return tf_call_clone;
}

any* Elaborator::cloneMethod_task_call(const method_task_call* source,
                                       any* parent) {
  const expr* prefix = source->Prefix();
  if (prefix)
    prefix = static_cast<expr*>(
        cloneAny(prefix, const_cast<method_task_call*>(source)));
  bool is_task = isTaskCall(source->VpiName(), prefix);
  tf_call* tf_call_clone = nullptr;
  if (is_task) {
    method_task_call* const clone = m_serializer->MakeMethod_task_call();
    tf_call_clone = clone;
    const uint32_t id = clone->UhdmId();
    *clone = *source;
    clone->UhdmId(id);
    clone->VpiParent(parent);
    if (auto obj = source->Prefix())
      clone->Prefix(static_cast<expr*>(cloneAny(obj, clone)));
    const ref_obj* ref = any_cast<const ref_obj*>(clone->Prefix());
    const class_var* varprefix = nullptr;
    if (ref) varprefix = any_cast<const class_var*>(ref->Actual_group());
    scheduleTaskFuncBinding(clone, varprefix);
    if (auto obj = source->With()) clone->With(cloneAny(obj, clone));
    if (auto obj = source->Scope())
      clone->Scope(static_cast<scope*>(cloneAny(obj, clone)));
    if (auto vec = source->Tf_call_args()) {
      auto clone_vec = m_serializer->MakeAnyVec();
      clone_vec->reserve(vec->size());
      clone->Tf_call_args(clone_vec);
      for (auto obj : *vec) {
        clone_vec->emplace_back(cloneAny(obj, clone));
      }
    }
    if (auto obj = source->Typespec())
      clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));
  } else {
    method_func_call* const clone = m_serializer->MakeMethod_func_call();
    tf_call_clone = clone;
    //*clone = *source;
    clone->VpiParent(parent);
    clone->VpiName(source->VpiName());
    clone->VpiFile(source->VpiFile());
    clone->VpiLineNo(source->VpiLineNo());
    clone->VpiColumnNo(source->VpiColumnNo());
    clone->VpiEndLineNo(source->VpiEndLineNo());
    clone->VpiEndColumnNo(source->VpiEndColumnNo());
    clone->Tf_call_args(source->Tf_call_args());
    if (auto obj = source->Prefix())
      clone->Prefix(static_cast<expr*>(cloneAny(obj, clone)));
    const ref_obj* ref = any_cast<const ref_obj*>(clone->Prefix());
    const class_var* varprefix = nullptr;
    if (ref) varprefix = any_cast<const class_var*>(ref->Actual_group());
    scheduleTaskFuncBinding(clone, varprefix);
    if (auto obj = source->With()) clone->With(cloneAny(obj, clone));
    if (auto obj = source->Scope())
      clone->Scope(static_cast<scope*>(cloneAny(obj, clone)));
    if (auto vec = source->Tf_call_args()) {
      auto clone_vec = m_serializer->MakeAnyVec();
      clone_vec->reserve(vec->size());
      clone->Tf_call_args(clone_vec);
      for (auto obj : *vec) {
        clone_vec->emplace_back(cloneAny(obj, clone));
      }
    }
    if (auto obj = source->Typespec())
      clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));
  }
  return tf_call_clone;
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

any* Elaborator::cloneModule_inst(const module_inst* source, any* parent) {
  module_inst* const clone = const_cast<module_inst*>(source);

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
    // Hierachical module list (elaborated)
    inHierarchy_ = true;

    // Collect instance elaborated nets
    ComponentMap netMap;
    if (clone->Nets()) {
      for (net* net : *clone->Nets()) {
        netMap.emplace(net->VpiName(), net);
      }
    }
    if (clone->Variables()) {
      for (variables* var : *clone->Variables()) {
        netMap.emplace(var->VpiName(), var);
        if (var->UhdmType() == uhdmenum_var) {
          enum_var* evar = (enum_var*)var;
          enum_typespec* etps = evar->Typespec<enum_typespec>();
          for (auto c : *etps->Enum_consts()) {
            netMap.emplace(c->VpiName(), c);
          }
        }
      }
    }
    if (clone->Interfaces()) {
      for (interface_inst* inter : *clone->Interfaces()) {
        netMap.emplace(inter->VpiName(), inter);
      }
    }
    if (clone->Interface_arrays()) {
      for (interface_array* inter : *clone->Interface_arrays()) {
        if (VectorOfinstance* instances = inter->Instances()) {
          for (instance* interf : *instances) {
            netMap.emplace(interf->VpiName(), interf);
          }
        }
      }
    }
    if (clone->Ports()) {
      for (port* port : *clone->Ports()) {
        if (const ref_obj* low = port->Low_conn<ref_obj>()) {
          if (const modport* actual = low->Actual_group<modport>()) {
            // If the interface of the modport is not yet in the map
            netMap.emplace(port->VpiName(), actual);
          }
        }
      }
    }
    if (clone->Array_nets()) {
      for (array_net* net : *clone->Array_nets()) {
        netMap.emplace(net->VpiName(), net);
      }
    }
    if (clone->Named_events()) {
      for (named_event* var : *clone->Named_events()) {
        netMap.emplace(var->VpiName(), var);
      }
    }

    // Collect instance parameters, defparams
    ComponentMap paramMap;
    if (muteErrors_ == true) {
      // In final hier_path binding we need the formal parameter, not the actual
      if (clone->Param_assigns()) {
        for (param_assign* passign : *clone->Param_assigns()) {
          paramMap.emplace(passign->Lhs()->VpiName(), passign->Rhs());
        }
      }
    }
    if (clone->Parameters()) {
      for (any* param : *clone->Parameters()) {
        ComponentMap::iterator itr = paramMap.find(param->VpiName());
        if ((itr != paramMap.end()) && ((*itr).second == nullptr)) {
          paramMap.erase(itr);
        }
        paramMap.emplace(param->VpiName(), param);
      }
    }
    if (clone->Def_params()) {
      for (def_param* param : *clone->Def_params()) {
        paramMap.emplace(param->VpiName(), param);
      }
    }
    if (clone->Typespecs()) {
      for (typespec* tps : *clone->Typespecs()) {
        if (tps->UhdmType() == uhdmenum_typespec) {
          enum_typespec* etps = (enum_typespec*)tps;
          for (auto c : *etps->Enum_consts()) {
            paramMap.emplace(c->VpiName(), c);
          }
        }
      }
    }
    if (clone->Ports()) {
      for (ports* port : *clone->Ports()) {
        if (const ref_obj* low = port->Low_conn<ref_obj>()) {
          if (const interface_inst* actual =
                  low->Actual_group<interface_inst>()) {
            netMap.emplace(port->VpiName(), actual);
          }
        }
      }
    }

    // Collect func and task declaration
    ComponentMap funcMap;
    if (clone->Task_funcs()) {
      for (task_func* var : *clone->Task_funcs()) {
        funcMap.emplace(var->VpiName(), var);
      }
    }

    ComponentMap modMap;

    // Check if Module instance has a definition, collect enums
    ComponentMap::iterator itrDef = flatComponentMap_.find(defName);
    if (itrDef != flatComponentMap_.end()) {
      const any* comp = (*itrDef).second;
      int32_t compType = comp->VpiType();
      switch (compType) {
        case vpiModule: {
          module_inst* defMod = (module_inst*)comp;
          if (defMod->Typespecs()) {
            for (typespec* tps : *defMod->Typespecs()) {
              if (tps->UhdmType() == uhdmenum_typespec) {
                enum_typespec* etps = (enum_typespec*)tps;
                for (enum_const* econst : *etps->Enum_consts()) {
                  paramMap.emplace(econst->VpiName(), econst);
                }
              }
            }
          }
        }
      }
    }

    // Collect gen_scope
    if (clone->Gen_scope_arrays()) {
      for (gen_scope_array* gsa : *clone->Gen_scope_arrays()) {
        for (gen_scope* gs : *gsa->Gen_scopes()) {
          netMap.emplace(gsa->VpiName(), gs);
        }
      }
    }

    // Module itself
    std::string_view modName = ltrim_until(clone->VpiName(), '@');
    modMap.emplace(modName, clone);

    if (clone->Modules()) {
      for (module_inst* mod : *clone->Modules()) {
        modMap.emplace(mod->VpiName(), mod);
      }
    }
    if (clone->Module_arrays()) {
      for (module_array* mod : *clone->Module_arrays()) {
        modMap.emplace(mod->VpiName(), mod);
      }
    }
    if (const clocking_block* block = clone->Default_clocking()) {
      modMap.emplace(block->VpiName(), block);
    }
    if (const clocking_block* block = clone->Global_clocking()) {
      modMap.emplace(block->VpiName(), block);
    }
    if (clone->Clocking_blocks()) {
      for (clocking_block* block : *clone->Clocking_blocks()) {
        modMap.emplace(block->VpiName(), block);
      }
    }

    // Push instance context on the stack
    instStack_.emplace_back(clone, netMap, paramMap, funcMap, modMap);
  }
  if (!muteErrors_) {
    elabModule_inst(clone);
  }

  bindScheduledTaskFunc();

  if (inHierarchy_ && !instStack_.empty() &&
      (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
    if (instStack_.empty()) {
      inHierarchy_ = false;
    }
  }

  return clone;
}

any* Elaborator::cloneNamed_begin(const named_begin* source, any* parent) {
  named_begin* const clone = m_serializer->MakeNamed_begin();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);

  ComponentMap varMap;
  if (!instStack_.empty()) {
    ComponentMap& modMap = std::get<4>(instStack_.back());
    modMap.emplace(clone->VpiName(), clone);
  }
  if (clone->Array_vars()) {
    for (variables* var : *clone->Array_vars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (clone->Variables()) {
    for (variables* var : *clone->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(clone, varMap, paramMap, funcMap, modMap);

  source->DeepCopy(clone, parent, this);

  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
  }

  return clone;
}

any* Elaborator::cloneNamed_fork(const named_fork* source, any* parent) {
  named_fork* const clone = m_serializer->MakeNamed_fork();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);

  ComponentMap varMap;
  if (!instStack_.empty()) {
    ComponentMap& modMap = std::get<4>(instStack_.back());
    modMap.emplace(clone->VpiName(), clone);
  }
  if (clone->Array_vars()) {
    for (variables* var : *clone->Array_vars()) {
      varMap.emplace(var->VpiName(), var);
    }
  }
  if (clone->Variables()) {
    for (variables* var : *clone->Variables()) {
      varMap.emplace(var->VpiName(), var);
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  instStack_.emplace_back(clone, varMap, paramMap, funcMap, modMap);

  source->DeepCopy(clone, parent, this);

  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
  }

  return clone;
}

any* Elaborator::clonePackage(const package* source, any* parent) {
  package* const clone = const_cast<package*>(source);

  ComponentMap netMap;

  if (clone->Array_vars()) {
    for (variables* var : *clone->Array_vars()) {
      netMap.emplace(var->VpiName(), var);
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
  if (clone->Named_events()) {
    for (named_event* var : *clone->Named_events()) {
      netMap.emplace(var->VpiName(), var);
    }
  }

  // Collect instance parameters, defparams
  ComponentMap paramMap;
  if (clone->Parameters()) {
    for (any* param : *clone->Parameters()) {
      paramMap.emplace(param->VpiName(), param);
    }
  }

  // Collect func and task declaration
  ComponentMap funcMap;
  ComponentMap modMap;
  // Push instance context on the stack
  instStack_.emplace_back(clone, netMap, paramMap, funcMap, modMap);

  source->DeepCopy(clone, parent, this);

  if (clone_) {
    if (auto vec = clone->Task_funcs()) {
      auto clone_vec = m_serializer->MakeTask_funcVec();
      clone_vec->reserve(vec->size());
      clone->Task_funcs(clone_vec);
      for (auto obj : *vec) {
        enterTask_func(obj);
        task_func* tf = static_cast<task_func*>(cloneAny(obj, clone));
        ComponentMap& funcMap1 =
            std::get<3>(instStack_.at(instStack_.size() - 2));
        auto it = funcMap1.find(tf->VpiName());
        if (it != funcMap1.end()) funcMap1.erase(it);
        funcMap1.emplace(tf->VpiName(), tf);
        leaveTask_func(obj);
        tf->VpiParent(clone);
        clone_vec->emplace_back(tf);
      }
    }
  }

  bindScheduledTaskFunc();

  if (!instStack_.empty() && (std::get<0>(instStack_.back()) == clone)) {
    instStack_.pop_back();
  }

  return clone;
}

any* Elaborator::clonePart_select(const part_select* source, any* parent) {
  any* const clone = basetype_t::clonePart_select(source, parent);
  if (any* res = bindAny(clone->VpiName())) {
    static_cast<part_select*>(clone)->Actual_group(res);
  }
  return clone;
}

any* Elaborator::cloneRef_obj(const ref_obj* source, any* parent) {
  any* const clone = basetype_t::cloneRef_obj(source, parent);
  if (any* res = bindAny(clone->VpiName())) {
    static_cast<ref_obj*>(clone)->Actual_group(res);
  }
  return clone;
}

any* Elaborator::cloneSys_func_call(const sys_func_call* source, any* parent) {
  sys_func_call* const clone = m_serializer->MakeSys_func_call();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);
  clone->VpiParent(parent);
  if (auto obj = source->User_systf())
    clone->User_systf(static_cast<user_systf*>(cloneAny(obj, clone)));
  if (auto obj = source->Scope())
    clone->Scope(static_cast<scope*>(cloneAny(obj, clone)));
  if (auto vec = source->Tf_call_args()) {
    auto clone_vec = m_serializer->MakeAnyVec();
    clone_vec->reserve(vec->size());
    clone->Tf_call_args(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(cloneAny(obj, clone));
    }
  }
  if (auto obj = source->Typespec())
    clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));

  return clone;
}

any* Elaborator::cloneSys_task_call(const sys_task_call* source, any* parent) {
  sys_task_call* const clone = m_serializer->MakeSys_task_call();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);

  clone->VpiParent(parent);
  if (auto obj = source->User_systf())
    clone->User_systf(static_cast<user_systf*>(cloneAny(obj, clone)));
  if (auto obj = source->Scope())
    clone->Scope(static_cast<scope*>(cloneAny(obj, clone)));
  if (auto vec = source->Tf_call_args()) {
    auto clone_vec = m_serializer->MakeAnyVec();
    clone_vec->reserve(vec->size());
    clone->Tf_call_args(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(cloneAny(obj, clone));
    }
  }
  if (auto obj = source->Typespec())
    clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));

  return clone;
}

any* Elaborator::cloneTagged_pattern(const tagged_pattern* source,
                                     any* parent) {
  if (uniquifyTypespec()) {
    tagged_pattern* const clone = m_serializer->MakeTagged_pattern();
    const uint32_t id = clone->UhdmId();
    *clone = *source;
    clone->UhdmId(id);
    clone->VpiParent(parent);
    if (auto obj = source->Typespec())
      clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));
    if (auto obj = source->Pattern()) clone->Pattern(cloneAny(obj, clone));
    return clone;
  } else {
    return const_cast<tagged_pattern*>(source);
  }
}

any* Elaborator::cloneTask(const task* source, any* parent) {
  task* const clone = m_serializer->MakeTask();
  const uint32_t id = clone->UhdmId();
  *clone = *source;
  clone->UhdmId(id);
  clone->VpiParent(parent);
  if (auto obj = source->Left_range())
    clone->Left_range(static_cast<expr*>(cloneAny(obj, clone)));
  if (auto obj = source->Right_range())
    clone->Right_range(static_cast<expr*>(cloneAny(obj, clone)));
  if (auto obj = source->Return())
    clone->Return(static_cast<variables*>(cloneAny(obj, clone)));
  if (auto obj = source->Instance())
    clone->Instance(const_cast<instance*>(obj));
  if (instance* inst = any_cast<instance*>(parent)) clone->Instance(inst);
  if (auto obj = source->Class_defn())
    clone->Class_defn(static_cast<clocking_block*>(cloneAny(obj, clone)));
  if (auto vec = source->Io_decls()) {
    auto clone_vec = m_serializer->MakeIo_declVec();
    clone_vec->reserve(vec->size());
    clone->Io_decls(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<io_decl*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Variables()) {
    auto clone_vec = m_serializer->MakeVariablesVec();
    clone_vec->reserve(vec->size());
    clone->Variables(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<variables*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Scopes()) {
    auto clone_vec = m_serializer->MakeScopeVec();
    clone_vec->reserve(vec->size());
    clone->Scopes(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<scope*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Typespecs()) {
    auto clone_vec = m_serializer->MakeTypespecVec();
    clone_vec->reserve(vec->size());
    clone->Typespecs(clone_vec);
    for (auto obj : *vec) {
      if (uniquifyTypespec()) {
        clone_vec->emplace_back(static_cast<typespec*>(cloneAny(obj, clone)));
      } else {
        clone_vec->emplace_back(obj);
      }
    }
  }
  enterTask_func(clone);
  if (auto vec = source->Concurrent_assertions()) {
    auto clone_vec = m_serializer->MakeConcurrent_assertionsVec();
    clone_vec->reserve(vec->size());
    clone->Concurrent_assertions(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<concurrent_assertions*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Property_decls()) {
    auto clone_vec = m_serializer->MakeProperty_declVec();
    clone_vec->reserve(vec->size());
    clone->Property_decls(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<property_decl*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Sequence_decls()) {
    auto clone_vec = m_serializer->MakeSequence_declVec();
    clone_vec->reserve(vec->size());
    clone->Sequence_decls(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<sequence_decl*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Named_events()) {
    auto clone_vec = m_serializer->MakeNamed_eventVec();
    clone_vec->reserve(vec->size());
    clone->Named_events(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<named_event*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Named_event_arrays()) {
    auto clone_vec = m_serializer->MakeNamed_event_arrayVec();
    clone_vec->reserve(vec->size());
    clone->Named_event_arrays(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<named_event_array*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Virtual_interface_vars()) {
    auto clone_vec = m_serializer->MakeVirtual_interface_varVec();
    clone_vec->reserve(vec->size());
    clone->Virtual_interface_vars(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(
          static_cast<virtual_interface_var*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Logic_vars()) {
    auto clone_vec = m_serializer->MakeLogic_varVec();
    clone_vec->reserve(vec->size());
    clone->Logic_vars(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<logic_var*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Array_vars()) {
    auto clone_vec = m_serializer->MakeArray_varVec();
    clone_vec->reserve(vec->size());
    clone->Array_vars(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<array_var*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Array_var_mems()) {
    auto clone_vec = m_serializer->MakeArray_varVec();
    clone_vec->reserve(vec->size());
    clone->Array_var_mems(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<array_var*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Param_assigns()) {
    auto clone_vec = m_serializer->MakeParam_assignVec();
    clone_vec->reserve(vec->size());
    clone->Param_assigns(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<param_assign*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Let_decls()) {
    auto clone_vec = m_serializer->MakeLet_declVec();
    clone_vec->reserve(vec->size());
    clone->Let_decls(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<let_decl*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Attributes()) {
    auto clone_vec = m_serializer->MakeAttributeVec();
    clone_vec->reserve(vec->size());
    clone->Attributes(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<attribute*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Parameters()) {
    auto clone_vec = m_serializer->MakeAnyVec();
    clone_vec->reserve(vec->size());
    clone->Parameters(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(static_cast<parameter*>(cloneAny(obj, clone)));
    }
  }
  if (auto vec = source->Instance_items()) {
    auto clone_vec = m_serializer->MakeAnyVec();
    clone_vec->reserve(vec->size());
    clone->Instance_items(clone_vec);
    for (auto obj : *vec) {
      clone_vec->emplace_back(cloneAny(obj, clone));
    }
  }
  if (auto obj = source->Stmt()) clone->Stmt(cloneAny(obj, clone));
  leaveTask_func(clone);
  return clone;
}

any* Elaborator::cloneTask_call(const task_call* source, any* parent) {
  bool is_task = isTaskCall(source->VpiName(), nullptr);
  tf_call* tf_call_clone = nullptr;
  if (is_task) {
    task_call* const clone = m_serializer->MakeTask_call();
    const uint32_t id = clone->UhdmId();
    *clone = *source;
    clone->UhdmId(id);
    tf_call_clone = clone;
    clone->VpiParent(parent);
    scheduleTaskFuncBinding(clone, nullptr);
    if (auto obj = source->Scope())
      clone->Scope(static_cast<scope*>(cloneAny(obj, clone)));
    if (auto vec = source->Tf_call_args()) {
      auto clone_vec = m_serializer->MakeAnyVec();
      clone_vec->reserve(vec->size());
      clone->Tf_call_args(clone_vec);
      for (auto obj : *vec) {
        clone_vec->emplace_back(cloneAny(obj, clone));
      }
    }
    if (auto obj = source->Typespec())
      clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));
  } else {
    func_call* const clone = m_serializer->MakeFunc_call();
    tf_call_clone = clone;
    //*clone = *source;
    clone->VpiParent(parent);
    clone->VpiName(source->VpiName());
    clone->VpiFile(source->VpiFile());
    clone->VpiLineNo(source->VpiLineNo());
    clone->VpiColumnNo(source->VpiColumnNo());
    clone->VpiEndLineNo(source->VpiEndLineNo());
    clone->VpiEndColumnNo(source->VpiEndColumnNo());
    clone->Tf_call_args(source->Tf_call_args());
    scheduleTaskFuncBinding(clone, nullptr);
    if (auto obj = source->Scope())
      clone->Scope(static_cast<scope*>(cloneAny(obj, clone)));
    if (auto vec = source->Tf_call_args()) {
      auto clone_vec = m_serializer->MakeAnyVec();
      clone_vec->reserve(vec->size());
      clone->Tf_call_args(clone_vec);
      for (auto obj : *vec) {
        clone_vec->emplace_back(cloneAny(obj, clone));
      }
    }
    if (auto obj = source->Typespec())
      clone->Typespec(static_cast<typespec*>(cloneAny(obj, clone)));
  }
  return tf_call_clone;
}

any* Elaborator::cloneVar_select(const var_select* source, any* parent) {
  any* const clone = basetype_t::cloneVar_select(source, parent);
  if (any* res = bindAny(clone->VpiName())) {
    static_cast<var_select*>(clone)->Actual_group(res);
  }
  return clone;
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
    const typespec* tps = prefix->Typespec();
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
          if (const class_typespec* ctps = ext->Class_typespec()) {
            base_defn = ctps->Class_defn();
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
