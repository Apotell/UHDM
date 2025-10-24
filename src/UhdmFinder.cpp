/*
 Copyright 2019 Alain Dargelas

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
 * File:   UhdmFinder.cpp
 * Author: Manav Kumar
 *
 * Created on October 04, 2025, 00:00 AM
 */


#include <uhdm/UhdmFinder.h>

// uhdm
#include <uhdm/Serializer.h>
#include <uhdm/uhdm.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace UHDM {
  std::vector<std::string_view>& tokenizeMulti(
    std::string_view str, std::string_view multichar_separator,
    std::vector<std::string_view>& result) {
    if (str.empty()) return result;
    
    size_t start = 0;
    size_t end = 0;
    const size_t sepSize = multichar_separator.size();
    const size_t stringSize = str.size();
    for (size_t i = 0; i < stringSize; i++) {
      bool isSeparator = true;
      for (size_t j = 0; j < sepSize; j++) {
        if (i + j >= stringSize) break;
        if (str[i + j] != multichar_separator[j]) {
          isSeparator = false;
          break;
        }
      }
      if (isSeparator) {
        result.emplace_back(str.data() + start, end - start);
        start = end = end + sepSize;
        i = i + sepSize - 1;
      } else {
        ++end;
      }
    }
    result.emplace_back(str.data() + start, end - start);
    return result;
  }
  
  std::vector<std::string>& tokenizeMulti(
    std::string_view str, std::string_view multichar_separator,
    std::vector<std::string>& result) {
    std::vector<std::string_view> view_result;
    tokenizeMulti(str, multichar_separator, view_result);
    result.insert(result.end(), view_result.begin(), view_result.end());
    return result;
  }

  std::string_view ltrim(std::string_view str) {
    while (!str.empty() &&
           std::isspace<char>(str.front(), std::locale::classic())) {
      str.remove_prefix(1);
    }
    return str;
  }
  
  std::string_view rtrim(std::string_view str) {
    while (!str.empty() &&
           std::isspace<char>(str.back(), std::locale::classic())) {
      str.remove_suffix(1);
    }
    return str;
  }
  
  std::string_view trim(std::string_view str) {
    return ltrim(rtrim(str));
  }

  template <typename... Ts>
  std::string StrCat(Ts&&... args) {
    std::ostringstream out;
    (out << ... << std::forward<Ts>(args));
    return out.str();
  }

// UhdmFinder::UhdmFinder(Session* session,
//                            const ForwardComponentMap& componentMap,
//                            uhdm::Serializer& serializer, bool muteStdout)
//     : m_session(session),
//       m_forwardComponentMap(componentMap),
//       m_serializer(serializer),
//       m_muteStdout(muteStdout) {
//   for (ForwardComponentMap::const_reference entry : m_forwardComponentMap) {
//     m_reverseComponentMap.emplace(entry.second, entry.first);
//   }
// }

const uhdm::Any* UHDM::UhdmFinder::findInTypespec(std::string_view name,
                                              RefType refType,
                                              const uhdm::Typespec* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  switch (scope->getUhdmType()) {
    case uhdm::UhdmType::EnumTypespec: {
      if (const uhdm::Any* const actual = findInCollection(
              name, refType,
              static_cast<const uhdm::EnumTypespec*>(scope)->getEnumConsts(),
              scope)) {
        return actual;
      }
    } break;

    case uhdm::UhdmType::StructTypespec: {
      if (const uhdm::Any* const actual = findInCollection(
              name, refType,
              static_cast<const uhdm::StructTypespec*>(scope)->getMembers(),
              scope)) {
        return actual;
      }
    } break;

    case uhdm::UhdmType::UnionTypespec: {
      if (const uhdm::Any* const actual = findInCollection(
              name, refType,
              static_cast<const uhdm::UnionTypespec*>(scope)->getMembers(),
              scope)) {
        return actual;
      }
    } break;

    case uhdm::UhdmType::ImportTypespec: {
      const uhdm::ImportTypespec* const it =
          any_cast<uhdm::ImportTypespec>(scope);
      if (const uhdm::Package* const p = getPackage(it->getName(), it)) {
        if (const uhdm::Any* const actual = findInPackage(name, refType, p)) {
          return actual;
        }
      }
    } break;

    case uhdm::UhdmType::ClassTypespec: {
      if (const uhdm::ClassDefn* cd =
              static_cast<const uhdm::ClassTypespec*>(scope)->getClassDefn()) {
        if (const uhdm::Any* const actual =
                findInClassDefn(name, refType, cd)) {
          return actual;
        }
      }
    } break;

    case uhdm::UhdmType::InterfaceTypespec: {
      if (const uhdm::Interface* ins =
              static_cast<const uhdm::InterfaceTypespec*>(scope)
                  ->getInterface()) {
        if (const uhdm::Any* const actual =
                findInInterface(name, refType, ins)) {
          return actual;
        }
      }
    } break;

    case uhdm::UhdmType::TypedefTypespec: {
      if (const uhdm::Any* const actual =
              findInRefTypespec(name, refType,
                                static_cast<const uhdm::TypedefTypespec*>(scope)
                                    ->getTypedefAlias())) {
        return actual;
      }
    } break;

    default:
      break;
  }

  return nullptr;
}

inline bool UhdmFinder::areSimilarNames(std::string_view name1,
                                          std::string_view name2) const {
  size_t pos = name1.find("::");
  if (pos != std::string::npos) {
    name1 = name1.substr(pos + 2);
  }

  pos = name1.find("work@");
  if (pos != std::string::npos) {
    name1 = name1.substr(pos + 5);
  }

  pos = name2.find("::");
  if (pos != std::string::npos) {
    name2 = name2.substr(pos + 2);
  }

  pos = name2.find("work@");
  if (pos != std::string::npos) {
    name2 = name2.substr(pos + 5);
  }

  return !name1.empty() && name1 == name2;
}

inline bool UhdmFinder::areSimilarNames(const uhdm::Any* object1,
                                          std::string_view name2) const {
  return areSimilarNames(object1->getName(), name2);
}

inline bool UhdmFinder::areSimilarNames(const uhdm::Any* object1,
                                          const uhdm::Any* object2) const {
  return areSimilarNames(object1->getName(), object2->getName());
}

bool UhdmFinder::isInElaboratedTree(const uhdm::Any* object) {
  const uhdm::Any* p = object;
  while (p != nullptr) {
    if (const uhdm::Instance* const inst = any_cast<uhdm::Instance>(p)) {
      if (inst->getTop()) return true;
    }
    p = p->getParent();
  }
  return false;
}

// VObjectType UhdmFinder::getDefaultNetType(
//     const uhdm::ValuedComponentI* component) const {
//   if (component == nullptr) return VObjectType::NO_TYPE;

//   if (const DesignComponent* dc1 =
//           valuedcomponenti_cast<DesignComponent>(component)) {
//     if (const DesignElement* de = dc1->getDesignElement()) {
//       return de->m_defaultNetType;
//     }
//   }

//   if (const ModuleInstance* mi =
//           valuedcomponenti_cast<ModuleInstance>(component)) {
//     if (const DesignComponent* dc2 =
//             valuedcomponenti_cast<DesignComponent>(mi->getDefinition())) {
//       if (const DesignElement* de = dc2->getDesignElement()) {
//         return de->m_defaultNetType;
//       }
//     }
//   }

//   return VObjectType::NO_TYPE;
// }

template <typename T>
const T* UhdmFinder::getParent(const uhdm::Any* object) const {
  while (object != nullptr) {
    if (const T* const p = any_cast<T>(object)) {
      return p;
    }
    object = object->getParent();
  }
  return nullptr;
}

const uhdm::Package* UhdmFinder::getPackage(std::string_view name,
                                              const uhdm::Any* object) const {
  if (const uhdm::Package* const p = getParent<uhdm::Package>(object)) {
    if (areSimilarNames(p, name)) {
      return p;
    }
  }

  if (const uhdm::Design* const d = getParent<uhdm::Design>(object)) {
    if (const uhdm::PackageCollection* const packages = d->getAllPackages()) {
      for (const uhdm::Package* p : *packages) {
        if (areSimilarNames(p, name)) {
          return p;
        }
      }
    }
  }

  return nullptr;
}

const uhdm::Module* UhdmFinder::getModule(std::string_view defname,
                                            const uhdm::Any* object) const {
  if (const uhdm::Module* const m = getParent<uhdm::Module>(object)) {
    if (m->getDefName() == defname) {
      return m;
    }
  }

  if (const uhdm::Design* const d = getParent<uhdm::Design>(object)) {
    if (const uhdm::ModuleCollection* const modules = d->getAllModules()) {
      for (const uhdm::Module* m : *modules) {
        if (m->getDefName() == defname) {
          return m;
        }
      }
    }
  }

  return nullptr;
}

const uhdm::Interface* UhdmFinder::getInterface(
    std::string_view defname, const uhdm::Any* object) const {
  if (const uhdm::Interface* const m = getParent<uhdm::Interface>(object)) {
    if (m->getDefName() == defname) {
      return m;
    }
  }

  if (const uhdm::Design* d = getParent<uhdm::Design>(object)) {
    if (const uhdm::InterfaceCollection* const interfaces =
            d->getAllInterfaces()) {
      for (const uhdm::Interface* m : *interfaces) {
        if (m->getDefName() == defname) {
          return m;
        }
      }
    }
  }

  return nullptr;
}

const uhdm::ClassDefn* UhdmFinder::getClassDefn(
    const uhdm::ClassDefnCollection* collection, std::string_view name) const {
  if (collection != nullptr) {
    for (const uhdm::ClassDefn* c : *collection) {
      if (areSimilarNames(c, name)) {
        return c;
      }
    }
  }
  return nullptr;
}

const uhdm::ClassDefn* UhdmFinder::getClassDefn(
    std::string_view name, const uhdm::Any* object) const {
  if (const uhdm::ClassDefn* const c = getParent<uhdm::ClassDefn>(object)) {
    if (areSimilarNames(c, name)) {
      return c;
    }
  }

  if (const uhdm::Package* const p = getParent<uhdm::Package>(object)) {
    if (const uhdm::ClassDefn* const c =
            getClassDefn(p->getClassDefns(), name)) {
      return c;
    }
  }

  if (const uhdm::Design* const d = getParent<uhdm::Design>(object)) {
    if (const uhdm::ClassDefn* const c =
            getClassDefn(d->getAllClasses(), name)) {
      return c;
    }

    if (d->getTypespecs() != nullptr) {
      for (const uhdm::Typespec* const t : *d->getTypespecs()) {
        if (const uhdm::ImportTypespec* const it =
                t->Cast<uhdm::ImportTypespec>()) {
          if (const uhdm::Constant* const i = it->getItem()) {
            if ((i->getValue() == "STRING:*") ||
                (i->getValue() == StrCat("STRING:", name))) {
              if (const uhdm::Package* const p =
                      getPackage(it->getName(), it)) {
                if (const uhdm::ClassDefn* const c =
                        getClassDefn(p->getClassDefns(), name)) {
                  return c;
                }
              }
            }
          }
        }
      }
    }
  }

  return nullptr;
}

// void UhdmFinder::enterHierPath(const uhdm::HierPath* object,
//                                  uint32_t vpiRelation) {
//   m_prefixStack.emplace_back(object);
// }

// void UhdmFinder::leaveHierPath(const uhdm::HierPath* object,
//                                  uint32_t vpiRelation) {
//   if (!m_prefixStack.empty() && (m_prefixStack.back() == object)) {
//     m_prefixStack.pop_back();
//   }
// }

const uhdm::Any* UhdmFinder::findInRefTypespec(
    std::string_view name, RefType refType, const uhdm::RefTypespec* scope) {
  if (scope == nullptr) return nullptr;

  if (const uhdm::Typespec* const ts = scope->getActual()) {
    return findInTypespec(name, refType, ts);
  }
  return nullptr;
}

template <typename T>
const uhdm::Any* UhdmFinder::findInCollection(
    std::string_view name, RefType refType, const std::vector<T*>* collection,
    const uhdm::Any* scope) {
  if (collection == nullptr) return nullptr;

  std::string_view shortName = name;
  if (shortName.find("::") != std::string::npos) {
    std::vector<std::string_view> tokens;
    UHDM::tokenizeMulti(shortName, "::", tokens);
    if (tokens.size() > 1) shortName = tokens.back();
  }

  for (const uhdm::Any* c : *collection) {
    if (c->getUhdmType() == uhdm::UhdmType::UnsupportedTypespec) continue;
    if (c->getUhdmType() == uhdm::UhdmType::UnsupportedStmt) continue;
    if (c->getUhdmType() == uhdm::UhdmType::UnsupportedExpr) continue;
    if (c->getUhdmType() == uhdm::UhdmType::VarSelect) continue;
    if (any_cast<uhdm::RefObj>(c) != nullptr) continue;

    if (any_cast<uhdm::Typespec>(c) == nullptr) {
      if (refType == RefType::Object) {
        if (areSimilarNames(c, name)) return c;
        if (areSimilarNames(c, shortName)) return c;
      }
    } else {
      if (refType == RefType::Typespec) {
        if (areSimilarNames(c, name)) return c;
        if (areSimilarNames(c, shortName)) return c;
      }
    }

    if (const uhdm::EnumTypespec* const et = any_cast<uhdm::EnumTypespec>(c)) {
      if (const uhdm::Any* const actual = findInTypespec(name, refType, et)) {
        return actual;
      }
    }

    if (c->getUhdmType() == uhdm::UhdmType::EnumVar) {
      if (const uhdm::Any* const actual = findInRefTypespec(
              name, refType,
              static_cast<const uhdm::EnumVar*>(c)->getTypespec())) {
        return actual;
      } else if (const uhdm::Any* const actual = findInRefTypespec(
                     shortName, refType,
                     static_cast<const uhdm::EnumVar*>(c)->getTypespec())) {
        return actual;
      }
    }
    // if (c->getUhdmType() == uhdm::UhdmType::StructVar) {
    //   if (const uhdm::Any* const actual = findInRefTypespec(
    //           name, static_cast<const uhdm::StructVar*>(c)->getTypespec())) {
    //     return actual;
    //   }
    // }
    if (const uhdm::RefTypespec* rt = any_cast<uhdm::RefTypespec>(c)) {
      if (scope != rt->getActual()) {
        if (const uhdm::Any* const actual =
                findInRefTypespec(name, refType, rt)) {
          return actual;
        } else if (const uhdm::Any* const actual =
                       findInRefTypespec(shortName, refType, rt)) {
          return actual;
        }
      }
    }
  }

  return nullptr;
}

const uhdm::Any* UhdmFinder::findInScope(std::string_view name,
                                           RefType refType,
                                           const uhdm::Scope* scope) {
  if (scope == nullptr) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getVariables(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual =
                 findInCollection(name, refType, scope->getGenVars(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getParamAssigns(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getParameters(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getPropertyDecls(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getTypespecs(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getNamedEvents(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getInternalScopes(), scope)) {
    return actual;
  } else if (const uhdm::Package* const p = any_cast<uhdm::Package>(scope)) {
    std::string fullName = StrCat(p->getName(), "::", name);
    if (const uhdm::Any* const actual =
            findInCollection(fullName, refType, scope->getTypespecs(), scope)) {
      return actual;
    }
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getInstanceItems(), scope)) {
    return actual;
  }

  return nullptr;
}

const uhdm::Any* UhdmFinder::findInInstance(std::string_view name,
                                              RefType refType,
                                              const uhdm::Instance* scope) {
  if (scope == nullptr) return nullptr;

  if (const uhdm::Any* const actual =
          findInCollection(name, refType, scope->getNets(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getArrayNets(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getTaskFuncs(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual =
                 findInCollection(name, refType, scope->getPrograms(), scope)) {
    return actual;
  }

  return findInScope(name, refType, scope);
}

const uhdm::Any* UhdmFinder::findInInterface(std::string_view name,
                                               RefType refType,
                                               const uhdm::Interface* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const uhdm::Any* const actual =
                 findInCollection(name, refType, scope->getModports(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getInterfaceTFDecls(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual =
                 findInCollection(name, refType, scope->getPorts(), scope)) {
    return actual;
  }
  return findInInstance(name, refType, scope);
}

const uhdm::Any* UhdmFinder::findInPackage(std::string_view name,
                                             RefType refType,
                                             const uhdm::Package* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getParameters(), scope)) {
    return actual;
  }

  if (const uhdm::Any* const actual = findInInstance(name, refType, scope)) {
    return actual;
  }

  if (scope->getTypespecs() != nullptr) {
    for (const uhdm::Typespec* const t : *scope->getTypespecs()) {
      if (const uhdm::ImportTypespec* const it =
              t->Cast<uhdm::ImportTypespec>()) {
        if (const uhdm::Constant* const i = it->getItem()) {
          if ((i->getValue() == "STRING:*") ||
              (i->getValue() == StrCat("STRING:", name))) {
            if (const uhdm::Package* const p = getPackage(it->getName(), it)) {
              if (const uhdm::Any* const actual =
                      findInPackage(name, refType, p)) {
                return actual;
              }
            }
          }
        }
      }
    }
  }

  return nullptr;
}

const uhdm::Any* UhdmFinder::findInUdpDefn(std::string_view name,
                                             RefType refType,
                                             const uhdm::UdpDefn* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) return scope;

  return findInCollection(name, refType, scope->getIODecls(), scope);
}

const uhdm::Any* UhdmFinder::findInProgram(std::string_view name,
                                             RefType refType,
                                             const uhdm::Program* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getParameters(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual =
                 findInCollection(name, refType, scope->getPorts(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getInterfaces(), scope)) {
    return actual;
  }

  return findInInstance(name, refType, scope);
}

const uhdm::Any* UhdmFinder::findInFunction(std::string_view name,
                                              RefType refType,
                                              const uhdm::Function* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const uhdm::Any* const actual =
                 findInCollection(name, refType, scope->getIODecls(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getVariables(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getParameters(), scope)) {
    return actual;
    // } else if (const uhdm::Package* const inst =
    //               scope->getInstance<uhdm::Package>()) {
    //  if (const uhdm::Any* const actual = findInPackage(name, refType, inst))
    //  {
    //    return actual;
    //  }
  }

  return findInScope(name, refType, scope);
}

const uhdm::Any* UhdmFinder::findInTask(std::string_view name,
                                          RefType refType,
                                          const uhdm::Task* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const uhdm::Any* const actual =
                 findInCollection(name, refType, scope->getIODecls(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getVariables(), scope)) {
    return actual;
  } else if (const uhdm::Package* const p =
                 scope->getInstance<uhdm::Package>()) {
    if (const uhdm::Any* const actual = findInPackage(name, refType, p)) {
      return actual;
    }
  }

  return findInScope(name, refType, scope);
}

const uhdm::Any* UhdmFinder::findInForStmt(std::string_view name,
                                             RefType refType,
                                             const uhdm::ForStmt* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  std::string_view shortName = name;
  if (shortName.find("::") != std::string::npos) {
    std::vector<std::string_view> tokens;
    UHDM::tokenizeMulti(shortName, "::", tokens);
    if (tokens.size() > 1) shortName = tokens.back();
  }

  if (const uhdm::AnyCollection* const inits = scope->getForInitStmts()) {
    for (auto init : *inits) {
      if (init->getUhdmType() == uhdm::UhdmType::AssignStmt) {
        const uhdm::Expr* const lhs =
            static_cast<const uhdm::AssignStmt*>(init)->getLhs();
        if (lhs->getUhdmType() == uhdm::UhdmType::UnsupportedTypespec) continue;
        if (lhs->getUhdmType() == uhdm::UhdmType::UnsupportedStmt) continue;
        if (lhs->getUhdmType() == uhdm::UhdmType::UnsupportedExpr) continue;
        if (lhs->getUhdmType() == uhdm::UhdmType::VarSelect) continue;
        if (any_cast<uhdm::RefObj>(lhs) != nullptr) continue;
        if (areSimilarNames(lhs, name)) return lhs;
        if (areSimilarNames(lhs, shortName)) return lhs;
      }
    }
  }

  return findInScope(name, refType, scope);
}

const uhdm::Any* UhdmFinder::findInForeachStmt(
    std::string_view name, RefType refType, const uhdm::ForeachStmt* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (const uhdm::Any* const var =
          findInCollection(name, refType, scope->getLoopVars(), scope)) {
    return var;
  }

  return findInScope(name, refType, scope);
}

template <typename T>
const uhdm::Any* UhdmFinder::findInScope_sub(
    std::string_view name, RefType refType, const T* scope,
    typename std::enable_if<
        std::is_same<uhdm::Begin, typename std::decay<T>::type>::value ||
        std::is_same<uhdm::ForkStmt,
                     typename std::decay<T>::type>::value>::type* /* = 0 */) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  std::string_view shortName = name;
  if (shortName.find("::") != std::string::npos) {
    std::vector<std::string_view> tokens;
    UHDM::tokenizeMulti(shortName, "::", tokens);
    if (tokens.size() > 1) shortName = tokens.back();
  }

  if (areSimilarNames(scope, name) || areSimilarNames(scope, shortName)) {
    return scope;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getVariables(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getParameters(), scope)) {
    return actual;
  }

  if (const uhdm::AnyCollection* const stmts = scope->getStmts()) {
    for (auto init : *stmts) {
      if (init->getUhdmType() == uhdm::UhdmType::AssignStmt) {
        const uhdm::Expr* const lhs =
            static_cast<const uhdm::AssignStmt*>(init)->getLhs();
        if (lhs->getUhdmType() == uhdm::UhdmType::UnsupportedTypespec) continue;
        if (lhs->getUhdmType() == uhdm::UhdmType::UnsupportedStmt) continue;
        if (lhs->getUhdmType() == uhdm::UhdmType::UnsupportedExpr) continue;
        if (lhs->getUhdmType() == uhdm::UhdmType::VarSelect) continue;
        if (any_cast<uhdm::RefObj>(lhs) != nullptr) continue;
        if (areSimilarNames(lhs, name)) return lhs;
        if (areSimilarNames(lhs, shortName)) return lhs;
      }
    }
  }

  return findInScope(name, refType, scope);
}

const uhdm::Any* UhdmFinder::findInClassDefn(std::string_view name,
                                               RefType refType,
                                               const uhdm::ClassDefn* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  std::string_view shortName = name;
  if (shortName.find("::") != std::string::npos) {
    std::vector<std::string_view> tokens;
    UHDM::tokenizeMulti(shortName, "::", tokens);
    if (tokens.size() > 1) shortName = tokens.back();
  }

  if (areSimilarNames(name, "this")) {
    return scope;
  } else if (areSimilarNames(name, "super")) {
    if (const uhdm::Extends* ext = scope->getExtends()) {
      if (const uhdm::RefTypespec* rt = ext->getClassTypespec()) {
        if (const uhdm::ClassTypespec* cts =
                rt->getActual<uhdm::ClassTypespec>())
          return cts->getClassDefn();
      }
    }
    return nullptr;
  }

  if (areSimilarNames(scope, name) || areSimilarNames(scope, shortName)) {
    return scope;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getVariables(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual =
                 findInCollection(name, refType, scope->getMethods(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInScope(
                 name, refType, static_cast<const uhdm::Scope*>(scope))) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getConstraints(), scope)) {
    return actual;
  } else if (const uhdm::Extends* ext = scope->getExtends()) {
    if (const uhdm::RefTypespec* rt = ext->getClassTypespec()) {
      if (const uhdm::ClassTypespec* cts =
              rt->getActual<uhdm::ClassTypespec>()) {
        return findInClassDefn(name, refType, cts->getClassDefn());
      }
    }
  }

  return findInScope(name, refType, scope);
}

const uhdm::Any* UhdmFinder::findInModule(std::string_view name,
                                            RefType refType,
                                            const uhdm::Module* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getInterfaces(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getInterfaceArrays(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getRefModules(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual =
                 findInInstance(name, refType, scope)) {
    return actual;
  } else if (const uhdm::Any* const actual =
                 findInCollection(name, refType, scope->getPorts(), scope)) {
    return actual;
  }

  return nullptr;
}

const uhdm::Any* UhdmFinder::findInDesign(std::string_view name,
                                            RefType refType,
                                            const uhdm::Design* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(name, "$root") || areSimilarNames(scope, name)) {
    return scope;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getParameters(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getParamAssigns(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getAllPackages(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getAllModules(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getAllClasses(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getAllInterfaces(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual = findInCollection(
                 name, refType, scope->getAllPrograms(), scope)) {
    return actual;
  } else if (const uhdm::Any* const actual =
                 findInCollection(name, refType, scope->getAllUdps(), scope)) {
    return actual;
  }

  return nullptr;
}

const uhdm::Any* UhdmFinder::findObject(const uhdm::Any* object) {
  std::string_view name = object->getName();
  name = UHDM::trim(name);
  if (name.empty()) return nullptr;

  const uhdm::Any* scope = object->getParent();
  if (name.find("::") != std::string::npos) {
    std::vector<std::string_view> tokens;
    UHDM::tokenizeMulti(name, "::", tokens);

    const uhdm::Any* actual = object;
    for (std::string_view token : tokens) {
      if (const uhdm::Package* const p = getPackage(token, actual)) {
        actual = p;
      } else if (const uhdm::ClassDefn* const p = getClassDefn(token, actual)) {
        actual = p;
      } else if (const uhdm::Any* const p =
                     FindObjectInScope(token, object, actual)) {
        actual = p;
      } else {
        return nullptr;
      }
    }
    return actual;
  } else if (!m_prefixStack.empty()) {
    if (const uhdm::Any* const prefix = getPrefix(object)) {
      scope = prefix;

      std::string_view::size_type npos = name.find('.');
      if (npos != std::string_view::npos) name.remove_prefix(npos + 1);
    }
  }

  return FindObjectInScope(name, object, scope);
}

const uhdm::Any* UhdmFinder::FindObjectInScope(std::string_view name,
                                                const uhdm::Any* object,
                                                const uhdm::Any* scope) {
  if (name.empty()) return nullptr;
  if (scope == nullptr) return nullptr;

  m_searched.clear();
  // const ValuedComponentI* component = nullptr;
  const RefType refType = (object->getUhdmType() == uhdm::UhdmType::RefTypespec)
                              ? RefType::Typespec
                              : RefType::Object;

  while (scope != nullptr) {
    switch (scope->getUhdmType()) {
      case uhdm::UhdmType::Function: {
        if (const uhdm::Any* const actual = findInFunction(
                name, refType, static_cast<const uhdm::Function*>(scope))) {
          return actual;
        }
      } break;

      case uhdm::UhdmType::Task: {
        if (const uhdm::Any* const actual = findInTask(
                name, refType, static_cast<const uhdm::Task*>(scope))) {
          return actual;
        }
      } break;

      case uhdm::UhdmType::ForStmt: {
        if (const uhdm::Any* const actual = findInForStmt(
                name, refType, static_cast<const uhdm::ForStmt*>(scope))) {
          return actual;
        }
      } break;

      case uhdm::UhdmType::ForeachStmt: {
        if (const uhdm::Any* const actual = findInForeachStmt(
                name, refType, static_cast<const uhdm::ForeachStmt*>(scope))) {
          return actual;
        }
      } break;

      case uhdm::UhdmType::Begin: {
        if (const uhdm::Any* const actual = findInScope_sub(
                name, refType, static_cast<const uhdm::Begin*>(scope))) {
          return actual;
        }
      } break;

      case uhdm::UhdmType::ForkStmt: {
        if (const uhdm::Any* const actual = findInScope_sub(
                name, refType, static_cast<const uhdm::ForkStmt*>(scope))) {
          return actual;
        }
      } break;

      case uhdm::UhdmType::ClassDefn: {
        if (const uhdm::Any* const actual = findInClassDefn(
                name, refType, static_cast<const uhdm::ClassDefn*>(scope))) {
          return actual;
        }
      } break;

      case uhdm::UhdmType::Module: {
        if (const uhdm::Any* const actual = findInModule(
                name, refType, static_cast<const uhdm::Module*>(scope))) {
          return actual;
        }
      } break;

      case uhdm::UhdmType::Interface: {
        if (const uhdm::Any* const actual = findInInterface(
                name, refType, static_cast<const uhdm::Interface*>(scope))) {
          return actual;
        }
      } break;

      case uhdm::UhdmType::Program: {
        if (const uhdm::Any* const actual = findInProgram(
                name, refType, static_cast<const uhdm::Program*>(scope))) {
          return actual;
        }
      } break;

      case uhdm::UhdmType::Package: {
        if (const uhdm::Any* const actual = findInPackage(
                name, refType, static_cast<const uhdm::Package*>(scope))) {
          return actual;
        }
      } break;

      case uhdm::UhdmType::UdpDefn: {
        if (const uhdm::Any* const actual = findInUdpDefn(
                name, refType, static_cast<const uhdm::UdpDefn*>(scope))) {
          return actual;
        }
      } break;

      case uhdm::UhdmType::Design: {
        if (const uhdm::Any* const actual = findInDesign(
                name, refType, static_cast<const uhdm::Design*>(scope))) {
          return actual;
        }
      } break;

      default: {
        if (const uhdm::Typespec* ts = any_cast<uhdm::Typespec>(scope)) {
          if (const uhdm::Any* const actual =
                  findInTypespec(name, refType, ts)) {
            return actual;
          }
        }
      } break;
    }

    // ReverseComponentMap::const_iterator it = m_reverseComponentMap.find(scope);
    // if (it != m_reverseComponentMap.end()) {
    //   component = it->second;
    // }

    scope = scope->getParent();
  }

  // for (ForwardComponentMap::const_reference entry : m_forwardComponentMap) {
  //   const DesignComponent* dc =
  //       valuedcomponenti_cast<DesignComponent>(entry.first);
  //   if (dc == nullptr) continue;

  //   const auto& fileContents = dc->getFileContents();
  //   if (!fileContents.empty()) {
  //     if (const FileContent* const fC = fileContents.front()) {
  //       for (const auto& td : fC->getTypeDefMap()) {
  //         const DataType* dt = td.second;
  //         while (dt != nullptr) {
  //           if (const uhdm::Typespec* const ts = dt->getTypespec()) {
  //             if (const uhdm::Any* const actual =
  //                     findInTypespec(name, refType, ts)) {
  //               return actual;
  //             }
  //           }
  //           dt = dt->getDefinition();
  //         }
  //       }
  //     }
  //   }
  // }

  // m_unbounded.emplace_back(object, component);
  return nullptr;
}

const uhdm::Any* UhdmFinder::getPrefix(const uhdm::Any* object) {
  if (object == nullptr) return nullptr;
  if (m_prefixStack.empty()) return nullptr;
  if (m_prefixStack.back() != object->getParent()) return nullptr;

  const uhdm::Any* const parent = object->getParent();
  switch (parent->getUhdmType()) {
    case uhdm::UhdmType::HierPath: {
      const uhdm::HierPath* hp = static_cast<const uhdm::HierPath*>(parent);
      if (hp->getPathElems() && (hp->getPathElems()->size() > 1)) {
        for (size_t i = 1, n = hp->getPathElems()->size(); i < n; ++i) {
          if (hp->getPathElems()->at(i) == object) {
            const uhdm::Any* const previous = hp->getPathElems()->at(i - 1);
            if (const uhdm::RefObj* const ro1 =
                    any_cast<uhdm::RefObj>(previous)) {
              if (areSimilarNames(ro1, "this") ||
                  areSimilarNames(ro1, "super")) {
                const uhdm::Any* prefix = ro1->getParent();
                while (prefix != nullptr) {
                  if (prefix->getUhdmType() == uhdm::UhdmType::ClassDefn)
                    return prefix;

                  prefix = prefix->getParent();
                }
                return prefix;
              }

              if (const uhdm::ArrayVar* const av =
                      ro1->getActual<uhdm::ArrayVar>()) {
                if (const uhdm::RefTypespec* const iod2 = av->getTypespec()) {
                  if (const uhdm::ArrayTypespec* const at =
                          iod2->getActual<uhdm::ArrayTypespec>()) {
                    if (const uhdm::RefTypespec* const et =
                            at->getElemTypespec()) {
                      return et->getActual();
                    }
                  }
                }
              } else if (const uhdm::Variables* const var =
                             ro1->getActual<uhdm::Variables>()) {
                if (const uhdm::RefTypespec* const iod2 = var->getTypespec()) {
                  return iod2->getActual();
                }
              } else if (const uhdm::IODecl* iod =
                             ro1->getActual<uhdm::IODecl>()) {
                if (const uhdm::RefTypespec* const iod2 = iod->getTypespec()) {
                  return iod2->getActual();
                }
              } else if (const uhdm::Parameter* const p1 =
                             ro1->getActual<uhdm::Parameter>()) {
                if (const uhdm::RefTypespec* const iod2 = p1->getTypespec()) {
                  return iod2->getActual();
                }
              } else if (const uhdm::Scope* const s =
                             ro1->getActual<uhdm::Scope>()) {
                return s;
              } else if (const uhdm::TypespecMember* const tm =
                             ro1->getActual<uhdm::TypespecMember>()) {
                if (const uhdm::RefTypespec* const iod2 = tm->getTypespec()) {
                  return iod2->getActual();
                }
              } else if (const uhdm::LogicNet* const ln =
                             ro1->getActual<uhdm::LogicNet>()) {
                // Ideally logic_net::Typespec should be valid but for
                // too many (or rather most) cases, the Typespec isn't set.
                // So, use the corresponding port in the parent module to
                // find the typespec.

                const uhdm::Typespec* ts = nullptr;
                if (const uhdm::RefTypespec* rt = ln->getTypespec()) {
                  ts = rt->getActual();
                } else if (const uhdm::Module* mi =
                               ln->getParent<uhdm::Module>()) {
                  if (mi->getPorts() != nullptr) {
                    for (const uhdm::Port* p2 : *mi->getPorts()) {
                      if (const uhdm::RefObj* ro2 =
                              p2->getLowConn<uhdm::RefObj>()) {
                        if (ro2->getActual() == ln) {
                          if (const uhdm::RefTypespec* rt = p2->getTypespec()) {
                            ts = rt->getActual();
                          }
                          break;
                        }
                      }
                    }
                  }
                }

                if (const uhdm::ClassTypespec* const cts =
                        any_cast<uhdm::ClassTypespec>(ts)) {
                  return cts->getClassDefn();
                } else if (const uhdm::StructTypespec* const sts =
                               any_cast<uhdm::StructTypespec>(ts)) {
                  return sts;
                }
              } else if (const uhdm::Port* const p =
                             ro1->getActual<uhdm::Port>()) {
                if (const uhdm::RefTypespec* const iod2 = p->getTypespec()) {
                  return iod2->getActual();
                }
              }
            }
            break;
          }
        }
      }
    } break;

    default:
      break;
  }

  return nullptr;
}

const uhdm::Any* UhdmFinder::findObject(std::string_view name, RefType refType, const uhdm::Any* scope) {
  if (!scope || name.empty()) return nullptr;
  
  m_searched.clear(); // Reset search history for new search
  
  switch (scope->getUhdmType()) {
    
    case uhdm::UhdmType::Design:
      return findInDesign(
              name, refType, static_cast<const uhdm::Design*>(scope)); 

    case uhdm::UhdmType::ClassDefn:
      return findInClassDefn(name, refType, static_cast<const uhdm::ClassDefn*>(scope));

    case uhdm::UhdmType::Module:
      return findInModule(name, refType, static_cast<const uhdm::Module*>(scope));

    case uhdm::UhdmType::Package:
      return findInPackage(name, refType, static_cast<const uhdm::Package*>(scope));
      
    default:
      // For other scopes, try to find in parent scope
      if (const uhdm::Any* parent = scope->getParent()) {
        return findObject(name, refType, parent);
      }
      break;
  }
  
  return nullptr;
}

// void ObjectBinder::enterChandle_var(const uhdm::ChandleVar* const object) {
//   if (object->getActual() != nullptr) return;
//
//   if (const uhdm::Any* actual = bindObject(object)) {
//     const_cast<uhdm::ChandleVar*>(object)->setActual(
//         const_cast<uhdm::Any*>(actual));
//   }
// }



}  // namespace UHDM
