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

#include <functional>
#include <iostream>
#include <map>
#include <sstream>

namespace UHDM {
class Serializer;
#ifndef SWIG
typedef std::function<any*(std::string_view name, const any* inst,
                           const any* pexpr)>
    GetObjectFunctor;

typedef std::function<task_func*(std::string_view name, const any* inst)>
    GetTaskFuncFunctor;
#endif
/* This UHDM extension offers expression reduction and other utilities that can
 be operating either:
 - standalone using UHDM fully elaborated tree
 - as a utility in a greater context, example: Surelog elaboration */

class ExprEval {
 public:
  ExprEval(bool muteError = false) : m_muteError(muteError) {}

  enum class ReturnType { VALUE, TYPESPEC, MEMBER };
  typedef std::vector<const instance*> Scopes;

  uint64_t size(
      const any* typespec, bool& invalidValue, const any* inst,
      const any* pexpr,
      bool full /* false: use only last range size, true: use all ranges */,
      bool muteError = false);

  uint64_t size(
      const vpiHandle typespec, bool& invalidValue, const vpiHandle inst,
      const vpiHandle pexpr,
      bool full /* false: use only last range size, true: use all ranges */,
      bool muteError = false);

  expr* reduceExpr(const any* object, bool& invalidValue, const any* inst,
                   const any* pexpr, bool muteErrors = false);

  uint64_t getWordSize(const expr* exp, const any* inst, const any* pexpr);

  uint64_t getValue(const UHDM::expr* expr);

  std::string toBinary(const UHDM::constant* c);

  any* getValue(std::string_view name, const any* inst, const any* pexpr,
                bool muteError = false, const any* checkLoop = nullptr);

  any* getObject(std::string_view name, const any* inst, const any* pexpr,
                 bool muteError = false);

  int64_t get_value(bool& invalidValue, const UHDM::expr* expr,
                    bool strict = true);

  uint64_t get_uvalue(bool& invalidValue, const UHDM::expr* expr,
                      bool strict = true);

  long double get_double(bool& invalidValue, const UHDM::expr* expr);

  expr* flattenPatternAssignments(Serializer& s, const typespec* tps,
                                  expr* assignExpr);

  void prettyPrint(const any* tree, uint32_t indent, std::ostream& out);
  void prettyPrint(std::string_view prefix, const any* tree, std::ostream& out);
  std::string prettyPrint(const UHDM::any* handle);

  expr* reduceCompOp(operation* op, bool& invalidValue, const any* inst,
                     const any* pexpr, bool muteError = false);

  expr* reduceBitSelect(expr* op, uint32_t index_val, bool& invalidValue,
                        const any* inst, const any* pexpr,
                        bool muteError = false);

  void recursiveFlattening(Serializer& s, VectorOfany* flattened,
                           const VectorOfany* ordered,
                           std::vector<const typespec*> fieldTypes);

  any* decodeHierPath(hier_path* path, bool& invalidValue, const any* inst,
                      const any* pexpr, ReturnType returnType,
                      bool muteError = false);

  any* hierarchicalSelector(std::vector<std::string>& select_path,
                            uint32_t level, UHDM::any* object,
                            bool& invalidValue, const any* inst,
                            const UHDM::any* pexpr, ReturnType returnType,
                            bool muteError = false);

  UHDM::expr* evalFunc(UHDM::function* func, std::vector<UHDM::any*>* args,
                       bool& invalidValue, const any* inst, UHDM::any* pexpr,
                       bool muteError = false);

  void evalStmt(std::string_view funcName, Scopes& scopes, bool& invalidValue,
                bool& continue_flag, bool& break_flag, bool& return_flag,
                const any* inst, const UHDM::any* stmt,
                std::map<std::string, const typespec*>& local_vars,
                bool muteError = false);

  bool setValueInInstance(std::string_view lhs, any* lhsexp, expr* rhsexp,
                          bool& invalidValue, Serializer& s, const any* inst,
                          const any* scope_exp,
                          std::map<std::string, const typespec*>& local_vars,
                          int opType, bool muteError);

  UHDM::task_func* getTaskFunc(std::string_view name, const any* inst);

  void setDesign(design* des) { m_design = des; }
  /* For Surelog or other UHDM clients to use the UHDM expr evaluator in their
   * context */
  void setGetObjectFunctor(GetObjectFunctor func) { getObjectFunctor = func; }
  void setGetValueFunctor(GetObjectFunctor func) { getValueFunctor = func; }
  void setGetTaskFuncFunctor(GetTaskFuncFunctor func) {
    getTaskFuncFunctor = func;
  }

  bool isFullySpecified(const typespec* tps);
  void reduceExceptions(const std::vector<int32_t> operationTypes) {
    m_skipOperationTypes = operationTypes;
  }

  std::vector<std::string_view> tokenizeMulti(
      std::string_view str, std::string_view multichar_separator);

 private:
  /* Computes the size in bits of an object {typespec, var, net, operation...}.
   */
  uint64_t _size(
      const any* typespec, bool& invalidValue, const any* inst,
      const any* pexpr,
      bool full /* false: use only last range size, true: use all ranges */,
      bool muteError = false);

  /* Tries to reduce any expression into a constant, returns the orignal
     expression if fails. If an invalid value is found in the process,
     invalidValue will be set to true */
  expr* _reduceExpr(const any* object, bool& invalidValue, const any* inst,
                    const any* pexpr, bool muteErrors = false);

  uint64_t _getWordSize(const expr* exp, const any* inst, const any* pexpr);

  uint64_t _getValue(const UHDM::expr* expr);

  std::string _toBinary(const UHDM::constant* c);

  any* _getValue(std::string_view name, const any* inst, const any* pexpr,
                 bool muteError = false, const any* checkLoop = nullptr);

  any* _getObject(std::string_view name, const any* inst, const any* pexpr,
                  bool muteError = false);

  int64_t _get_value(bool& invalidValue, const UHDM::expr* expr,
                     bool strict = true);

  uint64_t _get_uvalue(bool& invalidValue, const UHDM::expr* expr,
                       bool strict = true);

  long double _get_double(bool& invalidValue, const UHDM::expr* expr);

  expr* _flattenPatternAssignments(Serializer& s, const typespec* tps,
                                   expr* assignExpr);

  expr* _reduceCompOp(operation* op, bool& invalidValue, const any* inst,
                      const any* pexpr, bool muteError = false);

  expr* _reduceBitSelect(expr* op, uint32_t index_val, bool& invalidValue,
                         const any* inst, const any* pexpr,
                         bool muteError = false);

  any* _decodeHierPath(hier_path* path, bool& invalidValue, const any* inst,
                       const any* pexpr, ReturnType returnType,
                       bool muteError = false);

  any* _hierarchicalSelector(std::vector<std::string>& select_path,
                             uint32_t level, UHDM::any* object,
                             bool& invalidValue, const any* inst,
                             const UHDM::any* pexpr, ReturnType returnType,
                             bool muteError = false);

  UHDM::expr* _evalFunc(UHDM::function* func, std::vector<UHDM::any*>* args,
                        bool& invalidValue, const any* inst, UHDM::any* pexpr,
                        bool muteError = false);

  void _evalStmt(std::string_view funcName, Scopes& scopes, bool& invalidValue,
                 bool& continue_flag, bool& break_flag, bool& return_flag,
                 const any* inst, const UHDM::any* stmt,
                 std::map<std::string, const typespec*>& local_vars,
                 bool muteError = false);

  bool _setValueInInstance(std::string_view lhs, any* lhsexp, expr* rhsexp,
                           bool& invalidValue, Serializer& s, const any* inst,
                           const any* scope_exp,
                           std::map<std::string, const typespec*>& local_vars,
                           int opType, bool muteError);

  UHDM::task_func* _getTaskFunc(std::string_view name, const any* inst);

 private:
  GetObjectFunctor getObjectFunctor = nullptr;
  GetObjectFunctor getValueFunctor = nullptr;
  GetTaskFuncFunctor getTaskFuncFunctor = nullptr;
  const UHDM::design* m_design = nullptr;
  bool m_muteError = false;
  std::vector<int32_t> m_skipOperationTypes;
};

#ifndef SWIG
std::string vPrint(UHDM::any* handle);
#endif

}  // namespace UHDM

#endif
