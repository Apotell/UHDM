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
 * Author: hs
 *
 * Created on July 3, 2021, 8:03 PM
 */

#ifndef UHDM_EXPREVAL_H
#define UHDM_EXPREVAL_H

#include <uhdm/containers.h>
#include <uhdm/uhdm_forward_decl.h>
#include <uhdm/uhdm_types.h>
#include <uhdm/vpi_user.h>

#include <iostream>
#include <map>
#include <sstream>
#include <string_view>
#include <variant>

namespace uhdm {
class Serializer;

// This UHDM extension offers expression reduction and other utilities that can be operating either:
//   - standalone using UHDM fully elaborated tree
//   - as a utility in a greater context, example: Surelog elaboration

class ObjectProvider {
 public:
  virtual const Any* getObject(std::string_view name, const Any* inst, const Any* pany, bool muteErrors = false) = 0;
  virtual const TaskFunc* getTaskFunc(std::string_view name, const Any* inst, const Any* pany,
                                      bool muteErrors = false) = 0;
  virtual Any* getValue(std::string_view name, const Any* inst, const Any* pany, bool muteErrors = false) = 0;

 public:
  virtual ~ObjectProvider() = default;
  ObjectProvider(const ObjectProvider&) = delete;
  ObjectProvider& operator=(const ObjectProvider&) = delete;

 protected:
  ObjectProvider() = default;
};

class ExprEval final {
 public:
  explicit ExprEval(ObjectProvider* provider, bool muteError = false);

  // Tries to reduce Any expression into a constant, returns the orignal
  // expression if fails. If an invalid value is found in the process,
  // return false.
  [[nodiscard]] bool reduceExpr(const Expr* expr, const Any* pany, Expr** rexpr, bool muteError);
  [[nodiscard]] bool reduceTaggedPattern(const TaggedPattern* tagged, const Any* pany, std::vector<const Any*>* result);
  [[nodiscard]] bool reduceHierPath(const HierPath* hp, const Any* pany, bool returnTypespec, Any** rany,
                                    bool muteError);

  // Computes the size in bits of an object {typespec, var, net, operation...}.
  [[nodiscard]] bool getBitCount(const Any* any, const Any* pany,
                                 bool allRanges,  // false: use only last range size, true: use all ranges
                                 uint64_t* bits, bool muteError = false) const;
  [[nodiscard]] bool getWordSize(const Expr* exp, const Any* pany, uint64_t* wordSize) const;

  [[nodiscard]] bool getInt64(const Expr* expr, int64_t* result, bool strict = true) const;
  [[nodiscard]] bool getUInt64(const Expr* expr, uint64_t* result, bool strict = true) const;
  [[nodiscard]] bool getDouble(const Expr* expr, long double* result, bool strict = true) const;

  void reduceExceptions(const std::vector<int32_t>& operationTypes) { m_skipOperationTypes = operationTypes; }

  [[nodiscard]] Any* getValue(std::string_view name, const Any* inst, const Any* pany, bool muteError = false,
                              const Any* checkLoop = nullptr);

  const Any* getObject(std::string_view name, const Any* inst, const Any* pany, bool muteError = false);

  Expr* flattenPatternAssignments(Serializer& s, const Typespec* tps, Expr* assignExpr);

  void recursiveFlattening(Serializer& s, AnyCollection* flattened, const AnyCollection* ordered,
                           std::vector<const Typespec*> fieldTypes);

  const Any* hierarchicalSelector(std::vector<std::string>& select_path, uint32_t level, const Any* object,
                                  bool& invalidValue, const Any* inst, const Any* pany, bool returnTypespec,
                                  bool muteError = false);

  using Scopes = std::vector<const Instance*>;

  Expr* evalFunc(const Function* func, std::vector<Any*>* args, bool& invalidValue, const Any* inst, const Any* pany,
                 bool muteError = false);

  void evalStmt(std::string_view funcName, Scopes& scopes, bool& invalidValue, bool& continue_flag, bool& break_flag,
                bool& return_flag, const Any* inst, const Any* stmt,
                std::map<std::string, const Typespec*, std::less<>>& local_vars, bool muteError = false);

  bool setValueInInstance(std::string_view lhs, Any* lhsexp, Expr* rhsexp, bool& invalidValue, Serializer& s,
                          const Any* inst, const Any* scope_exp,
                          std::map<std::string, const Typespec*, std::less<>>& local_vars, int32_t opType,
                          bool muteError);
  void setDesign(Design* des) { m_design = des; }

  const TaskFunc* getTaskFunc(std::string_view name, const Any* inst, const Any* pany);

 private:
  void resize(Expr* resizedExp, int32_t size);

  using nvalue_t = std::variant<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t>;
  using rvalue_t = std::variant<float, double>;
  using svalue_t = std::variant<std::string>;
  using value_t = std::variant<std::monostate, nvalue_t, rvalue_t, svalue_t>;

  struct cast_op;
  struct rank_op;
  struct is_signed_op;
  struct is_unsigned_op;
  struct DimInfo;

  static bool isFullySpecified(const Typespec* tps);
  [[nodiscard]] bool getArraySizes(const Typespec* ts, const Any* pany, uint64_t& elemWidth, uint64_t& arrLength);

  template <typename T1, typename T2, typename T3>
  [[nodiscard]] std::variant<std::monostate, T2, T3> parseNumber(const T1* typespec, std::string_view sv,
                                                                 int32_t constType) const;
  template <typename T1, typename T2, typename T3>
  [[nodiscard]] std::variant<std::monostate, T2, T3> parseNumber(const TimeTypespec* typespec, std::string_view sv,
                                                                 int32_t constType) const;
  template <typename T1, typename T2, typename T3>
  [[nodiscard]] std::variant<std::monostate, T2, T3, std::string> parseBinary(const T1* typespec, std::string_view sv,
                                                                              int32_t constType) const;
  [[nodiscard]] std::string parseBinary(const Expr* expr) const;
  [[nodiscard]] value_t parse(const Expr* expr) const;

  bool formatBinary(const Constant* constant, std::string* result) const;
  template <typename T>
  static bool format(T value, int32_t constType, int32_t size, std::string* result);

  [[nodiscard]] value_t promote(const value_t& val, UhdmType targetType, bool isUnsigned) const;
  [[nodiscard]] UhdmType promoted(UhdmType type0, value_t& value0, UhdmType type1, value_t& value1) const;

  template <typename T>
  [[nodiscard]] Constant* createConstant(T value, Serializer& serializer, UhdmType uhdmType, int32_t constType,
                                         int32_t size) const;

  template <typename T>
  [[nodiscard]] bool reduceAny(const Any* any, const Any* pany, T** rT, bool muteError);
  [[nodiscard]] bool reduceCastOp(const Operation* op, const Any* pany, Expr** rexpr);
  template <typename F>
  [[nodiscard]] bool reduceUnaryOp(const Expr* iexpr, const Any* pany, F op, Expr** rexpr);
  template <typename F>
  [[nodiscard]] bool reduceBinaryOp(const Expr* iexpr0, const Expr* iexpr1, const Any* pany, F op, Expr** rexpr);
  [[nodiscard]] bool reduceConcatOp(const AnyCollection& operands, const Any* pany, Expr** rexpr);
  [[nodiscard]] bool reduceReplicationOp(const Expr* iexpr0, const Expr* iexpr1, const Any* pany, Expr** rexpr);
  [[nodiscard]] bool reduceUnaryReplicationOp(const Expr* iexpr0, const Any* pany, Expr** rexpr);
  [[nodiscard]] bool reduceConditionalOp(const Expr* iexpr0, const Expr* iexpr1, const Expr* iexpr2, const Any* pany,
                                         Expr** rexpr);
  [[nodiscard]] bool reduceCaseNeqOp(const Expr* iexpr0, const Expr* iexpr1, const Any* pany, Expr** rexpr);
  [[nodiscard]] bool reduceCaseEqOp(const Expr* iexpr0, const Expr* iexpr1, const Any* pany, Expr** rexpr);
  [[nodiscard]] bool reduceOperation(const Operation* operation, const Any* pany, Expr** rexpr, bool muteError);
  [[nodiscard]] bool reduceSysFuncCall(const SysFuncCall* call, const Any* pany, Expr** rexpr, bool muteError);
  [[nodiscard]] bool reduceFuncCall(const FuncCall* call, const Any* pany, Expr** rexpr, bool muteError);
  [[nodiscard]] bool reduceRefObj(const RefObj* ro, const Any* pany, Expr** rexpr, bool muteError);
  [[nodiscard]] bool reduceBitSelect(const BitSelect* bs, const Any* pany, Expr** rexpr, bool muteError);
  [[nodiscard]] bool selectArrayElement(const Constant* constant, uint32_t index, const Any* pany, Expr** rexpr,
                                        bool muteError);
  [[nodiscard]] bool reducePartSelect(const PartSelect* ps, const Any* pany, Expr** rexpr, bool muteError);
  [[nodiscard]] bool reduceIndexedPartSelect(const IndexedPartSelect* ips, const Any* pany, Expr** rexpr,
                                             bool muteError);
  [[nodiscard]] bool reduceVarSelect(const VarSelect* vs, const Any* pany, Expr** rexpr, bool muteError);
  [[nodiscard]] bool reduceStmt(const Any* s, const Any* pany, Expr** lastValue, bool muteError, bool& break_flag);
  [[nodiscard]] bool reduceParameter(const Parameter* vs, const Any* pany, Expr** rexpr, bool muteError);
  [[nodiscard]] bool reduceEnumConst(const EnumConst* econst, const Any* pany, Expr** rexpr, bool muteError);
  [[nodiscard]] bool reduceVariable(const Variable* var, const Any* pany, Expr** rexpr, bool muteError);
  [[nodiscard]] bool reduceMathSysFunc(const SysFuncCall* call, const Any* pany, Expr** rexpr, bool muteError);
  [[nodiscard]] bool reduceConvSysFunc(const SysFuncCall* call, const Any* pany, Expr** rexpr, bool muteError);
  bool buildCastConstant(const value_t& value, const Typespec* targetTs, Serializer& serializer, Expr** rexpr);
  bool reduceDataQuerySysFunc(const SysFuncCall* call, const Any* pexpr, Expr** rexpr, bool muteError);
  bool collectDimensions(const Typespec* ts, const Any* pexpr, std::vector<DimInfo>& dims);
  bool reduceArrayQuerySysFunc(const SysFuncCall* call, const Any* pexpr, Expr** rexpr, bool muteError);
  bool reduceBitVectorSysFunc(const SysFuncCall* call, const Any* pexpr, Expr** rexpr, bool muteError);

 private:
  ObjectProvider* const m_provider = nullptr;
  std::vector<int32_t> m_skipOperationTypes;
  const Design* m_design = nullptr;
  bool m_muteError = false;
};
}  // namespace uhdm
#endif  // UHDM_EXPREVAL_H
