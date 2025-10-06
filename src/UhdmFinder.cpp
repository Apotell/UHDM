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

// uhdm
#include <uhdm/Serializer.h>
#include <uhdm/UhdmFinder.h>
#include <uhdm/uhdm.h>

// System headers
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace uhdm {
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

std::vector<std::string>& tokenizeMulti(std::string_view str,
                                        std::string_view multichar_separator,
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

std::string_view trim(std::string_view str) { return ltrim(rtrim(str)); }

template <typename... Ts>
std::string StrCat(Ts&&... args) {
  std::ostringstream out;
  (out << ... << std::forward<Ts>(args));
  return out.str();
}

const Any* UhdmFinder::findInTypespec(std::string_view name, RefType refType,
                                      const Typespec* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  switch (scope->getUhdmType()) {
    case UhdmType::EnumTypespec: {
      if (const Any* const actual = findInCollection(
              name, refType,
              static_cast<const EnumTypespec*>(scope)->getEnumConsts(),
              scope)) {
        return actual;
      }
    } break;

    case UhdmType::StructTypespec: {
      if (const Any* const actual = findInCollection(
              name, refType,
              static_cast<const StructTypespec*>(scope)->getMembers(), scope)) {
        return actual;
      }
    } break;

    case UhdmType::UnionTypespec: {
      if (const Any* const actual = findInCollection(
              name, refType,
              static_cast<const UnionTypespec*>(scope)->getMembers(), scope)) {
        return actual;
      }
    } break;

    case UhdmType::ImportTypespec: {
      const ImportTypespec* const it = any_cast<ImportTypespec>(scope);
      if (const Package* const p = getPackage(it->getName(), it)) {
        if (const Any* const actual = findInPackage(name, refType, p)) {
          return actual;
        }
      }
    } break;

    case UhdmType::ClassTypespec: {
      if (const ClassDefn* cd =
              static_cast<const ClassTypespec*>(scope)->getClassDefn()) {
        if (const Any* const actual = findInClassDefn(name, refType, cd)) {
          return actual;
        }
      }
    } break;

    case UhdmType::InterfaceTypespec: {
      if (const Interface* ins =
              static_cast<const InterfaceTypespec*>(scope)->getInterface()) {
        if (const Any* const actual = findInInterface(name, refType, ins)) {
          return actual;
        }
      }
    } break;

    case UhdmType::TypedefTypespec: {
      if (const Any* const actual = findInRefTypespec(
              name, refType,
              static_cast<const TypedefTypespec*>(scope)->getTypedefAlias())) {
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

inline bool UhdmFinder::areSimilarNames(const Any* object1,
                                        std::string_view name2) const {
  return areSimilarNames(object1->getName(), name2);
}

inline bool UhdmFinder::areSimilarNames(const Any* object1,
                                        const Any* object2) const {
  return areSimilarNames(object1->getName(), object2->getName());
}

template <typename T>
const T* UhdmFinder::getParent(const Any* object) const {
  while (object != nullptr) {
    if (const T* const p = any_cast<T>(object)) {
      return p;
    }
    object = object->getParent();
  }
  return nullptr;
}

const Package* UhdmFinder::getPackage(std::string_view name,
                                      const Any* object) const {
  if (const Package* const p = getParent<Package>(object)) {
    if (areSimilarNames(p, name)) {
      return p;
    }
  }

  if (const Design* const d = getParent<Design>(object)) {
    if (const PackageCollection* const packages = d->getAllPackages()) {
      for (const Package* p : *packages) {
        if (areSimilarNames(p, name)) {
          return p;
        }
      }
    }
  }

  return nullptr;
}

const Module* UhdmFinder::getModule(std::string_view defname,
                                    const Any* object) const {
  if (const Module* const m = getParent<Module>(object)) {
    if (m->getDefName() == defname) {
      return m;
    }
  }

  if (const Design* const d = getParent<Design>(object)) {
    if (const ModuleCollection* const modules = d->getAllModules()) {
      for (const Module* m : *modules) {
        if (m->getDefName() == defname) {
          return m;
        }
      }
    }
  }

  return nullptr;
}

const Interface* UhdmFinder::getInterface(std::string_view defname,
                                          const Any* object) const {
  if (const Interface* const m = getParent<Interface>(object)) {
    if (m->getDefName() == defname) {
      return m;
    }
  }

  if (const Design* d = getParent<Design>(object)) {
    if (const InterfaceCollection* const interfaces = d->getAllInterfaces()) {
      for (const Interface* m : *interfaces) {
        if (m->getDefName() == defname) {
          return m;
        }
      }
    }
  }

  return nullptr;
}

const ClassDefn* UhdmFinder::getClassDefn(const ClassDefnCollection* collection,
                                          std::string_view name) const {
  if (collection != nullptr) {
    for (const ClassDefn* c : *collection) {
      if (areSimilarNames(c, name)) {
        return c;
      }
    }
  }
  return nullptr;
}

const ClassDefn* UhdmFinder::getClassDefn(std::string_view name,
                                          const Any* object) const {
  if (const ClassDefn* const c = getParent<ClassDefn>(object)) {
    if (areSimilarNames(c, name)) {
      return c;
    }
  }

  if (const Package* const p = getParent<Package>(object)) {
    if (const ClassDefn* const c = getClassDefn(p->getClassDefns(), name)) {
      return c;
    }
  }

  if (const Design* const d = getParent<Design>(object)) {
    if (const ClassDefn* const c = getClassDefn(d->getAllClasses(), name)) {
      return c;
    }

    if (d->getTypespecs() != nullptr) {
      for (const Typespec* const t : *d->getTypespecs()) {
        if (const ImportTypespec* const it = t->Cast<ImportTypespec>()) {
          if (const Constant* const i = it->getItem()) {
            if ((i->getValue() == "STRING:*") ||
                (i->getValue() == StrCat("STRING:", name))) {
              if (const Package* const p = getPackage(it->getName(), it)) {
                if (const ClassDefn* const c =
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

const Any* UhdmFinder::findInRefTypespec(std::string_view name, RefType refType,
                                         const RefTypespec* scope) {
  if (scope == nullptr) return nullptr;

  if (const Typespec* const ts = scope->getActual()) {
    return findInTypespec(name, refType, ts);
  }
  return nullptr;
}

template <typename T>
const Any* UhdmFinder::findInCollection(std::string_view name, RefType refType,
                                        const std::vector<T*>* collection,
                                        const Any* scope) {
  if (collection == nullptr) return nullptr;

  std::string_view shortName = name;
  if (shortName.find("::") != std::string::npos) {
    std::vector<std::string_view> tokens;
    tokenizeMulti(shortName, "::", tokens);
    if (tokens.size() > 1) shortName = tokens.back();
  }

  for (const Any* c : *collection) {
    if (c->getUhdmType() == UhdmType::UnsupportedTypespec) continue;
    if (c->getUhdmType() == UhdmType::UnsupportedStmt) continue;
    if (c->getUhdmType() == UhdmType::UnsupportedExpr) continue;
    if (c->getUhdmType() == UhdmType::VarSelect) continue;
    if (any_cast<RefObj>(c) != nullptr) continue;

    if (any_cast<Typespec>(c) == nullptr) {
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

    if (const EnumTypespec* const et = any_cast<EnumTypespec>(c)) {
      if (const Any* const actual = findInTypespec(name, refType, et)) {
        return actual;
      }
    }

    if (c->getUhdmType() == UhdmType::EnumVar) {
      if (const Any* const actual = findInRefTypespec(
              name, refType, static_cast<const EnumVar*>(c)->getTypespec())) {
        return actual;
      } else if (const Any* const actual = findInRefTypespec(
                     shortName, refType,
                     static_cast<const EnumVar*>(c)->getTypespec())) {
        return actual;
      }
    }
    // if (c->getUhdmType() == UhdmType::StructVar) {
    //   if (const Any* const actual = findInRefTypespec(
    //           name, static_cast<const StructVar*>(c)->getTypespec())) {
    //     return actual;
    //   }
    // }
    if (const RefTypespec* rt = any_cast<RefTypespec>(c)) {
      if (scope != rt->getActual()) {
        if (const Any* const actual = findInRefTypespec(name, refType, rt)) {
          return actual;
        } else if (const Any* const actual =
                       findInRefTypespec(shortName, refType, rt)) {
          return actual;
        }
      }
    }
  }

  return nullptr;
}

const Any* UhdmFinder::findInScope(std::string_view name, RefType refType,
                                   const Scope* scope) {
  if (scope == nullptr) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getVariables(), scope)) {
    return actual;
  } else if (const Any* const actual =
                 findInCollection(name, refType, scope->getGenVars(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getParamAssigns(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getParameters(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getPropertyDecls(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getTypespecs(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getNamedEvents(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getInternalScopes(), scope)) {
    return actual;
  } else if (const Package* const p = any_cast<Package>(scope)) {
    std::string fullName = StrCat(p->getName(), "::", name);
    if (const Any* const actual =
            findInCollection(fullName, refType, scope->getTypespecs(), scope)) {
      return actual;
    }
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getInstanceItems(), scope)) {
    return actual;
  }

  return nullptr;
}

const Any* UhdmFinder::findInInstance(std::string_view name, RefType refType,
                                      const Instance* scope) {
  if (scope == nullptr) return nullptr;

  if (const Any* const actual =
          findInCollection(name, refType, scope->getNets(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getArrayNets(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getTaskFuncs(), scope)) {
    return actual;
  } else if (const Any* const actual =
                 findInCollection(name, refType, scope->getPrograms(), scope)) {
    return actual;
  }

  return findInScope(name, refType, scope);
}

const Any* UhdmFinder::findInInterface(std::string_view name, RefType refType,
                                       const Interface* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const Any* const actual =
                 findInCollection(name, refType, scope->getModports(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getInterfaceTFDecls(), scope)) {
    return actual;
  } else if (const Any* const actual =
                 findInCollection(name, refType, scope->getPorts(), scope)) {
    return actual;
  }
  return findInInstance(name, refType, scope);
}

const Any* UhdmFinder::findInPackage(std::string_view name, RefType refType,
                                     const Package* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getParameters(), scope)) {
    return actual;
  }

  if (const Any* const actual = findInInstance(name, refType, scope)) {
    return actual;
  }

  if (scope->getTypespecs() != nullptr) {
    for (const Typespec* const t : *scope->getTypespecs()) {
      if (const ImportTypespec* const it = t->Cast<ImportTypespec>()) {
        if (const Constant* const i = it->getItem()) {
          if ((i->getValue() == "STRING:*") ||
              (i->getValue() == StrCat("STRING:", name))) {
            if (const Package* const p = getPackage(it->getName(), it)) {
              if (const Any* const actual = findInPackage(name, refType, p)) {
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

const Any* UhdmFinder::findInUdpDefn(std::string_view name, RefType refType,
                                     const UdpDefn* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) return scope;

  return findInCollection(name, refType, scope->getIODecls(), scope);
}

const Any* UhdmFinder::findInProgram(std::string_view name, RefType refType,
                                     const Program* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getParameters(), scope)) {
    return actual;
  } else if (const Any* const actual =
                 findInCollection(name, refType, scope->getPorts(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getInterfaces(), scope)) {
    return actual;
  }

  return findInInstance(name, refType, scope);
}

const Any* UhdmFinder::findInFunction(std::string_view name, RefType refType,
                                      const Function* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const Any* const actual =
                 findInCollection(name, refType, scope->getIODecls(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getVariables(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getParameters(), scope)) {
    return actual;
    // } else if (const Package* const inst =
    //               scope->getInstance<Package>()) {
    //  if (const Any* const actual = findInPackage(name, refType, inst))
    //  {
    //    return actual;
    //  }
  }

  return findInScope(name, refType, scope);
}

const Any* UhdmFinder::findInTask(std::string_view name, RefType refType,
                                  const Task* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const Any* const actual =
                 findInCollection(name, refType, scope->getIODecls(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getVariables(), scope)) {
    return actual;
  } else if (const Package* const p = scope->getInstance<Package>()) {
    if (const Any* const actual = findInPackage(name, refType, p)) {
      return actual;
    }
  }

  return findInScope(name, refType, scope);
}

const Any* UhdmFinder::findInForStmt(std::string_view name, RefType refType,
                                     const ForStmt* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  std::string_view shortName = name;
  if (shortName.find("::") != std::string::npos) {
    std::vector<std::string_view> tokens;
    tokenizeMulti(shortName, "::", tokens);
    if (tokens.size() > 1) shortName = tokens.back();
  }

  if (const AnyCollection* const inits = scope->getForInitStmts()) {
    for (auto init : *inits) {
      if (init->getUhdmType() == UhdmType::AssignStmt) {
        const Expr* const lhs = static_cast<const AssignStmt*>(init)->getLhs();
        if (lhs->getUhdmType() == UhdmType::UnsupportedTypespec) continue;
        if (lhs->getUhdmType() == UhdmType::UnsupportedStmt) continue;
        if (lhs->getUhdmType() == UhdmType::UnsupportedExpr) continue;
        if (lhs->getUhdmType() == UhdmType::VarSelect) continue;
        if (any_cast<RefObj>(lhs) != nullptr) continue;
        if (areSimilarNames(lhs, name)) return lhs;
        if (areSimilarNames(lhs, shortName)) return lhs;
      }
    }
  }

  return findInScope(name, refType, scope);
}

const Any* UhdmFinder::findInForeachStmt(std::string_view name, RefType refType,
                                         const ForeachStmt* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (const Any* const var =
          findInCollection(name, refType, scope->getLoopVars(), scope)) {
    return var;
  }

  return findInScope(name, refType, scope);
}

template <typename T>
const Any* UhdmFinder::findInScope_sub(
    std::string_view name, RefType refType, const T* scope,
    typename std::enable_if<
        std::is_same<Begin, typename std::decay<T>::type>::value ||
        std::is_same<ForkStmt,
                     typename std::decay<T>::type>::value>::type* /* = 0 */) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  std::string_view shortName = name;
  if (shortName.find("::") != std::string::npos) {
    std::vector<std::string_view> tokens;
    tokenizeMulti(shortName, "::", tokens);
    if (tokens.size() > 1) shortName = tokens.back();
  }

  if (areSimilarNames(scope, name) || areSimilarNames(scope, shortName)) {
    return scope;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getVariables(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getParameters(), scope)) {
    return actual;
  }

  if (const AnyCollection* const stmts = scope->getStmts()) {
    for (auto init : *stmts) {
      if (init->getUhdmType() == UhdmType::AssignStmt) {
        const Expr* const lhs = static_cast<const AssignStmt*>(init)->getLhs();
        if (lhs->getUhdmType() == UhdmType::UnsupportedTypespec) continue;
        if (lhs->getUhdmType() == UhdmType::UnsupportedStmt) continue;
        if (lhs->getUhdmType() == UhdmType::UnsupportedExpr) continue;
        if (lhs->getUhdmType() == UhdmType::VarSelect) continue;
        if (any_cast<RefObj>(lhs) != nullptr) continue;
        if (areSimilarNames(lhs, name)) return lhs;
        if (areSimilarNames(lhs, shortName)) return lhs;
      }
    }
  }

  return findInScope(name, refType, scope);
}

const Any* UhdmFinder::findInClassDefn(std::string_view name, RefType refType,
                                       const ClassDefn* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  std::string_view shortName = name;
  if (shortName.find("::") != std::string::npos) {
    std::vector<std::string_view> tokens;
    tokenizeMulti(shortName, "::", tokens);
    if (tokens.size() > 1) shortName = tokens.back();
  }

  if (areSimilarNames(name, "this")) {
    return scope;
  } else if (areSimilarNames(name, "super")) {
    if (const Extends* ext = scope->getExtends()) {
      if (const RefTypespec* rt = ext->getClassTypespec()) {
        if (const ClassTypespec* cts = rt->getActual<ClassTypespec>())
          return cts->getClassDefn();
      }
    }
    return nullptr;
  }

  if (areSimilarNames(scope, name) || areSimilarNames(scope, shortName)) {
    return scope;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getVariables(), scope)) {
    return actual;
  } else if (const Any* const actual =
                 findInCollection(name, refType, scope->getMethods(), scope)) {
    return actual;
  } else if (const Any* const actual =
                 findInScope(name, refType, static_cast<const Scope*>(scope))) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getConstraints(), scope)) {
    return actual;
  } else if (const Extends* ext = scope->getExtends()) {
    if (const RefTypespec* rt = ext->getClassTypespec()) {
      if (const ClassTypespec* cts = rt->getActual<ClassTypespec>()) {
        return findInClassDefn(name, refType, cts->getClassDefn());
      }
    }
  }

  return findInScope(name, refType, scope);
}

const Any* UhdmFinder::findInModule(std::string_view name, RefType refType,
                                    const Module* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(scope, name)) {
    return scope;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getInterfaces(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getInterfaceArrays(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getRefModules(), scope)) {
    return actual;
  } else if (const Any* const actual = findInInstance(name, refType, scope)) {
    return actual;
  } else if (const Any* const actual =
                 findInCollection(name, refType, scope->getPorts(), scope)) {
    return actual;
  }

  return nullptr;
}

const Any* UhdmFinder::findInDesign(std::string_view name, RefType refType,
                                    const Design* scope) {
  if (scope == nullptr) return nullptr;
  if (!m_searched.emplace(scope).second) return nullptr;

  if (areSimilarNames(name, "$root") || areSimilarNames(scope, name)) {
    return scope;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getParameters(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getParamAssigns(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getAllPackages(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getAllModules(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getAllClasses(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getAllInterfaces(), scope)) {
    return actual;
  } else if (const Any* const actual = findInCollection(
                 name, refType, scope->getAllPrograms(), scope)) {
    return actual;
  } else if (const Any* const actual =
                 findInCollection(name, refType, scope->getAllUdps(), scope)) {
    return actual;
  }

  return nullptr;
}

const Any* UhdmFinder::getPrefix(const Any* object) {
  if (object == nullptr) return nullptr;

  const Any* const parent = object->getParent();
  if (parent->getUhdmType() != UhdmType::HierPath) return nullptr;

  const HierPath* const hp = static_cast<const HierPath*>(parent);
  if ((hp->getPathElems() == nullptr) || (hp->getPathElems()->size() < 2))
    return nullptr;

  for (size_t i = 1, n = hp->getPathElems()->size(); i < n; ++i) {
    if (hp->getPathElems()->at(i) == object) {
      const Any* const previous = hp->getPathElems()->at(i - 1);
      if (const RefObj* const ro1 = any_cast<RefObj>(previous)) {
        if (areSimilarNames(ro1, "this") || areSimilarNames(ro1, "super")) {
          const Any* prefix = ro1->getParent();
          while (prefix != nullptr) {
            if (prefix->getUhdmType() == UhdmType::ClassDefn) return prefix;

            prefix = prefix->getParent();
          }
          return prefix;
        }

        if (const ArrayVar* const av = ro1->getActual<ArrayVar>()) {
          if (const RefTypespec* const iod2 = av->getTypespec()) {
            if (const ArrayTypespec* const at =
                    iod2->getActual<ArrayTypespec>()) {
              if (const RefTypespec* const et = at->getElemTypespec()) {
                return et->getActual();
              }
            }
          }
        } else if (const Variables* const var = ro1->getActual<Variables>()) {
          if (const RefTypespec* const iod2 = var->getTypespec()) {
            return iod2->getActual();
          }
        } else if (const IODecl* iod = ro1->getActual<IODecl>()) {
          if (const RefTypespec* const iod2 = iod->getTypespec()) {
            return iod2->getActual();
          }
        } else if (const Parameter* const p1 = ro1->getActual<Parameter>()) {
          if (const RefTypespec* const iod2 = p1->getTypespec()) {
            return iod2->getActual();
          }
        } else if (const Scope* const s = ro1->getActual<Scope>()) {
          return s;
        } else if (const TypespecMember* const tm =
                        ro1->getActual<TypespecMember>()) {
          if (const RefTypespec* const iod2 = tm->getTypespec()) {
            return iod2->getActual();
          }
        } else if (const LogicNet* const ln = ro1->getActual<LogicNet>()) {
          // Ideally logic_net::Typespec should be valid but for
          // too many (or rather most) cases, the Typespec isn't set.
          // So, use the corresponding port in the parent module to
          // find the typespec.

          const Typespec* ts = nullptr;
          if (const RefTypespec* rt = ln->getTypespec()) {
            ts = rt->getActual();
          } else if (const Module* mi = ln->getParent<Module>()) {
            if (mi->getPorts() != nullptr) {
              for (const Port* p2 : *mi->getPorts()) {
                if (const RefObj* ro2 = p2->getLowConn<RefObj>()) {
                  if (ro2->getActual() == ln) {
                    if (const RefTypespec* rt = p2->getTypespec()) {
                      ts = rt->getActual();
                    }
                    break;
                  }
                }
              }
            }
          }

          if (const ClassTypespec* const cts = any_cast<ClassTypespec>(ts)) {
            return cts->getClassDefn();
          } else if (const StructTypespec* const sts =
                          any_cast<StructTypespec>(ts)) {
            return sts;
          }
        } else if (const Port* const p = ro1->getActual<Port>()) {
          if (const RefTypespec* const iod2 = p->getTypespec()) {
            return iod2->getActual();
          }
        }
      }
    }
  }
  return nullptr;
}

// void ObjectBinder::enterChandle_var(const ChandleVar* const object) {
//   if (object->getActual() != nullptr) return;
//
//   if (const Any* actual = bindObject(object)) {
//     const_cast<ChandleVar*>(object)->setActual(
//         const_cast<Any*>(actual));
//   }
// }

const Any* UhdmFinder::find(std::string_view name, RefType refType,
                            const Any* object) {
  m_searched.clear();
  m_searched.emplace(object);  // Prevent returning the same object

  const Any* scope = object;
  if (std::string_view::size_type pos = name.find("::");
      pos != std::string::npos) {
    std::string_view prefixName = name.substr(0, pos);
    std::string_view suffixName = name.substr(pos + 2);

    if (const Package* const p = getPackage(prefixName, scope)) {
      return find(suffixName, refType, p);
    } else if (const ClassDefn* const p = getClassDefn(prefixName, scope)) {
      return find(suffixName, refType, p);
    } else if (const Any* const p =
                   find(prefixName, RefType::Typespec, scope)) {
      return find(suffixName, refType, p);
    } else {
      return nullptr;
    }
  }

  if (const Any* const prefix = getPrefix(scope)) {
    return find(name, refType, prefix);
  }

  while (scope != nullptr) {
    switch (scope->getUhdmType()) {
      case UhdmType::Function: {
        if (const Any* const actual = findInFunction(
                name, refType, static_cast<const Function*>(scope))) {
          return actual;
        }
      } break;

      case UhdmType::Task: {
        if (const Any* const actual =
                findInTask(name, refType, static_cast<const Task*>(scope))) {
          return actual;
        }
      } break;

      case UhdmType::ForStmt: {
        if (const Any* const actual = findInForStmt(
                name, refType, static_cast<const ForStmt*>(scope))) {
          return actual;
        }
      } break;

      case UhdmType::ForeachStmt: {
        if (const Any* const actual = findInForeachStmt(
                name, refType, static_cast<const ForeachStmt*>(scope))) {
          return actual;
        }
      } break;

      case UhdmType::Begin: {
        if (const Any* const actual = findInScope_sub(
                name, refType, static_cast<const Begin*>(scope))) {
          return actual;
        }
      } break;

      case UhdmType::ForkStmt: {
        if (const Any* const actual = findInScope_sub(
                name, refType, static_cast<const ForkStmt*>(scope))) {
          return actual;
        }
      } break;

      case UhdmType::ClassDefn: {
        if (const Any* const actual = findInClassDefn(
                name, refType, static_cast<const ClassDefn*>(scope))) {
          return actual;
        }
      } break;

      case UhdmType::Module: {
        if (const Any* const actual = findInModule(
                name, refType, static_cast<const Module*>(scope))) {
          return actual;
        }
      } break;

      case UhdmType::Interface: {
        if (const Any* const actual = findInInterface(
                name, refType, static_cast<const Interface*>(scope))) {
          return actual;
        }
      } break;

      case UhdmType::Program: {
        if (const Any* const actual = findInProgram(
                name, refType, static_cast<const Program*>(scope))) {
          return actual;
        }
      } break;

      case UhdmType::Package: {
        if (const Any* const actual = findInPackage(
                name, refType, static_cast<const Package*>(scope))) {
          return actual;
        }
      } break;

      case UhdmType::UdpDefn: {
        if (const Any* const actual = findInUdpDefn(
                name, refType, static_cast<const UdpDefn*>(scope))) {
          return actual;
        }
      } break;

      case UhdmType::Design: {
        if (const Any* const actual = findInDesign(
                name, refType, static_cast<const Design*>(scope))) {
          return actual;
        }
      } break;

      default: {
        if (const Typespec* ts = any_cast<Typespec>(scope)) {
          if (const Any* const actual = findInTypespec(name, refType, ts)) {
            return actual;
          }
        }
      } break;
    }

    scope = scope->getParent();
  }

  if (const Any* const p = getPackage("builtin", object)) {
    if (const Any* const actual = find(name, refType, p)) {
      return actual;
    }
  }

  return nullptr;
}

const Any* UhdmFinder::findObject(std::string_view name, const Any* scope) {
  return find(name, RefType::Object, scope);
}

const Any* UhdmFinder::findType(std::string_view name, const Any* scope) {
  return find(name, RefType::Typespec, scope);
}

}  // namespace uhdm
