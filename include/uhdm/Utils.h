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

[[nodiscard]] std::vector<std::string_view> tokenize(
    std::string_view str, std::string_view multichar_separator);

template <typename R = Any, typename T = Any>
constexpr auto getActual(T* object) -> typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
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
constexpr auto getTypespec(T* object) -> typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
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
constexpr auto getElemTypespec(T* typespec) -> typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  if (auto* const at = any_cast<ArrayTypespec>(typespec)) {
    return getActual<R>(at->getElemTypespec());
  }
  return nullptr;
}

bool setElemTypespec(ArrayTypespec* typespec, Typespec* actual);

template <typename R = Typespec, typename T = ArrayTypespec>
constexpr auto getIndexTypespec(T* typespec) -> typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  if (auto* const at = any_cast<ArrayTypespec>(typespec)) {
    return getActual<R>(at->getIndexTypespec());
  }
  return nullptr;
}

bool setIndexTypespec(ArrayTypespec* typespec, Typespec* actual);

template <typename R, typename T = Any>
constexpr auto getParent(T* any) -> typename std::conditional<std::is_const<T>::value, const R*, R*>::type {
  auto* p = any_cast<Any>(any);
  while (p != nullptr) {
    if (auto* const pp = any_cast<R>(p)) {
      return pp;
    }
    p = p->getParent();
  }
  return nullptr;
}

bool getSigned(const Typespec* typespec);
bool setSigned(Typespec* typespec, bool value);

template <typename T = Any>
auto getRanges(T* any) ->
    typename std::conditional<std::is_const<T>::value, const RangeCollection*,
                              RangeCollection*>::type {
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

void prettyPrint(std::ostream& out, const Any* object, size_t indent = 0);
std::string prettyPrint(const Any* object, size_t indent = 0);

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
