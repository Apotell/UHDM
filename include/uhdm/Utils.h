/*
 Copyright 2020 Alain Dargelas

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
 * File:   Utils.h
 * Author: hs
 *
 * Created on Oct 25, 2025, 02:00 AM
 */

#ifndef UHDM_UTILS_H
#define UHDM_UTILS_H
#pragma once

#include <uhdm/Serializer.h>
#include <uhdm/uhdm.h>

#include <sstream>
#include <type_traits>

namespace uhdm {
// StrCat(): concatenate the string representations of each argument into a string which is returned.
template <typename... Ts>
[[nodiscard]] std::string StrCat(Ts&&... args) {
  std::ostringstream out;
  (out << ... << std::forward<Ts>(args));
  return out.str();
}

// Similar to StrCat(), append arguments, converted to strings to "dest" string.
template <typename... Ts>
void StrAppend(std::string* dest, Ts&&... args) {
  dest->append(StrCat(std::forward<Ts>(args)...));
}

// Remove whitespace at the beginning of the string.
[[nodiscard]] constexpr std::string_view ltrim(std::string_view str) {
  while (!str.empty() && std::isspace(str.front())) str.remove_prefix(1);
  return str;
}

// Remove whitespace at the end of the string.
[[nodiscard]] constexpr std::string_view rtrim(std::string_view str) {
  while (!str.empty() && std::isspace(str.back())) str.remove_suffix(1);
  return str;
}

// Removing spaces on both ends.
[[nodiscard]] constexpr std::string_view trim(std::string_view str) { return ltrim(rtrim(str)); }

// Erase left of the input string until given character is reached.
[[nodiscard]] constexpr std::string_view ltrim_until(std::string_view str, char c) {
  auto pos = str.find(c);
  if (pos != std::string_view::npos) str = str.substr(pos + 1);
  return str;
}

// Erase right of the input string until given character is reached.
[[nodiscard]] constexpr std::string_view rtrim_until(std::string_view str, char c) {
  auto pos = str.rfind(c);
  if (pos != std::string_view::npos) str = str.substr(0, pos);
  return str;
}

// Erase from left and right of the input string until the given character is
// reached.
[[nodiscard]] constexpr std::string_view trim_until(std::string_view str, char c) {
  return ltrim_until(rtrim_until(str, c), c);
}

[[nodiscard]] std::vector<std::string_view> tokenize(std::string_view str, std::string_view multichar_separator);

template <typename R = Any, typename T = Any>
[[nodiscard]] constexpr auto getActual(T* object) ->
    typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  if (object == nullptr) return nullptr;

  if (auto* const ro = any_cast<RefObj>(object)) {
    return ro->template getActual<R>();
  } else if (auto* const cb = any_cast<ClockingBlock>(object)) {
    return cb->template getActual<R>();
  } else if (auto* const hp = any_cast<HierPath>(object)) {
    if (auto* const pe = hp->getPathElems()) {
      if (!pe->empty()) return getActual<R>(pe->back());
    }
  } else if (auto* const rm = any_cast<RefModule>(object)) {
    return rm->template getActual<R>();
  } else if (auto* const rt = any_cast<RefTypespec>(object)) {
    return rt->template getActual<R>();
  }

  return nullptr;
}

template <typename T>
bool setActual(uhdm::Any* object, T* actual) {
  if (object == nullptr) return false;

  if (RefObj* const ro = any_cast<RefObj>(object)) {
    return ro->setActual(actual);
  } else if (ClockingBlock* const cb = any_cast<ClockingBlock>(object)) {
    return cb->setActual(any_cast<ClockingBlock>(actual));
  } else if (RefModule* const rm = any_cast<RefModule>(object)) {
    return rm->setActual(actual);
  } else if (RefTypespec* const rt = any_cast<RefTypespec>(object)) {
    return rt->setActual(any_cast<Typespec>(actual));
  }

  return false;
}

template <typename R = Typespec, typename T = Any>
[[nodiscard]] constexpr auto getTypespec(T* object) ->
    typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  if (object == nullptr) return nullptr;

  if (auto* const e = any_cast<Expr>(object)) {
    if (auto* const rt = e->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const iod = any_cast<IODecl>(object)) {
    if (auto* const rt = iod->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const ne = any_cast<NamedEvent>(object)) {
    if (auto* const rt = ne->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const p = any_cast<Ports>(object)) {
    if (auto* const rt = p->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const pfd = any_cast<PropFormalDecl>(object)) {
    if (auto* const rt = pfd->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const sfd = any_cast<SeqFormalDecl>(object)) {
    if (auto* const rt = sfd->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const tp = any_cast<TaggedPattern>(object)) {
    if (auto* const rt = tp->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const tm = any_cast<TypespecMember>(object)) {
    if (auto* const rt = tm->getTypespec()) {
      return rt->template getActual<R>();
    }
  } else if (auto* const tp = any_cast<TypeParameter>(object)) {
    if (auto* const rt = tp->getTypespec()) {
      return rt->template getActual<R>();
    }
  }

  return nullptr;
}

bool setTypespec(Any* object, Typespec* typespec);

template <typename R = Typespec, typename T = ArrayTypespec>
[[nodiscard]] constexpr auto getElemTypespec(T* typespec) ->
    typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  if (auto* const at = any_cast<ArrayTypespec>(typespec)) {
    return getActual<R>(at->getElemTypespec());
  }
  return nullptr;
}

bool setElemTypespec(ArrayTypespec* typespec, Typespec* actual);

template <typename R = Typespec, typename T = ArrayTypespec>
[[nodiscard]] constexpr auto getIndexTypespec(T* typespec) ->
    typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  if (auto* const at = any_cast<ArrayTypespec>(typespec)) {
    return getActual<R>(at->getIndexTypespec());
  }
  return nullptr;
}

bool setIndexTypespec(ArrayTypespec* typespec, Typespec* actual);

[[nodiscard]] inline bool isVectorType(const Typespec* typespec) {
  switch (typespec->getUhdmType()) {
    case UhdmType::BitTypespec:
    case UhdmType::IntegerTypespec:
    case UhdmType::LogicTypespec: return true;
    default: return false;
  }
}

[[nodiscard]] inline bool isNumericType(const Typespec* typespec) {
  switch (typespec->getUhdmType()) {
    case UhdmType::ByteTypespec:
    case UhdmType::IntTypespec:
    case UhdmType::LongIntTypespec:
    case UhdmType::RealTypespec:
    case UhdmType::ShortIntTypespec:
    case UhdmType::ShortRealTypespec: return true;
    default: return false;
  }
}

[[nodiscard]] inline bool isBuiltinTypespec(const Typespec* typespec) {
  switch (typespec->getUhdmType()) {
    case UhdmType::BitTypespec:
    case UhdmType::ByteTypespec:
    case UhdmType::ChandleTypespec:
    case UhdmType::IntTypespec:
    case UhdmType::IntegerTypespec:
    case UhdmType::LogicTypespec:
    case UhdmType::LongIntTypespec:
    case UhdmType::RealTypespec:
    case UhdmType::ShortIntTypespec:
    case UhdmType::ShortRealTypespec:
    case UhdmType::StringTypespec:
    case UhdmType::TimeTypespec:
    case UhdmType::VoidTypespec: return true;
    default: return false;
  }
}

template <typename R, typename T = Any>
[[nodiscard]] constexpr auto getParent(T* any) ->
    typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  auto* p = any_cast<Any>(any);
  while (p != nullptr) {
    if (auto* const pp = any_cast<R>(p)) {
      return pp;
    }
    p = p->getParent();
  }
  return nullptr;
}

[[nodiscard]] bool getSigned(const Typespec* typespec);
bool setSigned(Typespec* typespec, bool value);

template <typename T = Any>
[[nodiscard]] auto getRanges(T* any) ->
    typename std::conditional<std::is_const<T>::value, const RangeCollection*, RangeCollection*>::type {
  if (auto* const at = any_cast<ArrayTypespec>(any)) {
    return at->getRanges();
  } else if (auto* const bt = any_cast<BitTypespec>(any)) {
    return bt->getRanges();
  } else if (auto* const ia = any_cast<InstanceArray>(any)) {
    return ia->getRanges();
  } else if (auto* const iod = any_cast<IODecl>(any)) {
    return iod->getRanges();
  } else if (auto* const lt = any_cast<LogicTypespec>(any)) {
    return lt->getRanges();
  } else if (auto* const p = any_cast<Parameter>(any)) {
    return p->getRanges();
  } else if (auto* const ut = any_cast<UnsupportedTypespec>(any)) {
    return ut->getRanges();
  }
  return nullptr;
}

[[nodiscard]] constexpr std::string_view getOperationName(int32_t type) {
  switch (type) {
    case vpiMinusOp /* = 1 */: return "MinusOp";
    case vpiPlusOp /* = 2 */: return "PlusOp";
    case vpiNotOp /* = 3 */: return "NotOp";
    case vpiBitNegOp /* = 4 */: return "BitNegOp";
    case vpiUnaryAndOp /* = 5 */: return "UnaryAndOp";
    case vpiUnaryNandOp /* = 6 */: return "UnaryNandOp";
    case vpiUnaryOrOp /* = 7 */: return "UnaryOrOp";
    case vpiUnaryNorOp /* = 8 */: return "UnaryNorOp";
    case vpiUnaryXorOp /* = 9 */: return "UnaryXorOp";
    case vpiUnaryXNorOp /* = 10 */: return "UnaryXNorOp";
    case vpiSubOp /* = 11 */: return "SubOp";
    case vpiDivOp /* = 12 */: return "DivOp";
    case vpiModOp /* = 13 */: return "ModOp";
    case vpiEqOp /* = 14 */: return "EqOp";
    case vpiNeqOp /* = 15 */: return "NeqOp";
    case vpiCaseEqOp /* = 16 */: return "CaseEqOp";
    case vpiCaseNeqOp /* = 17 */: return "CaseNeqOp";
    case vpiGtOp /* = 18 */: return "GtOp";
    case vpiGeOp /* = 19 */: return "GeOp";
    case vpiLtOp /* = 20 */: return "LtOp";
    case vpiLeOp /* = 21 */: return "LeOp";
    case vpiLShiftOp /* = 22 */: return "LShiftOp";
    case vpiRShiftOp /* = 23 */: return "RShiftOp";
    case vpiAddOp /* = 24 */: return "AddOp";
    case vpiMultOp /* = 25 */: return "MultOp";
    case vpiLogAndOp /* = 26 */: return "LogAndOp";
    case vpiLogOrOp /* = 27 */: return "LogOrOp";
    case vpiBitAndOp /* = 28 */: return "BitAndOp";
    case vpiBitOrOp /* = 29 */: return "BitOrOp";
    case vpiBitXorOp /* = 30 */: return "BitXorOp";
    case vpiBitXNorOp /* = 31 */: return "BitXNorOp";
    case vpiConditionOp /* = 32 */: return "ConditionOp";
    case vpiConcatOp /* = 33 */: return "ConcatOp";
    case vpiMultiConcatOp /* = 34 */: return "MultiConcatOp";
    case vpiEventOrOp /* = 35 */: return "EventOrOp";
    case vpiNullOp /* = 36 */: return "NullOp";
    case vpiListOp /* = 37 */: return "ListOp";
    case vpiMinTypMaxOp /* = 38 */: return "MinTypMaxOp";
    case vpiPosedgeOp /* = 39 */: return "PosedgeOp";
    case vpiNegedgeOp /* = 40 */: return "NegedgeOp";
    case vpiArithLShiftOp /* = 41 */: return "ArithLShiftOp";
    case vpiArithRShiftOp /* = 42 */: return "ArithRShiftOp";
    case vpiPowerOp /* = 43 */: return "PowerOp";

    case vpiImplyOp /* = 50 */: return "ImplyOp";
    case vpiNonOverlapImplyOp /* = 51 */: return "NonOverlapImplyOp";
    case vpiOverlapImplyOp /* = 52 */: return "OverlapImplyOp";
    case vpiUnaryCycleDelayOp /* = 53 */: return "UnaryCycleDelayOp";
    case vpiCycleDelayOp /* = 54 */: return "CycleDelayOp";
    case vpiIntersectOp /* = 55 */: return "IntersectOp";
    case vpiFirstMatchOp /* = 56 */: return "FirstMatchOp";
    case vpiThroughoutOp /* = 57 */: return "ThroughoutOp";
    case vpiWithinOp /* = 58 */: return "WithinOp";
    case vpiRepeatOp /* = 59 */: return "RepeatOp";
    case vpiConsecutiveRepeatOp /* = 60 */: return "ConsecutiveRepeatOp";
    case vpiGotoRepeatOp /* = 61 */: return "GotoRepeatOp";

    case vpiPostIncOp /* = 62 */: return "PostIncOp";
    case vpiPreIncOp /* = 63 */: return "PreIncOp";
    case vpiPostDecOp /* = 64 */: return "PostDecOp";
    case vpiPreDecOp /* = 65 */: return "PreDecOp";

    case vpiMatchOp /* = 66 */: return "MatchOp";
    case vpiCastOp /* = 67 */: return "CastOp";
    case vpiIffOp /* = 68 */: return "IffOp";
    case vpiWildEqOp /* = 69 */: return "WildEqOp";
    case vpiWildNeqOp /* = 70 */: return "WildNeqOp";

    case vpiStreamLROp /* = 71 */: return "StreamLROp";
    case vpiStreamRLOp /* = 72 */: return "StreamRLOp";

    case vpiMatchedOp /* = 73 */: return "MatchedOp";
    case vpiTriggeredOp /* = 74 */: return "TriggeredOp";
    case vpiAssignmentPatternOp /* = 75 */: return "AssignmentPatternOp";
    case vpiMultiAssignmentPatternOp /* = 76 */: return "MultiAssignmentPatternOp";
    case vpiIfOp /* = 77 */: return "IfOp";
    case vpiIfElseOp /* = 78 */: return "IfElseOp";
    case vpiCompAndOp /* = 79 */: return "CompAndOp";
    case vpiCompOrOp /* = 80 */: return "CompOrOp";
    case vpiTypeOp /* = 81 */: return "TypeOp";
    case vpiAssignmentOp /* = 82 */: return "AssignmentOp";

    case vpiAcceptOnOp /* = 83 */: return "AcceptOnOp";
    case vpiRejectOnOp /* = 84 */: return "RejectOnOp";
    case vpiSyncAcceptOnOp /* = 85 */: return "SyncAcceptOnOp";
    case vpiSyncRejectOnOp /* = 86 */: return "SyncRejectOnOp";
    case vpiOverlapFollowedByOp /* = 87 */: return "OverlapFollowedByOp";
    case vpiNonOverlapFollowedByOp /* = 88 */: return "NonOverlapFollowedByOp";
    case vpiNexttimeOp /* = 89 */: return "NexttimeOp";
    case vpiAlwaysOp /* = 90 */: return "AlwaysOp";
    case vpiEventuallyOp /* = 91 */: return "EventuallyOp";
    case vpiUntilOp /* = 92 */: return "UntilOp";
    case vpiUntilWithOp /* = 93 */: return "UntilWithOp";
    case vpiImpliesOp /* = 94 */: return "ImpliesOp";
    case vpiInsideOp /* = 95 */: return "InsideOp";

    default: return "<unknown>";
  }
}

[[nodiscard]] constexpr bool isConvSysFunc(std::string_view name) {
  return (name == "$rtoi") || (name == "$itor") || (name == "$signed") || (name == "$unsigned") ||
         (name == "$realtobits") || (name == "$bitstoreal") || (name == "$shortrealtobits") || (name == "$cast") ||
         (name == "$bitstoshortreal");
}

[[nodiscard]] constexpr bool isMathSysFunc(std::string_view name) {
  return (name == "$clog2") || (name == "$asin") || (name == "$acos") || (name == "$atan") || (name == "$ln") ||
         (name == "$log10") || (name == "$exp") || (name == "$sqrt") || (name == "$floor") || (name == "$ceil") ||
         (name == "$sin") || (name == "$cos") || (name == "$tan") || (name == "$sinh") || (name == "$cosh") ||
         (name == "$tanh") || (name == "$asinh") || (name == "$acosh") || (name == "$atanh") || (name == "$atan2") ||
         (name == "$hypot") || (name == "$pow");
}
[[nodiscard]] constexpr bool isDataQuerySysFunc(std::string_view name) {
  return (name == "$bits") || (name == "$isunbounded") || (name == "$typename");
}

[[nodiscard]] constexpr bool isArrayQuerySysFunc(std::string_view name) {
  return (name == "$unpacked_dimensions") || (name == "$dimensions") || (name == "$left") || (name == "$right") ||
         (name == "$low") || (name == "$high") || (name == "$increment") || (name == "$size");
}

[[nodiscard]] constexpr bool isBitVectorSysFunc(std::string_view name) {
  return (name == "$countbits") || (name == "$onehot") || (name == "$isunknown") || (name == "$countones") ||
         (name == "$onehot0");
}

void prettyPrint(std::ostream& out, const Any* object, size_t indent = 0);
[[nodiscard]] std::string prettyPrint(const Any* object, size_t indent = 0);

template <typename T>
void prettyPrint(std::ostream& out, const std::vector<T*>* collection, std::string_view separator = ", ",
                 size_t indent = 0) {
  if (collection == nullptr) return;

  if (indent > 0) out << std::string(indent, ' ');
  for (const T* any : *collection) {
    prettyPrint(out, any, indent);
    if (any != collection->back()) out << separator;
  }
}
}  // namespace uhdm

#endif  // UHDM_UTILS_H
