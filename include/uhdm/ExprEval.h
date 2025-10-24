// -*- c++ -*-

/*

 Copyright 2019-2022 Alain Dargelas

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
 * File:   ExprEval.h
 * Author: alaindargelas
 *
 * Created on July 3, 2021, 8:03 PM
 */

#ifndef UHDM_EXPREVAL_H
#define UHDM_EXPREVAL_H

#include <uhdm/containers.h>
#include <uhdm/uhdm_forward_decl.h>
#include <uhdm/vpi_user.h>

#include <iostream>
#include <map>
#include <sstream>
#include <string_view>

namespace uhdm {
class Serializer;

/* This UHDM extension offers expression reduction and other utilities that can
 be operating either:
 - standalone using UHDM fully elaborated tree
 - as a utility in a greater context, example: Surelog elaboration */

class ObjectProvider {
 public:
  virtual const Any* getObject(std::string_view name, const Any* inst,
                               const Any* pexpr, bool muteErrors = false) = 0;
  virtual const TaskFunc* getTaskFunc(std::string_view name, const Any* inst,
                                      const Any* pexpr,
                                      bool muteErrors = false) = 0;
  virtual Any* getValue(std::string_view name, const Any* inst,
                        const Any* pexpr, bool muteErrors = false) = 0;

 public:
  virtual ~ObjectProvider() = default;
  ObjectProvider(const ObjectProvider&) = delete;
  ObjectProvider& operator=(const ObjectProvider&) = delete;

 protected:
  ObjectProvider() = default;
};

class ExprEval {
 public:
  ExprEval(ObjectProvider* provider, bool muteError = false)
      : m_provider(provider), m_muteError(muteError) {}
#ifndef SWIG
  static bool isFullySpecified(const Typespec* tps);
  /* Computes the size in bits of an object {typespec, var, net, operation...}.
   */
  uint64_t size(
      const Any* typespec, bool& invalidValue, const Any* inst,
      const Any* pexpr,
      bool full /* false: use only last range size, true: use all ranges */,
      bool muteError = false);
#endif

  uint64_t size(
      const vpiHandle typespec, bool& invalidValue, const vpiHandle inst,
      const vpiHandle pexpr,
      bool full /* false: use only last range size, true: use all ranges */,
      bool muteError = false);

#ifndef SWIG
  void reduceExceptions(const std::vector<int32_t> &operationTypes) {
    m_skipOperationTypes = operationTypes;
  }
  /* Tries to reduce Any expression into a constant, returns the orignal
     expression if fails. If an invalid value is found in the process,
     invalidValue will be set to true */
  Expr* reduceExpr(const Any* object, bool& invalidValue, const Any* inst,
                   const Any* pexpr, bool muteErrors = false);

  uint64_t getWordSize(const Expr* exp, const Any* inst, const Any* pexpr);

  uint64_t getValue(const Expr* expr);

  std::string toBinary(const Constant* c) const;

  Any* getValue(std::string_view name, const Any* inst, const Any* pexpr,
                bool muteError = false, const Any* checkLoop = nullptr);

  const Any* getObject(std::string_view name, const Any* inst, const Any* pexpr,
                       bool muteError = false);

  int64_t get_value(bool& invalidValue, const Expr* expr, bool strict = true);

  uint64_t get_uvalue(bool& invalidValue, const Expr* expr, bool strict = true);

  long double get_double(bool& invalidValue, const Expr* expr);

  Expr* flattenPatternAssignments(Serializer& s, const Typespec* tps,
                                  Expr* assignExpr);

  Expr* reduceCompOp(Operation* op, bool& invalidValue, const Any* inst,
                     const Any* pexpr, bool muteError = false);

  Expr* reduceBitSelect(const Expr* op, uint32_t index_val, bool& invalidValue,
                        const Any* inst, const Any* pexpr,
                        bool muteError = false);

  void recursiveFlattening(Serializer& s, AnyCollection* flattened,
                           const AnyCollection* ordered,
                           std::vector<const Typespec*> fieldTypes);

  Any* decodeHierPath(HierPath* path, bool& invalidValue, const Any* inst,
                      const Any* pexpr, bool returnTypespec,
                      bool muteError = false);

  const Any* hierarchicalSelector(std::vector<std::string>& select_path,
                                  uint32_t level, const Any* object,
                                  bool& invalidValue, const Any* inst,
                                  const Any* pexpr, bool returnTypespec,
                                  bool muteError = false);

  using Scopes = std::vector<const Instance*>;

  Expr* evalFunc(const Function* func, std::vector<Any*>* args,
                 bool& invalidValue, const Any* inst, Any* pexpr,
                 bool muteError = false);

  void evalStmt(std::string_view funcName, Scopes& scopes, bool& invalidValue,
                bool& continue_flag, bool& break_flag, bool& return_flag,
                const Any* inst, const Any* stmt,
                std::map<std::string, const Typespec*>& local_vars,
                bool muteError = false);

  bool setValueInInstance(std::string_view lhs, Any* lhsexp, Expr* rhsexp,
                          bool& invalidValue, Serializer& s, const Any* inst,
                          const Any* scope_exp,
                          std::map<std::string, const Typespec*>& local_vars,
                          int opType, bool muteError);
  void setDesign(Design* des) { m_design = des; }

  const TaskFunc* getTaskFunc(std::string_view name, const Any* inst,
                              const Any* pexpr);

  std::vector<std::string_view> tokenizeMulti(
      std::string_view str, std::string_view multichar_separator);
#endif

  static void prettyPrint(const Any* tree, uint32_t indent, std::ostream& out);
  static std::string prettyPrint(const Any* handle);

 private:
  void resize(Expr* resizedExp, int32_t size);

 private:
  ObjectProvider* const m_provider = nullptr;
  const Design* m_design = nullptr;
  bool m_muteError = false;
  std::vector<int32_t> m_skipOperationTypes;
};

#ifndef SWIG
std::string vPrint(Any* handle);
#endif

}  // namespace uhdm

#endif  // UHDM_EXPREVAL_H
