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
 * File:   UhdmFinder.h
 * Author: Manav Kumar
 *
 * Created on October 04, 2025, 00:00 AM
 */

#ifndef UHDM_UHDMFINDER_H
#define UHDM_UHDMFINDER_H
#pragma once

// uhdm
#include <uhdm/UhdmListener.h>
#include <uhdm/uhdm_forward_decl.h>

#include <map>
#include <set>
#include <string_view>
#include <vector>

namespace uhdm {
class Serializer;

class UhdmFinder final {
 private:
  using Searched = std::set<const Any*>;
  enum class RefType {
    Object,
    Typespec,
  };

 public:
  const Any* findObject(std::string_view name, const Any* object);
  const Any* findType(std::string_view name, const Any* object);

 private:
  bool areSimilarNames(std::string_view name1, std::string_view name2) const;
  bool areSimilarNames(const Any* object1, std::string_view name2) const;
  bool areSimilarNames(const Any* object1, const Any* object2) const;

  const Any* getPrefix(const Any* object);

  template <typename T>
  const T* getParent(const Any* object) const;

  const Package* getPackage(std::string_view name, const Any* object) const;
  const Module* getModule(std::string_view defname, const Any* object) const;
  const Interface* getInterface(std::string_view defname,
                                const Any* object) const;

  const ClassDefn* getClassDefn(const ClassDefnCollection* collection,
                                std::string_view name) const;
  const ClassDefn* getClassDefn(std::string_view name, const Any* object) const;

  const Any* findInTypespec(std::string_view name, RefType refType,
                            const Typespec* scope);
  const Any* findInRefTypespec(std::string_view name, RefType refType,
                               const RefTypespec* scope);
  template <typename T>
  const Any* findInCollection(std::string_view name, RefType refType,
                              const std::vector<T*>* collection,
                              const Any* scope);
  const Any* findInScope(std::string_view name, RefType refType,
                         const Scope* scope);
  const Any* findInInstance(std::string_view name, RefType refType,
                            const Instance* scope);
  const Any* findInInterface(std::string_view name, RefType refType,
                             const Interface* scope);
  const Any* findInPackage(std::string_view name, RefType refType,
                           const Package* scope);
  const Any* findInUdpDefn(std::string_view name, RefType refType,
                           const UdpDefn* scope);
  const Any* findInProgram(std::string_view name, RefType refType,
                           const Program* scope);
  const Any* findInFunction(std::string_view name, RefType refType,
                            const Function* scope);
  const Any* findInTask(std::string_view name, RefType refType,
                        const Task* scope);
  const Any* findInForStmt(std::string_view name, RefType refType,
                           const ForStmt* scope);
  const Any* findInForeachStmt(std::string_view name, RefType refType,
                               const ForeachStmt* scope);
  template <typename T>
  const Any* findInScope_sub(
      std::string_view name, RefType refType, const T* scope,
      typename std::enable_if<
          std::is_same<Begin, typename std::decay<T>::type>::value ||
          std::is_same<ForkStmt, typename std::decay<T>::type>::value>::type* =
          0);
  const Any* findInClassDefn(std::string_view name, RefType refType,
                             const ClassDefn* scope);
  const Any* findInModule(std::string_view name, RefType refType,
                          const Module* scope);
  const Any* findInDesign(std::string_view name, RefType refType,
                          const Design* scope);

  const Any* find(std::string_view name, RefType refType, const Any* scope);

 private:
  Searched m_searched;
};

};  // namespace uhdm

#endif /* UHDM_UHDMFINDER_H */
