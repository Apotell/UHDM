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
#include <uhdm/uhdm.h>

#include <iostream>

namespace UHDM {
Elaborator::Elaborator(Serializer* serializer, bool debug /* = false */,
                       bool muteErrors /* = false */)
    : m_serializer(serializer), m_debug(debug), m_muteErrors(muteErrors) {}

static void propagateParamAssign(param_assign* pass, const any* target) {
  UHDM_OBJECT_TYPE targetType = target->UhdmType();
  Serializer& s = *pass->GetSerializer();
  switch (targetType) {
    case UHDM_OBJECT_TYPE::uhdmclass_defn: {
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
        if (const ref_typespec* rt = ext->Class_typespec()) {
          propagateParamAssign(pass, rt->Actual_typespec<class_typespec>());
        }
      }
      if (const auto vars = defn->Variables()) {
        for (auto var : *vars) {
          propagateParamAssign(pass, var);
        }
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmclass_var: {
      class_var* var = (class_var*)target;
      if (const ref_typespec* rt = var->Typespec()) {
        propagateParamAssign(pass, rt->Actual_typespec());
      }
      break;
    }
    case UHDM_OBJECT_TYPE::uhdmclass_typespec: {
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

void Elaborator::enterVariables(const variables* object, vpiHandle handle) {
  if (object->UhdmType() == UHDM_OBJECT_TYPE::uhdmclass_var) {
    if (!m_inHierarchy)
      return;  // Only do class var propagation while in elaboration
    const class_var* cv = (class_var*)object;
    class_var* const rw_cv = (class_var*)cv;
    if (const ref_typespec* tps = cv->Typespec()) {
      ref_typespec* ctps = clone(tps, rw_cv);
      rw_cv->Typespec(ctps);
      if (const class_typespec* cctps =
              ctps->Actual_typespec<class_typespec>()) {
        if (VectorOfparam_assign* params = cctps->Param_assigns()) {
          for (param_assign* pass : *params) {
            propagateParamAssign(pass, cctps->Class_defn());
          }
        }
      }
    }
  }
}

void Elaborator::enterAny(const any* object, vpiHandle handle) {
  if (const variables* const var = any_cast<const variables*>(object)) {
    enterVariables(var, handle);
  }
}

void Elaborator::leaveDesign(const design* object, vpiHandle handle) {
  const_cast<design*>(object)->VpiElaborated(true);
}

static std::string_view ltrim_until(std::string_view str, char c) {
  auto it = str.find(c);
  if (it != std::string_view::npos) str.remove_prefix(it + 1);
  return str;
}

void Elaborator::enterModule_inst(const module_inst* object, vpiHandle handle) {
  bool topLevelModule = object->VpiTopModule();
  const std::string_view instName = object->VpiName();
  const std::string_view defName = object->VpiDefName();
  bool flatModule =
      instName.empty() && ((object->VpiParent() == 0) ||
                           ((object->VpiParent() != 0) &&
                            (object->VpiParent()->VpiType() != vpiModule)));
  // false when it is a module in a hierachy tree
  if (m_debug)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << ", Top:" << topLevelModule
              << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    m_flatComponentMap.emplace(object->VpiDefName(), object);
  } else {
    // Hierachical module list (elaborated)
    m_inHierarchy = true;

    // Collect instance elaborated nets
    ComponentMap netMap;
    if (object->Nets()) {
      for (net* net : *object->Nets()) {
        if (!net->VpiName().empty()) {
          netMap.emplace(net->VpiName(), net);
        }
      }
    }

    if (object->Variables()) {
      for (variables* var : *object->Variables()) {
        if (!var->VpiName().empty()) {
          netMap.emplace(var->VpiName(), var);
        }
        if (var->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_var) {
          enum_var* evar = (enum_var*)var;
          if (const ref_typespec* rt = evar->Typespec()) {
            if (const enum_typespec* etps =
                    rt->Actual_typespec<enum_typespec>()) {
              for (auto c : *etps->Enum_consts()) {
                if (!c->VpiName().empty()) {
                  netMap.emplace(c->VpiName(), c);
                }
              }
            }
          }
        }
      }
    }

    if (object->Interfaces()) {
      for (interface_inst* inter : *object->Interfaces()) {
        if (!inter->VpiName().empty()) {
          netMap.emplace(inter->VpiName(), inter);
        }
      }
    }
    if (object->Interface_arrays()) {
      for (interface_array* inter : *object->Interface_arrays()) {
        if (VectorOfinstance* instances = inter->Instances()) {
          for (instance* interf : *instances) {
            if (!interf->VpiName().empty()) {
              netMap.emplace(interf->VpiName(), interf);
            }
          }
        }
      }
    }

    if (object->Ports()) {
      for (port* port : *object->Ports()) {
        if (const ref_obj* low = port->Low_conn<ref_obj>()) {
          if (const modport* actual = low->Actual_group<modport>()) {
            // If the interface of the modport is not yet in the map
            if (!port->VpiName().empty()) {
              netMap.emplace(port->VpiName(), actual);
            }
          }
        }
      }
    }
    if (object->Array_nets()) {
      for (array_net* net : *object->Array_nets()) {
        if (!net->VpiName().empty()) {
          netMap.emplace(net->VpiName(), net);
        }
      }
    }

    if (object->Named_events()) {
      for (named_event* var : *object->Named_events()) {
        if (!var->VpiName().empty()) {
          netMap.emplace(var->VpiName(), var);
        }
      }
    }

    // Collect instance parameters, defparams
    ComponentMap paramMap;
    if (m_muteErrors == true) {
      // In final hier_path binding we need the formal parameter, not the actual
      if (object->Param_assigns()) {
        for (param_assign* passign : *object->Param_assigns()) {
          if (!passign->Lhs()->VpiName().empty()) {
            paramMap.emplace(passign->Lhs()->VpiName(), passign->Rhs());
          }
        }
      }
    }
    if (object->Parameters()) {
      for (any* param : *object->Parameters()) {
        ComponentMap::iterator itr = paramMap.find(param->VpiName());
        if ((itr != paramMap.end()) && ((*itr).second == nullptr)) {
          paramMap.erase(itr);
        }
        if (!param->VpiName().empty()) {
          paramMap.emplace(param->VpiName(), param);
        }
      }
    }
    if (object->Def_params()) {
      for (def_param* param : *object->Def_params()) {
        if (!param->VpiName().empty()) {
          paramMap.emplace(param->VpiName(), param);
        }
      }
    }

    if (object->Typespecs()) {
      for (typespec* tps : *object->Typespecs()) {
        if (tps->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_typespec) {
          enum_typespec* etps = (enum_typespec*)tps;
          for (auto c : *etps->Enum_consts()) {
            if (!c->VpiName().empty()) {
              paramMap.emplace(c->VpiName(), c);
            }
          }
        }
      }
    }
    if (object->Ports()) {
      for (ports* port : *object->Ports()) {
        if (const ref_obj* low = port->Low_conn<ref_obj>()) {
          if (const interface_inst* actual =
                  low->Actual_group<interface_inst>()) {
            if (!port->VpiName().empty()) {
              netMap.emplace(port->VpiName(), actual);
            }
          }
        }
      }
    }

    // Collect func and task declaration
    ComponentMap funcMap;
    if (object->Task_funcs()) {
      for (task_func* var : *object->Task_funcs()) {
        if (!var->VpiName().empty()) {
          funcMap.emplace(var->VpiName(), var);
        }
      }
    }

    ComponentMap modMap;

    // Check if Module instance has a definition, collect enums
    ComponentMap::iterator itrDef = m_flatComponentMap.find(defName);
    if (itrDef != m_flatComponentMap.end()) {
      const BaseClass* comp = (*itrDef).second;
      int32_t compType = comp->VpiType();
      switch (compType) {
        case vpiModule: {
          module_inst* defMod = (module_inst*)comp;
          if (defMod->Typespecs()) {
            for (typespec* tps : *defMod->Typespecs()) {
              if (tps->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_typespec) {
                enum_typespec* etps = (enum_typespec*)tps;
                for (enum_const* econst : *etps->Enum_consts()) {
                  if (!econst->VpiName().empty()) {
                    paramMap.emplace(econst->VpiName(), econst);
                  }
                }
              }
            }
          }
        }
      }
    }

    // Collect gen_scope
    if (object->Gen_scope_arrays()) {
      for (gen_scope_array* gsa : *object->Gen_scope_arrays()) {
        if (!gsa->VpiName().empty()) {
          for (gen_scope* gs : *gsa->Gen_scopes()) {
            netMap.emplace(gsa->VpiName(), gs);
          }
        }
      }
    }

    // Module itself
    std::string_view modName = ltrim_until(object->VpiName(), '@');
    if (!modName.empty()) {
      modMap.emplace(modName, object);  // instance
    }
    modName = ltrim_until(object->VpiDefName(), '@');
    if (!modName.empty()) {
      modMap.emplace(modName, object);  // definition
    }

    if (object->Modules()) {
      for (module_inst* mod : *object->Modules()) {
        if (!mod->VpiName().empty()) {
          modMap.emplace(mod->VpiName(), mod);
        }
      }
    }

    if (object->Module_arrays()) {
      for (module_array* mod : *object->Module_arrays()) {
        if (!mod->VpiName().empty()) {
          modMap.emplace(mod->VpiName(), mod);
        }
      }
    }

    if (const clocking_block* block = object->Default_clocking()) {
      if (!block->VpiName().empty()) {
        modMap.emplace(block->VpiName(), block);
      }
    }

    if (const clocking_block* block = object->Global_clocking()) {
      if (!block->VpiName().empty()) {
        modMap.emplace(block->VpiName(), block);
      }
    }

    if (object->Clocking_blocks()) {
      for (clocking_block* block : *object->Clocking_blocks()) {
        if (!block->VpiName().empty()) {
          modMap.emplace(block->VpiName(), block);
        }
      }
    }

    // Push instance context on the stack
    m_instStack.emplace_back(object, netMap, paramMap, funcMap, modMap);
  }
  if (m_muteErrors == false) {
    elabModule_inst(object, handle);
  }
}

void Elaborator::elabModule_inst(const module_inst* object, vpiHandle handle) {
  module_inst* inst = const_cast<module_inst*>(object);
  bool topLevelModule = object->VpiTopModule();
  const std::string_view instName = object->VpiName();
  const std::string_view defName = object->VpiDefName();
  bool flatModule =
      instName.empty() && ((object->VpiParent() == 0) ||
                           ((object->VpiParent() != 0) &&
                            (object->VpiParent()->VpiType() != vpiModule)));
  // false when it is a module in a hierachy tree
  if (m_debug)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << ", Top:" << topLevelModule
              << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    m_flatComponentMap.emplace(object->VpiDefName(), object);
  } else {
    // Do not elab modules used in hier_path base, that creates a loop
    if (inCallstackOfType(uhdmhier_path)) {
      return;
    }
    if (!m_clone) return;
    // Hierachical module list (elaborated)
    m_inHierarchy = true;
    ComponentMap::iterator itrDef = m_flatComponentMap.find(defName);
    // Check if Module instance has a definition
    if (itrDef != m_flatComponentMap.end()) {
      const BaseClass* comp = (*itrDef).second;
      if (comp->VpiType() != vpiModule) return;
      module_inst* defMod = (module_inst*)comp;
//<MODULE_ELABORATOR_LISTENER>
    }
  }
}

void Elaborator::leaveModule_inst(const module_inst* object, vpiHandle handle) {
  bindScheduledTaskFunc();
  if (m_inHierarchy && !m_instStack.empty() &&
      (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
    if (m_instStack.empty()) {
      m_inHierarchy = false;
    }
  }
}

void Elaborator::enterPackage(const package* object, vpiHandle handle) {
  ComponentMap netMap;

  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      if (!var->VpiName().empty()) {
        netMap.emplace(var->VpiName(), var);
      }
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      if (!var->VpiName().empty()) {
        netMap.emplace(var->VpiName(), var);
      }
      if (var->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_var) {
        enum_var* evar = (enum_var*)var;
        if (const ref_typespec* rt = evar->Typespec()) {
          if (const enum_typespec* etps =
                  rt->Actual_typespec<enum_typespec>()) {
            for (auto c : *etps->Enum_consts()) {
              if (!c->VpiName().empty()) {
                netMap.emplace(c->VpiName(), c);
              }
            }
          }
        }
      }
    }
  }

  if (object->Named_events()) {
    for (named_event* var : *object->Named_events()) {
      if (!var->VpiName().empty()) {
        netMap.emplace(var->VpiName(), var);
      }
    }
  }

  // Collect instance parameters, defparams
  ComponentMap paramMap;
  if (object->Parameters()) {
    for (any* param : *object->Parameters()) {
      if (!param->VpiName().empty()) {
        paramMap.emplace(param->VpiName(), param);
      }
    }
  }

  // Collect func and task declaration
  ComponentMap funcMap;
  ComponentMap modMap;
  // Push instance context on the stack
  m_instStack.emplace_back(object, netMap, paramMap, funcMap, modMap);
}

void Elaborator::leavePackage(const package* object, vpiHandle handle) {
  if (m_clone) {
    if (auto vec = object->Task_funcs()) {
      auto clone_vec = m_serializer->MakeTask_funcVec();
      ((package*)object)->Task_funcs(clone_vec);
      for (auto obj : *vec) {
        enterTask_func(obj, nullptr);
        auto* tf = clone(obj, (package*)object);
        if (!tf->VpiName().empty()) {
          ComponentMap& funcMap =
              std::get<3>(m_instStack.at(m_instStack.size() - 2));
          auto it = funcMap.find(tf->VpiName());
          if (it != funcMap.end()) funcMap.erase(it);
          funcMap.emplace(tf->VpiName(), tf);
        }
        leaveTask_func(obj, nullptr);
        tf->VpiParent((package*)object);
        clone_vec->push_back(tf);
      }
    }
  }
  bindScheduledTaskFunc();
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterClass_defn(const class_defn* object, vpiHandle handle) {
  ComponentMap varMap;
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;

  const class_defn* defn = object;
  while (defn != nullptr) {
    // Collect instance elaborated nets
    if (defn->Variables()) {
      for (variables* var : *defn->Variables()) {
        if (!var->VpiName().empty()) {
          varMap.emplace(var->VpiName(), var);
        }
        if (var->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_var) {
          enum_var* evar = (enum_var*)var;
          if (const ref_typespec* rt = evar->Typespec()) {
            if (const enum_typespec* etps = rt->Actual_typespec<enum_typespec>()) {
              for (auto c : *etps->Enum_consts()) {
                if (!c->VpiName().empty()) {
                  varMap.emplace(c->VpiName(), c);
                }
              }
            }
          }
        }
      }
    }

    if (defn->Named_events()) {
      for (named_event* var : *defn->Named_events()) {
        if (!var->VpiName().empty()) {
          varMap.emplace(var->VpiName(), var);
        }
      }
    }

    // Collect instance parameters, defparams
    if (defn->Parameters()) {
      for (any* param : *defn->Parameters()) {
        if (!param->VpiName().empty()) {
          paramMap.emplace(param->VpiName(), param);
        }
      }
    }

    // Collect func and task declaration
    if (defn->Task_funcs()) {
      for (task_func* tf : *defn->Task_funcs()) {
        if (funcMap.find(tf->VpiName()) == funcMap.end()) {
          if (!tf->VpiName().empty()) {
            // Bind to overriden function in sub-class
            funcMap.emplace(tf->VpiName(), tf);
          }
        }
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

  // Push class defn context on the stack
  // Class context is going to be pushed in case of:
  //   - imbricated classes
  //   - inheriting classes (Through the extends relation)
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
  if (m_muteErrors == false) {
    elabClass_defn(object, nullptr);
  }
}

void Elaborator::elabClass_defn(const class_defn* object, vpiHandle handle) {
  if (!m_clone) return;
  class_defn* cl = (class_defn*)object;
//<CLASS_ELABORATOR_LISTENER>
}

void Elaborator::bindScheduledTaskFunc() {
  for (auto& call_prefix : m_scheduledTfCallBinding) {
    tf_call* call = call_prefix.first;
    const class_var* prefix = call_prefix.second;
    if (call->UhdmType() == UHDM_OBJECT_TYPE::uhdmfunc_call) {
      if (function* f =
              any_cast<function*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((func_call*)call)->Function(f);
      }
    } else if (call->UhdmType() == UHDM_OBJECT_TYPE::uhdmtask_call) {
      if (task* f = any_cast<task*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((task_call*)call)->Task(f);
      }
    } else if (call->UhdmType() == UHDM_OBJECT_TYPE::uhdmmethod_func_call) {
      if (function* f =
              any_cast<function*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((method_func_call*)call)->Function(f);
      }
    } else if (call->UhdmType() == UHDM_OBJECT_TYPE::uhdmmethod_task_call) {
      if (task* f = any_cast<task*>(bindTaskFunc(call->VpiName(), prefix))) {
        ((method_task_call*)call)->Task(f);
      }
    }
  }
  m_scheduledTfCallBinding.clear();
}

void Elaborator::leaveClass_defn(const class_defn* object, vpiHandle handle) {
  bindScheduledTaskFunc();
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterInterface_inst(const interface_inst* object,
                                     vpiHandle handle) {
  const std::string_view instName = object->VpiName();
  const std::string_view defName = object->VpiDefName();
  bool flatModule =
      instName.empty() && ((object->VpiParent() == 0) ||
                           ((object->VpiParent() != 0) &&
                            (object->VpiParent()->VpiType() != vpiModule)));
  // false when it is an interface in a hierachy tree
  if (m_debug)
    std::cout << "Module: " << defName << " (" << instName
              << ") Flat:" << flatModule << std::endl;

  if (flatModule) {
    // Flat list of module (unelaborated)
    m_flatComponentMap.emplace(object->VpiDefName(), object);
  } else {
    // Hierachical module list (elaborated)
    m_inHierarchy = true;

    // Collect instance elaborated nets
    ComponentMap netMap;
    if (object->Nets()) {
      for (net* net : *object->Nets()) {
        if (!net->VpiName().empty()) {
          netMap.emplace(net->VpiName(), net);
        }
      }
    }
    if (object->Array_nets()) {
      for (array_net* net : *object->Array_nets()) {
        if (!net->VpiName().empty()) {
          netMap.emplace(net->VpiName(), net);
        }
      }
    }

    if (object->Variables()) {
      for (variables* var : *object->Variables()) {
        if (!var->VpiName().empty()) {
          netMap.emplace(var->VpiName(), var);
        }
        if (var->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_var) {
          enum_var* evar = (enum_var*)var;
          if (const ref_typespec* rt = evar->Typespec()) {
            if (const enum_typespec* etps =
                    rt->Actual_typespec<enum_typespec>()) {
              for (auto c : *etps->Enum_consts()) {
                if (!c->VpiName().empty()) {
                  netMap.emplace(c->VpiName(), c);
                }
              }
            }
          }
        }
      }
    }

    if (object->Interfaces()) {
      for (interface_inst* inter : *object->Interfaces()) {
        if (!inter->VpiName().empty()) {
          netMap.emplace(inter->VpiName(), inter);
        }
      }
    }
    if (object->Interface_arrays()) {
      for (interface_array* inter : *object->Interface_arrays()) {
        for (instance* interf : *inter->Instances())
          if (!interf->VpiName().empty()) {
            netMap.emplace(interf->VpiName(), interf);
          }
      }
    }
    if (object->Named_events()) {
      for (named_event* var : *object->Named_events()) {
        if (!var->VpiName().empty()) {
          netMap.emplace(var->VpiName(), var);
        }
      }
    }

    // Collect instance parameters, defparams
    ComponentMap paramMap;
    if (object->Param_assigns()) {
      for (param_assign* passign : *object->Param_assigns()) {
        if (!passign->Lhs()->VpiName().empty()) {
          paramMap.emplace(passign->Lhs()->VpiName(), passign->Rhs());
        }
      }
    }
    if (object->Parameters()) {
      for (any* param : *object->Parameters()) {
        if (!param->VpiName().empty()) {
          ComponentMap::iterator itr = paramMap.find(param->VpiName());
          if ((itr != paramMap.end()) && ((*itr).second == nullptr)) {
            paramMap.erase(itr);
          }
          paramMap.emplace(param->VpiName(), param);
        }
      }
    }

    if (object->Ports()) {
      for (ports* port : *object->Ports()) {
        if (!port->VpiName().empty()) {
          if (const ref_obj* ro = port->Low_conn<ref_obj>()) {
            if (const any* actual = ro->Actual_group()) {
              if (actual->UhdmType() == UHDM_OBJECT_TYPE::uhdminterface_inst) {
                netMap.emplace(port->VpiName(), actual);
              } else if (actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmmodport) {
                // If the interface of the modport is not yet in the map
                netMap.emplace(port->VpiName(), actual);
              }
            }
          }
        }
      }
    }

    // Collect func and task declaration
    ComponentMap funcMap;
    if (object->Task_funcs()) {
      for (task_func* var : *object->Task_funcs()) {
        if (!var->VpiName().empty()) {
          funcMap.emplace(var->VpiName(), var);
        }
      }
    }

    // Check if Module instance has a definition, collect enums
    ComponentMap::iterator itrDef = m_flatComponentMap.find(defName);
    if (itrDef != m_flatComponentMap.end()) {
      const BaseClass* comp = (*itrDef).second;
      int32_t compType = comp->VpiType();
      switch (compType) {
        case vpiModule: {
          module_inst* defMod = (module_inst*)comp;
          if (defMod->Typespecs()) {
            for (typespec* tps : *defMod->Typespecs()) {
              if (tps->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_typespec) {
                enum_typespec* etps = (enum_typespec*)tps;
                for (enum_const* econst : *etps->Enum_consts()) {
                  if (!econst->VpiName().empty()) {
                    paramMap.emplace(econst->VpiName(), econst);
                  }
                }
              }
            }
          }
        }
      }
    }

    // Collect gen_scope
    if (object->Gen_scope_arrays()) {
      for (gen_scope_array* gsa : *object->Gen_scope_arrays()) {
        if (!gsa->VpiName().empty()) {
          for (gen_scope* gs : *gsa->Gen_scopes()) {
            netMap.emplace(gsa->VpiName(), gs);
          }
        }
      }
    }
    ComponentMap modMap;

    if (const clocking_block* block = object->Default_clocking()) {
      if (!block->VpiName().empty()) {
        modMap.emplace(block->VpiName(), block);
      }
    }

    if (const clocking_block* block = object->Global_clocking()) {
      if (!block->VpiName().empty()) {
        modMap.emplace(block->VpiName(), block);
      }
    }

    if (object->Clocking_blocks()) {
      for (clocking_block* block : *object->Clocking_blocks()) {
        if (!block->VpiName().empty()) {
          modMap.emplace(block->VpiName(), block);
        }
      }
    }

    // Push instance context on the stack
    m_instStack.emplace_back(object, netMap, paramMap, funcMap, modMap);

    // Check if Module instance has a definition
    if (itrDef != m_flatComponentMap.end()) {
      const BaseClass* comp = (*itrDef).second;
      int32_t compType = comp->VpiType();
      switch (compType) {
        case vpiInterface: {
          //  interface* defMod = (interface*)comp;
          if (m_clone) {
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
}

void Elaborator::leaveInterface_inst(const interface_inst* object,
                                     vpiHandle handle) {
  bindScheduledTaskFunc();
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

// Hardcoded implementations

any* Elaborator::bindNet(std::string_view name) const {
  if (name.empty()) return nullptr;
  for (InstStack::const_reverse_iterator i = m_instStack.rbegin();
       i != m_instStack.rend(); ++i) {
    if (m_ignoreLastInstance) {
      if (i == m_instStack.rbegin()) continue;
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
  if (name.empty()) return nullptr;
  for (InstStack::const_reverse_iterator i = m_instStack.rbegin();
       i != m_instStack.rend(); ++i) {
    if (m_ignoreLastInstance) {
      if (i == m_instStack.rbegin()) continue;
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
  if (name.empty()) return nullptr;
  for (InstStack::const_reverse_iterator i = m_instStack.rbegin();
       i != m_instStack.rend(); ++i) {
    if (m_ignoreLastInstance) {
      if (i == m_instStack.rbegin()) continue;
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
  if (name.empty()) return nullptr;
  for (InstStack::const_reverse_iterator i = m_instStack.rbegin();
       i != m_instStack.rend(); ++i) {
    if (m_ignoreLastInstance) {
      if (i == m_instStack.rbegin()) continue;
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
    if (const ref_typespec* rt = prefix->Typespec()) {
      if (const class_typespec* tps = rt->Actual_typespec<class_typespec>()) {
        const class_defn* defn = tps->Class_defn();
        while (defn) {
          if (defn->Task_funcs()) {
            for (task_func* tf : *defn->Task_funcs()) {
              if (tf->VpiName() == name) return tf;
            }
          }

          const class_defn* base_defn = nullptr;
          if (const extends* ext = defn->Extends()) {
            if (const ref_typespec* ctps_rt = ext->Class_typespec()) {
              if (const class_typespec* ctps =
                      ctps_rt->Actual_typespec<class_typespec>()) {
                base_defn = ctps->Class_defn();
              }
            }
          }
          defn = base_defn;
        }
      }
    }
  }
  return nullptr;
}

bool Elaborator::isFunctionCall(std::string_view name,
                                        const expr* prefix) const {
  for (InstStack::const_reverse_iterator i = m_instStack.rbegin();
       i != m_instStack.rend(); ++i) {
    const ComponentMap& funcMap = std::get<3>(*i);
    ComponentMap::const_iterator funcItr = funcMap.find(name);
    if (funcItr != funcMap.end()) {
      return (funcItr->second->UhdmType() == UHDM_OBJECT_TYPE::uhdmfunction);
    }
  }
  if (prefix) {
    if (const ref_obj* ref = any_cast<const ref_obj*>(prefix)) {
      if (const class_var* vprefix = ref->Actual_group<class_var>()) {
        if (const any* func = bindTaskFunc(name, vprefix)) {
          return (func->UhdmType() == UHDM_OBJECT_TYPE::uhdmfunction);
        }
      }
    }
  }
  return true;
}

bool Elaborator::isTaskCall(std::string_view name,
                                    const expr* prefix) const {
  for (InstStack::const_reverse_iterator i = m_instStack.rbegin();
       i != m_instStack.rend(); ++i) {
    const ComponentMap& funcMap = std::get<3>(*i);
    ComponentMap::const_iterator funcItr = funcMap.find(name);
    if (funcItr != funcMap.end()) {
      return (funcItr->second->UhdmType() == UHDM_OBJECT_TYPE::uhdmtask);
    }
  }
  if (prefix) {
    if (const ref_obj* ref = any_cast<const ref_obj*>(prefix)) {
      if (const class_var* vprefix = ref->Actual_group<class_var>()) {
        if (const any* task = bindTaskFunc(name, vprefix)) {
          return (task->UhdmType() == UHDM_OBJECT_TYPE::uhdmtask);
        }
      }
    }
  }
  return true;
}

void Elaborator::enterTask_func(const task_func* object, vpiHandle handle) {
  // Collect instance elaborated nets
  ComponentMap varMap;
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }
  if (object->Io_decls()) {
    for (io_decl* decl : *object->Io_decls()) {
      if (!decl->VpiName().empty()) {
        varMap.emplace(decl->VpiName(), decl);
      }
    }
  }
  if (!object->VpiName().empty()) {
    varMap.emplace(object->VpiName(), object->Return());
  }

  if (const any* parent = object->VpiParent()) {
    if (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmclass_defn) {
      const class_defn* defn = (const class_defn*)parent;
      while (defn) {
        if (defn->Variables()) {
          for (any* var : *defn->Variables()) {
            if (!var->VpiName().empty()) {
              varMap.emplace(var->VpiName(), var);
            }
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

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void Elaborator::leaveTask_func(const task_func* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterFor_stmt(const for_stmt* object, vpiHandle handle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }
  if (object->VpiForInitStmts()) {
    for (any* stmt : *object->VpiForInitStmts()) {
      if (stmt->UhdmType() == UHDM_OBJECT_TYPE::uhdmassignment) {
        assignment* astmt = (assignment*)stmt;
        const any* lhs = astmt->Lhs();
        if (lhs->UhdmType() != UHDM_OBJECT_TYPE::uhdmref_var) {
          if (!lhs->VpiName().empty()) {
            varMap.emplace(lhs->VpiName(), lhs);
          }
        }
      }
    }
  }
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void Elaborator::leaveFor_stmt(const for_stmt* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterForeach_stmt(const foreach_stmt* object,
                                   vpiHandle handle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }
  if (object->VpiLoopVars()) {
    for (any* var : *object->VpiLoopVars()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void Elaborator::leaveForeach_stmt(const foreach_stmt* object,
                                   vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterBegin(const begin* object, vpiHandle handle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void Elaborator::leaveBegin(const begin* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterNamed_begin(const named_begin* object, vpiHandle handle) {
  ComponentMap varMap;
  if (!m_instStack.empty()) {
    ComponentMap& modMap = std::get<4>(m_instStack.back());
    if (!object->VpiName().empty()) {
      modMap.emplace(object->VpiName(), object);
    }
  }
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void Elaborator::leaveNamed_begin(const named_begin* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterFork_stmt(const fork_stmt* object, vpiHandle handle) {
  ComponentMap varMap;
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void Elaborator::leaveFork_stmt(const fork_stmt* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterNamed_fork(const named_fork* object, vpiHandle handle) {
  ComponentMap varMap;
  if (!m_instStack.empty()) {
    ComponentMap& modMap = std::get<4>(m_instStack.back());
    if (!object->VpiName().empty()) {
      modMap.emplace(object->VpiName(), object);
    }
  }
  if (object->Array_vars()) {
    for (variables* var : *object->Array_vars()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }
  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      if (!var->VpiName().empty()) {
        varMap.emplace(var->VpiName(), var);
      }
    }
  }

  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  m_instStack.emplace_back(object, varMap, paramMap, funcMap, modMap);
}

void Elaborator::leaveNamed_fork(const named_fork* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterFunction(const function* object, vpiHandle handle) {
  enterTask_func(object, handle);
}

void Elaborator::leaveFunction(const function* object, vpiHandle handle) {
  leaveTask_func(object, handle);
}

void Elaborator::enterTask(const task* object, vpiHandle handle) {
  enterTask_func(object, handle);
}

void Elaborator::leaveTask(const task* object, vpiHandle handle) {
  leaveTask_func(object, handle);
}

void Elaborator::enterGen_scope(const gen_scope* object, vpiHandle handle) {
  // Collect instance elaborated nets

  ComponentMap netMap;
  if (object->Nets()) {
    for (net* net : *object->Nets()) {
      if (!net->VpiName().empty()) {
        netMap.emplace(net->VpiName(), net);
      }
    }
  }
  if (object->Array_nets()) {
    for (array_net* net : *object->Array_nets()) {
      if (!net->VpiName().empty()) {
        netMap.emplace(net->VpiName(), net);
      }
    }
  }

  if (object->Variables()) {
    for (variables* var : *object->Variables()) {
      if (!var->VpiName().empty()) {
        netMap.emplace(var->VpiName(), var);
      }
      if (var->UhdmType() == UHDM_OBJECT_TYPE::uhdmenum_var) {
        enum_var* evar = (enum_var*)var;
        if (const ref_typespec* rt = evar->Typespec()) {
          if (const enum_typespec* etps = rt->Typespec<enum_typespec>()) {
            for (auto c : *etps->Enum_consts()) {
              if (!c->VpiName().empty()) {
                netMap.emplace(c->VpiName(), c);
              }
            }
          }
        }
      }
    }
  }

  if (object->Interfaces()) {
    for (interface_inst* inter : *object->Interfaces()) {
      if (!inter->VpiName().empty()) {
        netMap.emplace(inter->VpiName(), inter);
      }
    }
  }
  if (object->Interface_arrays()) {
    for (interface_array* inter : *object->Interface_arrays()) {
      if (VectorOfinstance* instances = inter->Instances()) {
        for (instance* interf : *instances) {
          if (!interf->VpiName().empty()) {
            netMap.emplace(interf->VpiName(), interf);
          }
        }
      }
    }
  }

  // Collect instance parameters, defparams
  ComponentMap paramMap;
  if (object->Parameters()) {
    for (any* param : *object->Parameters()) {
      if (!param->VpiName().empty()) {
        paramMap.emplace(param->VpiName(), param);
      }
    }
  }
  if (object->Def_params()) {
    for (def_param* param : *object->Def_params()) {
      if (!param->VpiName().empty()) {
        paramMap.emplace(param->VpiName(), param);
      }
    }
  }

  ComponentMap funcMap;
  ComponentMap modMap;

  if (object->Modules()) {
    for (module_inst* mod : *object->Modules()) {
      if (!mod->VpiName().empty()) {
        modMap.emplace(mod->VpiName(), mod);
      }
    }
  }

  if (object->Module_arrays()) {
    for (module_array* mod : *object->Module_arrays()) {
      if (!mod->VpiName().empty()) {
        modMap.emplace(mod->VpiName(), mod);
      }
    }
  }

  // Collect gen_scope
  if (object->Gen_scope_arrays()) {
    for (gen_scope_array* gsa : *object->Gen_scope_arrays()) {
      if (!gsa->VpiName().empty()) {
        for (gen_scope* gs : *gsa->Gen_scopes()) {
          modMap.emplace(gsa->VpiName(), gs);
        }
      }
    }
  }
  m_instStack.emplace_back(object, netMap, paramMap, funcMap, modMap);
}

void Elaborator::leaveGen_scope(const gen_scope* object, vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::pushVar(any* var) {
  ComponentMap netMap;
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  if (!var->VpiName().empty()) {
    netMap.emplace(var->VpiName(), var);
  }
  m_instStack.emplace_back(var, netMap, paramMap, funcMap, modMap);
}

void Elaborator::popVar(any* var) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == var)) {
    m_instStack.pop_back();
  }
}

void Elaborator::enterMethod_func_call(const method_func_call* object,
                                       vpiHandle handle) {
  ComponentMap netMap;
  ComponentMap paramMap;
  ComponentMap funcMap;
  ComponentMap modMap;
  if (object->Tf_call_args()) {
    for (auto arg : *object->Tf_call_args()) {
      if (!arg->VpiName().empty()) {
        netMap.emplace(arg->VpiName(), arg);
      }
    }
  }
  m_instStack.emplace_back(object, netMap, paramMap, funcMap, modMap);
}

void Elaborator::leaveMethod_func_call(const method_func_call* object,
                                       vpiHandle handle) {
  if (!m_instStack.empty() && (std::get<0>(m_instStack.back()) == object)) {
    m_instStack.pop_back();
  }
}

void Elaborator::leaveRef_obj(const ref_obj* object, vpiHandle handle) {
  const any* actual = (any_cast<ref_obj*>(object))->Actual_group();
  const any* parent = object->VpiParent();
  // Last call binding happens here.
  // Logic net are the default binding (When no proper binding was found).
  // Hier path binding leaf node is more accurately done in the clone_tree operation
  // because of variable name scope shadowing issues. 
  if ((!actual) || (actual && parent && parent->UhdmType() != uhdmhier_path)) {
    if (any* res = bindAny(object->VpiName())) {
      ((ref_obj*)object)->Actual_group(res);
    }
  }
}

void Elaborator::leaveBit_select(const bit_select* object, vpiHandle handle) {
  leaveRef_obj(object, handle);
}

void Elaborator::leaveIndexed_part_select(const indexed_part_select* object,
                                          vpiHandle handle) {
  leaveRef_obj(object, handle);
}

void Elaborator::leavePart_select(const part_select* object, vpiHandle handle) {
  leaveRef_obj(object, handle);
}

void Elaborator::leaveVar_select(const var_select* object, vpiHandle handle) {
  leaveRef_obj(object, handle);
}

template <typename T>
inline std::vector<T*>* Elaborator::Clone(const std::vector<T*>* source) {
  if ((source == nullptr) || source->empty()) return nullptr;
  std::vector<T*>* const target = m_serializer->MakeVec<T>();
  target->insert(target->end(), source->cbegin(), source->cend());
  return target;
}

template <typename T>
inline std::vector<T*>* Elaborator::DeepClone(const std::vector<T*>* source,
                                              any* parent) {
  if ((source == nullptr) || source->empty()) return nullptr;
  std::vector<T*>* const target = m_serializer->MakeVec<T>();
  target->reserve(source->size());
  for (const T* any : *source) {
    target->emplace_back(clone(any, parent));
  }
  return target;
}

// clang-format off
void Elaborator::DeepCopy(const any* source, any* target) {}

//<COPY_ANY_IMPLEMENTATIONS>
// clang-format on

template <typename T>
inline T *Elaborator::DeepClone(const T *source, any *parent) {
  if (source == nullptr) return nullptr;
  T* const target = m_serializer->Clone(source);
  target->VpiParent(parent);
  DeepCopy(source, target);
  return target;
}

any* Elaborator::DeepCloneAny(const any* source, any* parent) {
  if (source == nullptr) return nullptr;

  switch (source->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmarray_net:
    case UHDM_OBJECT_TYPE::uhdmenum_net:
    case UHDM_OBJECT_TYPE::uhdminteger_net:
    case UHDM_OBJECT_TYPE::uhdmlogic_net:
    case UHDM_OBJECT_TYPE::uhdmnet_bit:
    case UHDM_OBJECT_TYPE::uhdmpacked_array_net:
    case UHDM_OBJECT_TYPE::uhdmstruct_net:
    case UHDM_OBJECT_TYPE::uhdmtime_net: {
      if (any* const target = bindNet(source->VpiName())) {
        return target;
      }
    } break;
    case UHDM_OBJECT_TYPE::uhdmparameter:
    case UHDM_OBJECT_TYPE::uhdmtype_parameter: {
      if (any* const target = bindParam(source->VpiName())) {
        // TODO(HS): BAD HACK!!!
        target->VpiParent(parent);
        if (parameter* const targetParameter = any_cast<parameter>(target)) {
          const uint32_t id = target->UhdmId();
          *targetParameter = *static_cast<const parameter*>(source);
          target->UhdmId(id);
          DeepCopy(static_cast<const parameter*>(source), targetParameter);
        } else if (type_parameter* const targetTypeParameter =
                       any_cast<type_parameter>(target)) {
          const uint32_t id = target->UhdmId();
          *targetTypeParameter = *static_cast<const type_parameter*>(source);
          target->UhdmId(id);
          DeepCopy(static_cast<const type_parameter*>(source),
                   targetTypeParameter);
        }
        return target;
      }
    } break;
    case UHDM_OBJECT_TYPE::uhdmbegin: {
      enterBegin(static_cast<const begin*>(source), nullptr);
    } break;
    case UHDM_OBJECT_TYPE::uhdmnamed_begin: {
      enterNamed_begin(static_cast<const named_begin*>(source), nullptr);
    } break;
    case UHDM_OBJECT_TYPE::uhdmfork_stmt: {
      enterFork_stmt(static_cast<const fork_stmt*>(source), nullptr);
    } break;
    case UHDM_OBJECT_TYPE::uhdmnamed_fork: {
      enterNamed_fork(static_cast<const named_fork*>(source), nullptr);
    } break;
    default:
      break;
  }

  any* target = nullptr;
  // clang-format off
  switch (source->UhdmType()) {
//<CLONE_CASE_STATEMENTS>
    default: break;
  }
  // clang-format on

  switch (source->UhdmType()) {
    case UHDM_OBJECT_TYPE::uhdmbegin: {
      leaveBegin(static_cast<const begin*>(source), nullptr);
    } break;
    case UHDM_OBJECT_TYPE::uhdmnamed_begin: {
      leaveNamed_begin(static_cast<const named_begin*>(source), nullptr);
    } break;
    case UHDM_OBJECT_TYPE::uhdmfork_stmt: {
      leaveFork_stmt(static_cast<const fork_stmt*>(source), nullptr);
    } break;
    case UHDM_OBJECT_TYPE::uhdmnamed_fork: {
      leaveNamed_fork(static_cast<const named_fork*>(source), nullptr);
    } break;
    default:
      break;
  }

  return target;
}

sys_func_call* Elaborator::DeepClone(const sys_func_call* source, any* parent) {
  sys_func_call* const target = m_serializer->Clone<sys_func_call>(source);
  target->VpiParent(parent);
  if (auto obj = source->User_systf())
    target->User_systf(clone(obj, target));
  if (auto obj = source->Scope()) target->Scope(clone(obj, target));
  if (auto vec = source->Tf_call_args())
    target->Tf_call_args(DeepClone(vec, target));
  if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
  return target;
}

sys_task_call* Elaborator::DeepClone(const sys_task_call* source, any* parent) {
  sys_task_call* const target = m_serializer->Clone<sys_task_call>(source);
  target->VpiParent(parent);
  if (auto obj = source->User_systf()) target->User_systf(clone(obj, target));
  if (auto obj = source->Scope()) target->Scope(clone(obj, target));
  if (auto vec = source->Tf_call_args())
    target->Tf_call_args(DeepClone(vec, target));
  if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
  return target;
}

tf_call* Elaborator::DeepClone(const method_func_call* source,
                                     any* parent) {
  const expr* prefix = source->Prefix();
  if (prefix) {
    prefix = clone(prefix, const_cast<method_func_call*>(source));
  }
  bool is_function = isFunctionCall(source->VpiName(), prefix);
  if (is_function) {
    method_func_call *const target = m_serializer->Clone<method_func_call>(source);
    target->VpiParent(parent);
    if (auto obj = source->Prefix()) target->Prefix(clone(obj, target));
    const any* parent = target->VpiParent();
    const ref_obj* ref = any_cast<const ref_obj*>(target->Prefix());
    const class_var* varprefix = nullptr;
    if (ref) varprefix = any_cast<const class_var*>(ref->Actual_group());
    scheduleTaskFuncBinding(target, varprefix);
    any* pushedVar = nullptr;
    if (auto vec = source->Tf_call_args()) {
      auto clone_vec = m_serializer->MakeAnyVec();
      target->Tf_call_args(clone_vec);
      for (auto obj : *vec) {
        any* arg = clone(obj, target);
        // CB callbacks_to_append[$];
        // unique_callbacks_to_append = callbacks_to_append.unique( cb_ )
        // with ( cb_.get_inst_id );
        if (parent->UhdmType() == UHDM_OBJECT_TYPE::uhdmhier_path) {
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
                  if (actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var) {
                    array_var* arr = (array_var*)actual;
                    if (arr->Variables() && !arr->Variables()->empty()) {
                      variables* var = arr->Variables()->front();
                      if (variables* varclone = clone(var, obj->VpiParent())) {
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
        clone_vec->push_back(arg);
      }
    }
    if (auto obj = source->With()) target->With(clone(obj, target));
    if (pushedVar) popVar(pushedVar);
    if (auto obj = source->Scope()) target->Scope(clone(obj, target));
    if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
    return target;
  } else {
    method_task_call *const target = m_serializer->MakeMethod_task_call();
    target->VpiName(source->VpiName());
    target->Tf_call_args(source->Tf_call_args());
    target->VpiParent(parent);
    target->VpiFile(source->VpiFile());
    target->VpiLineNo(source->VpiLineNo());
    target->VpiColumnNo(source->VpiColumnNo());
    target->VpiEndLineNo(source->VpiEndLineNo());
    target->VpiEndColumnNo(source->VpiEndColumnNo());
    if (auto obj = source->Prefix()) target->Prefix(clone(obj, target));
    const ref_obj* ref = any_cast<const ref_obj*>(target->Prefix());
    const class_var* varprefix = nullptr;
    if (ref) varprefix = any_cast<const class_var*>(ref->Actual_group());
    scheduleTaskFuncBinding(target, varprefix);
    if (auto obj = source->With()) target->With(clone(obj, target));
    if (auto obj = source->Scope()) target->Scope(clone(obj, target));
    if (auto vec = source->Tf_call_args())
      target->Tf_call_args(DeepClone(vec, target));
    if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
    return target;
  }  
}

constant* Elaborator::DeepClone(const constant* source, any* parent) {
  if (uniquifyTypespec() || (source->VpiSize() == -1)) {
    constant* const target = m_serializer->Clone<constant>(source);
    target->VpiParent(parent);
    if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
    return target;
  } else {
    return const_cast<constant*>(source);
  }
}

tagged_pattern* Elaborator::DeepClone(const tagged_pattern* source,
                                      any* parent) {
  if (uniquifyTypespec()) {
    tagged_pattern* const target = m_serializer->Clone<tagged_pattern>(source);
    target->VpiParent(parent);
    if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
    if (auto obj = source->Pattern()) target->Pattern(clone(obj, target));
    return target;
  } else {
    return const_cast<tagged_pattern*>(source);
  }
}

tf_call* Elaborator::DeepClone(const method_task_call* source,
                                      any* parent) {
  const expr* prefix = source->Prefix();
  if (prefix) {
    prefix = clone(prefix, const_cast<method_task_call*>(source));
  }
  bool is_task = isTaskCall(source->VpiName(), prefix);
  if (is_task) {
    method_task_call* const target =
        m_serializer->Clone<method_task_call>(source);
    target->VpiParent(parent);
    if (auto obj = source->Prefix()) target->Prefix(clone(obj, target));
    const ref_obj* ref = any_cast<const ref_obj*>(target->Prefix());
    const class_var* varprefix = nullptr;
    if (ref) varprefix = any_cast<const class_var*>(ref->Actual_group());
    scheduleTaskFuncBinding(target, varprefix);
    if (auto obj = source->With()) target->With(clone(obj, target));
    if (auto obj = source->Scope()) target->Scope(clone(obj, target));
    if (auto vec = source->Tf_call_args())
      target->Tf_call_args(DeepClone(vec, target));
    if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
    return target;
  } else {
    method_func_call* const target = m_serializer->MakeMethod_func_call();
    target->VpiName(source->VpiName());
    target->Tf_call_args(source->Tf_call_args());
    target->VpiParent(parent);
    target->VpiFile(source->VpiFile());
    target->VpiLineNo(source->VpiLineNo());
    target->VpiColumnNo(source->VpiColumnNo());
    target->VpiEndLineNo(source->VpiEndLineNo());
    target->VpiEndColumnNo(source->VpiEndColumnNo());
    if (auto obj = source->Prefix()) target->Prefix(clone(obj, target));
    const ref_obj* ref = any_cast<const ref_obj*>(target->Prefix());
    const class_var* varprefix = nullptr;
    if (ref) varprefix = any_cast<const class_var*>(ref->Actual_group());
    scheduleTaskFuncBinding(target, varprefix);
    if (auto obj = source->With()) target->With(clone(obj, target));
    if (auto obj = source->Scope()) target->Scope(clone(obj, target));
    if (auto vec = source->Tf_call_args())
      target->Tf_call_args(DeepClone(vec, target));
    if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
    return target;
  }
}

tf_call* Elaborator::DeepClone(const func_call* source, any* parent) {
  bool is_function = isFunctionCall(source->VpiName(), nullptr);
  if (is_function) {
    func_call* const target = m_serializer->Clone<func_call>(source);
    target->VpiParent(parent);
    scheduleTaskFuncBinding(target, nullptr);
    if (auto obj = source->Scope()) target->Scope(clone(obj, target));
    if (auto vec = source->Tf_call_args())
      target->Tf_call_args(DeepClone(vec, target));
    if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
    return target;
  } else {
    task_call* const target = m_serializer->MakeTask_call();
    target->VpiName(source->VpiName());
    target->Tf_call_args(source->Tf_call_args());
    target->VpiParent(parent);
    target->VpiFile(source->VpiFile());
    target->VpiLineNo(source->VpiLineNo());
    target->VpiColumnNo(source->VpiColumnNo());
    target->VpiEndLineNo(source->VpiEndLineNo());
    target->VpiEndColumnNo(source->VpiEndColumnNo());
    scheduleTaskFuncBinding(target, nullptr);
    if (auto obj = source->Scope()) target->Scope(clone(obj, target));
    if (auto vec = source->Tf_call_args())
      target->Tf_call_args(DeepClone(vec, target));
    if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
    return target;
  }
}

tf_call* Elaborator::DeepClone(const task_call* source, any* parent) {
  bool is_task = isTaskCall(source->VpiName(), nullptr);
  if (is_task) {
    task_call* const target = m_serializer->Clone<task_call>(source);
    target->VpiParent(parent);
    scheduleTaskFuncBinding(target, nullptr);
    if (auto obj = source->Scope()) target->Scope(clone(obj, target));
    if (auto vec = source->Tf_call_args())
      target->Tf_call_args(DeepClone(vec, target));
    if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
    return target;
  } else {
    func_call* const target = m_serializer->MakeFunc_call();
    target->VpiName(source->VpiName());
    target->VpiFile(source->VpiFile());
    target->VpiLineNo(source->VpiLineNo());
    target->VpiColumnNo(source->VpiColumnNo());
    target->VpiEndLineNo(source->VpiEndLineNo());
    target->VpiEndColumnNo(source->VpiEndColumnNo());
    target->Tf_call_args(source->Tf_call_args());
    target->VpiParent(parent);
    scheduleTaskFuncBinding(target, nullptr);
    if (auto obj = source->Scope()) target->Scope(clone(obj, target));
    if (auto vec = source->Tf_call_args())
      target->Tf_call_args(DeepClone(vec, target));
    if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
    return target;
  }
}

gen_scope_array* Elaborator::DeepClone(const gen_scope_array* source,
                                       any* parent) {
  gen_scope_array* const target = m_serializer->Clone<gen_scope_array>(source);
  target->VpiParent(parent);
  if (auto obj = source->Gen_var()) target->Gen_var(clone(obj, target));
  if (auto vec = source->Gen_scopes()) {
    auto clone_vec = m_serializer->MakeGen_scopeVec();
    target->Gen_scopes(clone_vec);
    for (auto obj : *vec) {
      enterGen_scope(obj, nullptr);
      clone_vec->emplace_back(clone(obj, target));
      leaveGen_scope(obj, nullptr);
    }
  }
  if (auto obj = source->VpiInstance()) target->VpiInstance(clone(obj, target));
  return target;
}

function* Elaborator::DeepClone(const function* source, any* parent) {
  function* const target = m_serializer->Clone<function>(source);
  target->VpiParent(parent);
  if (auto obj = source->Left_range()) target->Left_range(clone(obj, target));
  if (auto obj = source->Right_range()) target->Right_range(clone(obj, target));
  if (auto obj = source->Return()) target->Return((variables*)obj);
  if (auto obj = source->Instance()) target->Instance((instance*)obj);
  if (instance* inst = any_cast<instance*>(parent)) target->Instance(inst);
  if (auto obj = source->Class_defn()) target->Class_defn(clone(obj, target));
  if (auto vec = source->Io_decls()) target->Io_decls(DeepClone(vec, target));
  if (auto vec = source->Variables()) target->Variables(DeepClone(vec, target));
  if (auto vec = source->Parameters())
    target->Parameters(DeepClone(vec, target));
  if (auto vec = source->Scopes()) target->Scopes(DeepClone(vec, target));
  if (auto vec = source->Typespecs()) {
    auto clone_vec = m_serializer->MakeTypespecVec();
    target->Typespecs(clone_vec);
    for (auto obj : *vec) {
      if (uniquifyTypespec()) {
        clone_vec->push_back(clone(obj, target));
      } else {
        clone_vec->push_back(obj);
      }
    }
  }
  enterTask_func(target, nullptr);
  if (auto vec = source->Concurrent_assertions())
    target->Concurrent_assertions(DeepClone(vec, target));
  if (auto vec = source->Property_decls())
    target->Property_decls(DeepClone(vec, target));
  if (auto vec = source->Sequence_decls())
    target->Sequence_decls(DeepClone(vec, target));
  if (auto vec = source->Named_events())
    target->Named_events(DeepClone(vec, target));
  if (auto vec = source->Named_event_arrays())
    target->Named_event_arrays(DeepClone(vec, target));
  if (auto vec = source->Virtual_interface_vars())
    target->Virtual_interface_vars(DeepClone(vec, target));
  if (auto vec = source->Logic_vars())
    target->Logic_vars(DeepClone(vec, target));
  if (auto vec = source->Array_vars())
    target->Array_vars(DeepClone(vec, target));
  if (auto vec = source->Array_var_mems())
    target->Array_var_mems(DeepClone(vec, target));
  if (auto vec = source->Param_assigns())
    target->Param_assigns(DeepClone(vec, target));
  if (auto vec = source->Let_decls()) target->Let_decls(DeepClone(vec, target));
  if (auto vec = source->Attributes())
    target->Attributes(DeepClone(vec, target));
  if (auto vec = source->Instance_items())
    target->Instance_items(DeepClone(vec, target));
  if (auto obj = source->Stmt()) target->Stmt(clone(obj, target));
  leaveTask_func(target, nullptr);
  return target;
}

task* Elaborator::DeepClone(const task* source, any* parent) {
  task* const target = m_serializer->Clone<task>(source);
  target->VpiParent(parent);
  if (auto obj = source->Left_range()) target->Left_range(clone(obj, target));
  if (auto obj = source->Right_range()) target->Right_range(clone(obj, target));
  if (auto obj = source->Return()) target->Return(clone(obj, target));
  if (auto obj = source->Instance()) target->Instance((instance*)obj);
  if (instance* inst = any_cast<instance*>(parent)) target->Instance(inst);
  if (auto obj = source->Class_defn()) target->Class_defn(clone(obj, target));
  if (auto vec = source->Io_decls()) target->Io_decls(DeepClone(vec, target));
  if (auto vec = source->Variables()) target->Variables(DeepClone(vec, target));
  if (auto vec = source->Scopes()) target->Scopes(DeepClone(vec, target));
  if (auto vec = source->Typespecs()) {
    auto clone_vec = m_serializer->MakeTypespecVec();
    target->Typespecs(clone_vec);
    for (auto obj : *vec) {
      if (uniquifyTypespec()) {
        clone_vec->push_back(clone(obj, target));
      } else {
        clone_vec->push_back(obj);
      }
    }
  }
  enterTask_func(target, nullptr);
  if (auto vec = source->Concurrent_assertions())
    target->Concurrent_assertions(DeepClone(vec, target));
  if (auto vec = source->Property_decls())
    target->Property_decls(DeepClone(vec, target));
  if (auto vec = source->Sequence_decls())
    target->Sequence_decls(DeepClone(vec, target));
  if (auto vec = source->Named_events())
    target->Named_events(DeepClone(vec, target));
  if (auto vec = source->Named_event_arrays())
    target->Named_event_arrays(DeepClone(vec, target));
  if (auto vec = source->Virtual_interface_vars())
    target->Virtual_interface_vars(DeepClone(vec, target));
  if (auto vec = source->Logic_vars())
    target->Logic_vars(DeepClone(vec, target));
  if (auto vec = source->Array_vars())
    target->Array_vars(DeepClone(vec, target));
  if (auto vec = source->Array_var_mems())
    target->Array_var_mems(DeepClone(vec, target));
  if (auto vec = source->Param_assigns())
    target->Param_assigns(DeepClone(vec, target));
  if (auto vec = source->Let_decls()) target->Let_decls(DeepClone(vec, target));
  if (auto vec = source->Attributes())
    target->Attributes(DeepClone(vec, target));
  if (auto vec = source->Parameters())
    target->Parameters(DeepClone(vec, target));
  if (auto vec = source->Instance_items())
    target->Instance_items(DeepClone(vec, target));
  if (auto obj = source->Stmt()) target->Stmt(clone(obj, target));
  leaveTask_func(target, nullptr);
  return target;
}

cont_assign* Elaborator::DeepClone(const cont_assign* source, any* parent) {
  cont_assign* const target = m_serializer->Clone<cont_assign>(source);
  target->VpiParent(parent);
  if (auto obj = source->Delay()) target->Delay(clone(obj, target));
  expr* lhs = nullptr;
  if (auto obj = source->Lhs()) {
    lhs = clone(obj, target);
    if (lhs->UhdmType() == uhdmhier_path) {
      hier_path* path = (hier_path*)lhs;
      any* last = path->Path_elems()->back();
      if (ref_obj* ro = any_cast<ref_obj*>(last)) {
        if (net* n = any_cast<net*>(ro->Actual_group())) {
          // The net parent has to be the same as a current scope
          if (n->VpiParent() == parent) lhs = n;
        }
      }
    }
    target->Lhs(lhs);
  }
  if (auto obj = source->Rhs()) {
    expr* rhs = clone(obj, target);
    if (rhs->UhdmType() == uhdmhier_path) {
      hier_path* path = (hier_path*)rhs;
      any* last = path->Path_elems()->back();
      if (ref_obj* ro = any_cast<ref_obj*>(last)) {
        if (constant* c = any_cast<constant*>(ro->Actual_group())) {
          // The constant parrent's parent has to be the same as a current scope
          if (c->VpiParent()->VpiParent() == parent) rhs = c;
        }
      }
    }
    target->Rhs(rhs);
    if (ref_obj* ro = any_cast<ref_obj*>(lhs)) {
      if (struct_var* stv = ro->Actual_group<struct_var>()) {
        if (ref_typespec* rt = stv->Typespec()) {
          if (typespec* ts = rt->Actual_typespec()) {
            ExprEval eval(m_muteErrors);
            if (expr* res =
                    eval.flattenPatternAssignments(*m_serializer, ts, rhs)) {
              if (res->UhdmType() == UHDM_OBJECT_TYPE::uhdmoperation) {
                ((operation*)rhs)->Operands(((operation*)res)->Operands());
              }
            }
          }
        }
      }
    }
  }
  if (auto vec = source->Cont_assign_bits())
    target->Cont_assign_bits(DeepClone(vec, target));
  return target;
}

any* Elaborator::bindClassTypespec(class_typespec* ctps, any* current,
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
          } else if (current->UhdmType() ==
                     UHDM_OBJECT_TYPE::uhdmmethod_func_call) {
            if (tf->UhdmType() == UHDM_OBJECT_TYPE::uhdmfunction)
              ((method_func_call*)current)->Function((function*)tf);
          } else if (current->UhdmType() ==
                     UHDM_OBJECT_TYPE::uhdmmethod_task_call) {
            if (tf->UhdmType() == UHDM_OBJECT_TYPE::uhdmtask)
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

hier_path* Elaborator::DeepClone(const hier_path* source,
                                any* parent) {
  hier_path* const target = m_serializer->Clone<hier_path>(source);
  target->VpiParent(parent);
  if (auto vec = source->Path_elems()) {
    auto clone_vec = m_serializer->MakeAnyVec();
    target->Path_elems(clone_vec);
    any* previous = nullptr;
    for (auto obj : *vec) {
      any* current = clone(obj, target);
      clone_vec->push_back(current);
      bool found = false;
      if (ref_obj* ref = any_cast<ref_obj*>(current)) {
        if (current->VpiName() == "this") {
          const any* tmp = current;
          while (tmp) {
            if (tmp->UhdmType() == UHDM_OBJECT_TYPE::uhdmclass_defn) {
              ref->Actual_group((any*)tmp);
              found = true;
              break;
            }
            tmp = tmp->VpiParent();
          }
        } else if (current->VpiName() == "super") {
          const any* tmp = current;
          while (tmp) {
            if (tmp->UhdmType() == UHDM_OBJECT_TYPE::uhdmclass_defn) {
              class_defn* def = (class_defn*)tmp;
              if (const extends* ext = def->Extends()) {
                if (const ref_typespec* rt = ext->Class_typespec()) {
                  if (const class_typespec* ctps =
                          rt->Actual_typespec<class_typespec>()) {
                    ref->Actual_group((any*)ctps->Class_defn());
                    found = true;
                    break;
                  }
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
            if (previous->UhdmType() == UHDM_OBJECT_TYPE::uhdmbit_select) {
              bit_select* prev = (bit_select*)previous;
              ro->Actual_group((any*)prev->Actual_group());
              found = true;
            }
          }
        }
        std::string nameIndexed(name);
        if (obj->UhdmType() == UHDM_OBJECT_TYPE::uhdmbit_select) {
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
              case UHDM_OBJECT_TYPE::uhdmdesign: {
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
              case UHDM_OBJECT_TYPE::uhdmgen_scope: {
                gen_scope* scope = (gen_scope*)actual;
                if (obj->UhdmType() == UHDM_OBJECT_TYPE::uhdmmethod_func_call) {
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
                } else if (obj->UhdmType() ==
                           UHDM_OBJECT_TYPE::uhdmmethod_task_call) {
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
                          break;
                        }
                      }
                    }
                  }
                }
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmmodport: {
                modport* mp = (modport*)actual;
                if (mp->Io_decls()) {
                  for (io_decl* decl : *mp->Io_decls()) {
                    if (decl->VpiName() == name) {
                      found = true;
                      previous = decl;
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(decl);
                      }
                      break;
                    }
                  }
                }
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmnamed_event: {
                if (name == "triggered") {
                  // Builtin
                  found = true;
                }
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmarray_net: {
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
                  call->VpiParent(target);
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
              case UHDM_OBJECT_TYPE::uhdmarray_var:
              case UHDM_OBJECT_TYPE::uhdmpacked_array_var: {
                const typespec* tps = nullptr;
                if (actual_type == UHDM_OBJECT_TYPE::uhdmpacked_array_var) {
                  packed_array_var* avar = (packed_array_var*)actual;
                  if (VectorOfany* vars = avar->Elements()) {
                    if (!vars->empty()) {
                      actual = vars->front();
                      actual_type = actual->UhdmType();
                    }
                  }
                  if (const ref_typespec* rt = avar->Typespec()) {
                    tps = rt->Actual_typespec();
                    if (const packed_array_typespec* ptps =
                            rt->Actual_typespec<packed_array_typespec>()) {
                      if (const ref_typespec* ert = ptps->Elem_typespec()) {
                        tps = ert->Actual_typespec();
                      }
                    }
                  }
                } else {
                  array_var* avar = (array_var*)actual;
                  if (VectorOfvariables* vars = avar->Variables()) {
                    if (!vars->empty()) {
                      actual = vars->front();
                      actual_type = actual->UhdmType();
                    }
                  }
                  if (const ref_typespec* rt = avar->Typespec()) {
                    tps = rt->Actual_typespec();
                    if (const array_typespec* atps =
                            rt->Actual_typespec<array_typespec>()) {
                      if (const ref_typespec* ert = atps->Elem_typespec()) {
                        tps = ert->Actual_typespec();
                      }
                    }
                  }
                }
                if (name == "size" || name == "exists" || name == "find" ||
                    name == "max" || name == "min") {
                  func_call* call = m_serializer->MakeFunc_call();
                  call->VpiName(name);
                  call->VpiParent(target);
                  if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                    cro->Actual_group(call);
                  }
                  // Builtin method
                  found = true;
                  previous = (any*)call;
                }
                if (found == false) {
                  if (tps) {
                    UHDM_OBJECT_TYPE ttype = tps->UhdmType();
                    if (ttype == UHDM_OBJECT_TYPE::uhdmpacked_array_typespec) {
                      packed_array_typespec* ptps = (packed_array_typespec*)tps;
                      tps = (typespec*)ptps->Elem_typespec();
                      if (tps) ttype = tps->UhdmType();
                    } else if (ttype == UHDM_OBJECT_TYPE::uhdmarray_typespec) {
                      array_typespec* ptps = (array_typespec*)tps;
                      tps = (typespec*)ptps->Elem_typespec();
                      if (tps) ttype = tps->UhdmType();
                    }
                    if (ttype == UHDM_OBJECT_TYPE::uhdmstring_typespec) {
                      found = true;
                    } else if (ttype == UHDM_OBJECT_TYPE::uhdmclass_typespec) {
                      class_typespec* ctps = (class_typespec*)tps;
                      any* tmp = bindClassTypespec(ctps, current, name, found);
                      if (found) {
                        previous = tmp;
                      }
                    } else if (ttype == UHDM_OBJECT_TYPE::uhdmstruct_typespec) {
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
                    } else if (ttype == UHDM_OBJECT_TYPE::uhdmenum_typespec) {
                      if (name == "name") {
                        // Builtin introspection
                        found = true;
                      }
                    } else if (ttype == UHDM_OBJECT_TYPE::uhdmunion_typespec) {
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
              case UHDM_OBJECT_TYPE::uhdmpacked_array_net: {
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
                  call->VpiParent(target);
                  if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                    cro->Actual_group(call);
                  }
                  // Builtin method
                  found = true;
                  previous = (any*)call;
                }
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmnamed_begin: {
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
              case UHDM_OBJECT_TYPE::uhdmnamed_fork: {
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
              case UHDM_OBJECT_TYPE::uhdmclocking_block: {
                clocking_block* block = (clocking_block*)actual;
                if (block->Clocking_io_decls()) {
                  for (clocking_io_decl* decl : *block->Clocking_io_decls()) {
                    if (decl->VpiName() == name) {
                      found = true;
                      previous = decl;
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(decl);
                      }
                      break;
                    }
                  }
                }
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmmodule_inst: {
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
                if (!found && mod->Interfaces()) {
                  for (auto m : *mod->Interfaces()) {
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
                        break;
                      }
                    }
                  }
                }
                if (!found && mod->Task_funcs()) {
                  for (auto tsf : *mod->Task_funcs()) {
                    if (tsf->VpiName() == name ||
                        tsf->VpiName() == nameIndexed) {
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(tsf);
                      }
                      previous = tsf;
                      found = true;
                      break;
                    }
                  }
                }
                if (!found && mod->Param_assigns()) {
                  for (auto pa : *mod->Param_assigns()) {
                    if (pa->Lhs()->VpiName() == name ||
                        pa->Lhs()->VpiName() == nameIndexed) {
                      if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                        cro->Actual_group(pa->Rhs());
                      }
                      previous = pa;
                      found = true;
                      break;
                    }
                  }
                }
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmclass_var: {
                const typespec* tps = nullptr;
                if (const ref_typespec* rt = ((class_var*)actual)->Typespec()) {
                  tps = rt->Actual_typespec();
                }
                if (tps == nullptr) break;
                UHDM_OBJECT_TYPE ttype = tps->UhdmType();
                if (ttype == UHDM_OBJECT_TYPE::uhdmclass_typespec) {
                  class_typespec* ctps = (class_typespec*)tps;
                  any* tmp = bindClassTypespec(ctps, current, name, found);
                  if (found) {
                    previous = tmp;
                  }
                } else if (ttype == UHDM_OBJECT_TYPE::uhdmstruct_typespec) {
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
                if (current->UhdmType() ==
                    UHDM_OBJECT_TYPE::uhdmmethod_func_call) {
                  found = true;
                }
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmstruct_net:
              case UHDM_OBJECT_TYPE::uhdmstruct_var: {
                VectorOftypespec_member* members = nullptr;
                if (actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_net) {
                  if (ref_typespec* rt = ((struct_net*)actual)->Typespec()) {
                    if (struct_typespec* sts =
                            rt->Actual_typespec<struct_typespec>()) {
                      members = sts->Members();
                    } else if (union_typespec* uts =
                                   rt->Actual_typespec<union_typespec>()) {
                      members = uts->Members();
                    }
                  }
                } else if (actual->UhdmType() ==
                           UHDM_OBJECT_TYPE::uhdmstruct_var) {
                  if (ref_typespec* rt = ((struct_var*)actual)->Typespec()) {
                    if (struct_typespec* sts =
                            rt->Actual_typespec<struct_typespec>()) {
                      members = sts->Members();
                    }
                  }
                }
                if (members) {
                  for (typespec_member* member : *members) {
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
              case UHDM_OBJECT_TYPE::uhdmunion_var: {
                union_typespec* stpt = nullptr;
                if (ref_typespec* rt = ((union_var*)actual)->Typespec()) {
                  stpt = rt->Actual_typespec<union_typespec>();
                }
                if (stpt == nullptr) break;
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
                break;
              }
              case UHDM_OBJECT_TYPE::uhdminterface_inst: {
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
                if (!found && interf->Parameters()) {
                  for (any* var : *interf->Parameters()) {
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
                          if (actual_decl->UhdmType() ==
                              UHDM_OBJECT_TYPE::uhdmref_obj) {
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
                if (!found && interf->Gen_scope_arrays()) {
                  for (auto gsa : *interf->Gen_scope_arrays()) {
                    if (gsa->VpiName() == name ||
                        gsa->VpiName() == nameIndexed) {
                      if (!gsa->Gen_scopes()->empty()) {
                        auto gs = gsa->Gen_scopes()->front();
                        if (ref_obj* cro = any_cast<ref_obj*>(current)) {
                          cro->Actual_group(gs);
                        }
                        previous = gs;
                        found = true;
                        break;
                      }
                    }
                  }
                }
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmarray_var: {
                if (current->UhdmType() ==
                    UHDM_OBJECT_TYPE::uhdmmethod_func_call)
                  found = true;
                else if (current->UhdmType() ==
                         UHDM_OBJECT_TYPE::uhdmbit_select)
                  found = true;
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmstring_var: {
                if (current->UhdmType() ==
                    UHDM_OBJECT_TYPE::uhdmmethod_func_call)
                  found = true;
                else if (current->UhdmType() ==
                         UHDM_OBJECT_TYPE::uhdmbit_select)
                  found = true;
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmclass_typespec: {
                class_typespec* ctps = (class_typespec*)actual;
                any* tmp = bindClassTypespec(ctps, current, name, found);
                if (found) {
                  previous = tmp;
                }
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmio_decl: {
                io_decl* decl = (io_decl*)actual;
                typespec* tps = nullptr;
                if (ref_typespec* rt = decl->Typespec()) {
                  tps = rt->Actual_typespec();
                }
                if (tps == nullptr) break;
                UHDM_OBJECT_TYPE ttype = tps->UhdmType();
                if (ttype == UHDM_OBJECT_TYPE::uhdmstring_typespec) {
                  found = true;
                } else if (ttype == UHDM_OBJECT_TYPE::uhdmclass_typespec) {
                  class_typespec* ctps = (class_typespec*)tps;
                  any* tmp = bindClassTypespec(ctps, current, name, found);
                  if (found) {
                    previous = tmp;
                  }
                } else if (ttype == UHDM_OBJECT_TYPE::uhdmstruct_typespec) {
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
                } else if (ttype == UHDM_OBJECT_TYPE::uhdmenum_typespec) {
                  if (name == "name") {
                    // Builtin introspection
                    found = true;
                  }
                } else if (ttype == UHDM_OBJECT_TYPE::uhdmunion_typespec) {
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
                if (decl->Ranges()) {
                  if (current->UhdmType() ==
                      UHDM_OBJECT_TYPE::uhdmmethod_func_call)
                    found = true;
                  else if (current->UhdmType() ==
                           UHDM_OBJECT_TYPE::uhdmbit_select)
                    found = true;
                }
                // TODO: class method support
                if (current->UhdmType() ==
                    UHDM_OBJECT_TYPE::uhdmmethod_func_call)
                  found = true;
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmparameter: {
                parameter* param = (parameter*)actual;
                const typespec* tps = nullptr;
                if (const ref_typespec* rt = param->Typespec()) {
                  tps = rt->Actual_typespec();
                }
                if (tps == nullptr) break;
                UHDM_OBJECT_TYPE ttype = tps->UhdmType();
                if (ttype == UHDM_OBJECT_TYPE::uhdmpacked_array_typespec) {
                  packed_array_typespec* ptps = (packed_array_typespec*)tps;
                  if (const ref_typespec* ert = ptps->Elem_typespec()) {
                    if (const typespec* ets = ert->Actual_typespec()) {
                      tps = ets;
                      ttype = ets->UhdmType();
                    }
                  }
                } else if (ttype == UHDM_OBJECT_TYPE::uhdmarray_typespec) {
                  array_typespec* ptps = (array_typespec*)tps;
                  if (const ref_typespec* ert = ptps->Elem_typespec()) {
                    if (const typespec* ets = ert->Actual_typespec()) {
                      tps = ets;
                      ttype = ets->UhdmType();
                    }
                  }
                }
                if (ttype == UHDM_OBJECT_TYPE::uhdmstring_typespec) {
                  found = true;
                } else if (ttype == UHDM_OBJECT_TYPE::uhdmclass_typespec) {
                  class_typespec* ctps = (class_typespec*)tps;
                  any* tmp = bindClassTypespec(ctps, current, name, found);
                  if (found) {
                    previous = tmp;
                  }
                } else if (ttype == UHDM_OBJECT_TYPE::uhdmstruct_typespec) {
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
                } else if (ttype == UHDM_OBJECT_TYPE::uhdmenum_typespec) {
                  if (name == "name") {
                    // Builtin introspection
                    found = true;
                  }
                } else if (ttype == UHDM_OBJECT_TYPE::uhdmunion_typespec) {
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
                if (param->Ranges()) {
                  if (current->UhdmType() ==
                      UHDM_OBJECT_TYPE::uhdmmethod_func_call)
                    found = true;
                  else if (current->UhdmType() ==
                           UHDM_OBJECT_TYPE::uhdmbit_select)
                    found = true;
                }
                break;
              }
              case UHDM_OBJECT_TYPE::uhdmoperation: {
                operation* op = (operation*)actual;
                if (op->VpiOpType() != vpiAssignmentPatternOp) {
                  break;
                }
                const struct_typespec* stps = nullptr;
                if (const ref_typespec* rt = op->Typespec()) {
                  stps = rt->Actual_typespec<struct_typespec>();
                }
                if (stps == nullptr) break;
                std::vector<std::string_view> fieldNames;
                std::vector<const typespec*> fieldTypes;
                for (typespec_member* memb : *stps->Members()) {
                  if (const ref_typespec* rt = memb->Typespec()) {
                    fieldNames.emplace_back(memb->VpiName());
                    fieldTypes.emplace_back(rt->Actual_typespec());
                  }
                }
                std::vector<any*> tmp(fieldNames.size());
                VectorOfany* orig = op->Operands();
                any* defaultOp = nullptr;
                any* res = nullptr;
                int32_t index = 0;
                for (auto oper : *orig) {
                  if (oper->UhdmType() ==
                      UHDM_OBJECT_TYPE::uhdmtagged_pattern) {
                    tagged_pattern* tp = (tagged_pattern*)oper;
                    const typespec* ttp = nullptr;
                    if (const ref_typespec* rt = tp->Typespec()) {
                      ttp = rt->Actual_typespec();
                    }
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
              case UHDM_OBJECT_TYPE::uhdmref_var: {
                found = true;
                // TODO: class var support
                break;
              }
              default:
                // TODO: class method support
                if (current->UhdmType() ==
                    UHDM_OBJECT_TYPE::uhdmmethod_func_call)
                  found = true;
                break;
            }
            if (!found) {
              if ((!muteErrors()) && (!isInUhdmAllIterator())) {
                const std::string errMsg(source->VpiName());
                m_serializer->GetErrorHandler()(
                    ErrorType::UHDM_UNRESOLVED_HIER_PATH, errMsg, source,
                    nullptr);
              }
            }
          } else {
            if ((!muteErrors()) && (!isInUhdmAllIterator())) {
              if (previous->UhdmType() == UHDM_OBJECT_TYPE::uhdmbit_select) {
                break;
              }
              const std::string errMsg(source->VpiName());
              m_serializer->GetErrorHandler()(
                  ErrorType::UHDM_UNRESOLVED_HIER_PATH, errMsg, source, nullptr);
            }
          }
        } else if (previous->UhdmType() ==
                   UHDM_OBJECT_TYPE::uhdmtypespec_member) {
          typespec_member* member = (typespec_member*)previous;
          const typespec* tps = nullptr;
          if (const ref_typespec* rt = member->Typespec()) {
            tps = rt->Actual_typespec();
          }
          if (tps == nullptr) break;
          UHDM_OBJECT_TYPE ttype = tps->UhdmType();
          if (ttype == UHDM_OBJECT_TYPE::uhdmpacked_array_typespec) {
            packed_array_typespec* ptps = (packed_array_typespec*)tps;
            if (const ref_typespec* rt = ptps->Elem_typespec()) {
              tps = rt->Actual_typespec();
              ttype = tps->UhdmType();
            }
          } else if (ttype == UHDM_OBJECT_TYPE::uhdmarray_typespec) {
            array_typespec* ptps = (array_typespec*)tps;
            if (const ref_typespec* rt = ptps->Elem_typespec()) {
              tps = rt->Actual_typespec();
              ttype = tps->UhdmType();
            }
          }
          if (ttype == UHDM_OBJECT_TYPE::uhdmstruct_typespec) {
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
          } else if (ttype == UHDM_OBJECT_TYPE::uhdmunion_typespec) {
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
          } else if (ttype == UHDM_OBJECT_TYPE::uhdmstring_typespec) {
            if (name == "len") {
              found = true;
            }
          }
        } else if (previous->UhdmType() == UHDM_OBJECT_TYPE::uhdmarray_var) {
          array_var* avar = (array_var*)previous;
          if (VectorOfvariables* vars = avar->Variables()) {
            if (!vars->empty()) {
              variables* actual = vars->front();
              UHDM_OBJECT_TYPE actual_type = actual->UhdmType();
              switch (actual_type) {
                case UHDM_OBJECT_TYPE::uhdmstruct_net:
                case UHDM_OBJECT_TYPE::uhdmstruct_var: {
                  VectorOftypespec_member* members = nullptr;
                  if (actual->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_net) {
                    if (ref_typespec* rt = ((struct_net*)actual)->Typespec()) {
                      if (struct_typespec* sts =
                              rt->Actual_typespec<struct_typespec>()) {
                        members = sts->Members();
                      } else if (union_typespec* uts =
                                     rt->Actual_typespec<union_typespec>()) {
                        members = uts->Members();
                      }
                    }
                  } else if (actual->UhdmType() ==
                             UHDM_OBJECT_TYPE::uhdmstruct_var) {
                    if (ref_typespec* rt = ((struct_var*)actual)->Typespec()) {
                      if (struct_typespec* sts =
                              rt->Actual_typespec<struct_typespec>()) {
                        members = sts->Members();
                      }
                    }
                  }
                  if (members) {
                    for (typespec_member* member : *members) {
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
          }
        } else if (previous->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_var ||
                   previous->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_net) {
          VectorOftypespec_member* members = nullptr;
          if (previous->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_net) {
            if (ref_typespec* rt = ((struct_net*)previous)->Typespec()) {
              if (struct_typespec* sts =
                      rt->Actual_typespec<struct_typespec>()) {
                members = sts->Members();
              } else if (union_typespec* uts =
                             rt->Actual_typespec<union_typespec>()) {
                members = uts->Members();
              }
            }
          } else if (previous->UhdmType() == UHDM_OBJECT_TYPE::uhdmstruct_var) {
            if (ref_typespec* rt = ((struct_var*)previous)->Typespec()) {
              if (struct_typespec* sts =
                      rt->Actual_typespec<struct_typespec>()) {
                members = sts->Members();
              }
            }
          }
          if (members) {
            for (typespec_member* member : *members) {
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
        } else if (previous->UhdmType() == UHDM_OBJECT_TYPE::uhdmmodule_inst) {
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
        } else if (previous->UhdmType() == UHDM_OBJECT_TYPE::uhdmgen_scope) {
          gen_scope* scope = (gen_scope*)previous;
          if (obj->UhdmType() == UHDM_OBJECT_TYPE::uhdmmethod_func_call) {
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
          } else if (obj->UhdmType() ==
                     UHDM_OBJECT_TYPE::uhdmmethod_task_call) {
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
            if (!found && scope->Nets()) {
              for (nets* n : *scope->Nets()) {
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
          }
        }
      }
      if (!found) previous = current;
    }
  }
  if (auto vec = source->VpiUses()) target->VpiUses(DeepClone(vec, target));
  if (auto obj = source->Typespec()) target->Typespec(clone(obj, target));
  return target;
}
}  // namespace UHDM
