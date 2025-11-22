// -*- c++ -*-

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
 * File:   Serializer.h
 * Author:
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef UHDM_SERIALIZER_H
#define UHDM_SERIALIZER_H

#include <uhdm/SymbolFactory.h>
#include <uhdm/containers.h>
#include <uhdm/vpi_uhdm.h>
#include <uhdm/uhdm_types.h>
#include <uhdm/uhdm.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#define UHDM_MAX_BIT_WIDTH (1024 * 1024)

namespace UHDM {
enum ErrorType {
  UHDM_UNSUPPORTED_EXPR = 700,
  UHDM_UNSUPPORTED_STMT = 701,
  UHDM_WRONG_OBJECT_TYPE = 703,
  UHDM_UNDEFINED_PATTERN_KEY = 712,
  UHDM_UNMATCHED_FIELD_IN_PATTERN_ASSIGN = 713,
  UHDM_REAL_TYPE_AS_SELECT = 714,
  UHDM_RETURN_VALUE_VOID_FUNCTION = 715,
  UHDM_ILLEGAL_DEFAULT_VALUE = 716,
  UHDM_MULTIPLE_CONT_ASSIGN = 717,
  UHDM_ILLEGAL_WIRE_LHS = 718,
  UHDM_ILLEGAL_PACKED_DIMENSION = 719,
  UHDM_NON_SYNTHESIZABLE = 720,
  UHDM_ENUM_CONST_SIZE_MISMATCH = 721,
  UHDM_DIVIDE_BY_ZERO = 722,
  UHDM_INTERNAL_ERROR_OUT_OF_BOUND = 723,
  UHDM_UNDEFINED_USER_FUNCTION = 724,
  UHDM_UNRESOLVED_HIER_PATH = 725,
  UHDM_UNDEFINED_VARIABLE = 726,
  UHDM_INVALID_CASE_STMT_VALUE = 727,
  UHDM_UNSUPPORTED_TYPESPEC = 728,
  UHDM_UNRESOLVED_PROPERTY = 729,
  UHDM_NON_TEMPORAL_SEQUENCE_USE = 730,
  UHDM_NON_POSITIVE_VALUE = 731,
  UHDM_SIGNED_UNSIGNED_PORT_CONN = 732,
  UHDM_FORCING_UNSIGNED_TYPE = 733
};

#ifndef SWIG
typedef std::function<void(ErrorType errType, const std::string&,
                           const any* object1, const any* object2)>
    ErrorHandler;

void DefaultErrorHandler(ErrorType errType, const std::string& errorMsg,
                         const any* object1, const any* object2);

class Factory final {
  friend Serializer;

  using objects_t = std::vector<any*>;
  using collections_t = std::vector<objects_t*>;

 public:
  template <typename T>
  T* Make() {
    T* const any = new T;
    m_objects.emplace_back(any);
    return any;
  }

  template <typename T>
  std::vector<T*>* MakeVec() {
    std::vector<T*>* const collection = new std::vector<T*>;
    m_collections.emplace_back((objects_t*)collection);
    return collection;
  }

  bool Erase(const any* obj) {
    objects_t::iterator it = std::find(m_objects.begin(), m_objects.end(), obj);
    if (it != m_objects.end()) {
      delete obj;
      m_objects.erase(it);
      return true;
    }
    return false;
  }

  template <typename T>
  bool Erase(const std::vector<T*>* collection) {
    collections_t::iterator it =
        std::find(m_collections.begin(), m_collections.end(),
                  static_cast<const collections_t*>(collection));
    if (it != m_collections.end()) {
      delete collection;
      m_collections.erase(it);
      return true;
    }
    return false;
  }

  void EraseIfNotIn(const AnySet& container, AnySet& erased) {
    objects_t keepers;
    for (objects_t::reference obj : m_objects) {
      if (container.find(obj) == container.cend()) {
        erased.emplace(obj);
        delete obj;
      } else {
        keepers.emplace_back(obj);
      }
    }
    keepers.swap(m_objects);
  }

  void MapToIndex(std::map<const any*, uint32_t>& table,
                  uint32_t index = 1) const {
    for (objects_t::const_reference obj : m_objects) {
      table.emplace(obj, index++);
    }
  }

  void Purge() {
    for (objects_t::reference obj : m_objects) {
      delete obj;
    }
    for (collections_t::reference collection : m_collections) {
      delete collection;
    }

    m_objects.clear();
    m_collections.clear();
  }

  const objects_t& getObjects() { return m_objects; }
  const objects_t& getObjects() const { return m_objects; }

  const collections_t& getCollections() { return m_collections; }
  const collections_t& getCollections() const { return m_collections; }

 private:
  objects_t m_objects;
  collections_t m_collections;
};

#endif

class Serializer final {
 public:
  using IdMap = std::map<const BaseClass*, uint32_t>;
  static constexpr uint32_t kBadIndex = static_cast<uint32_t>(-1);
  static const uint32_t kVersion;

  Serializer() = default;
  ~Serializer();

#ifndef SWIG
  void Save(const std::filesystem::path& filepath);
  void Save(const std::string& filepath);
  void Purge();

  void SetGCEnabled(bool enabled) { m_enableGC = enabled; }
  void GarbageCollect();

  void SetErrorHandler(ErrorHandler handler) { m_errorHandler = handler; }
  ErrorHandler GetErrorHandler() { return m_errorHandler; }

  IdMap AllObjects() const;
#endif

  const std::vector<vpiHandle> Restore(const std::filesystem::path& filepath);
  const std::vector<vpiHandle> Restore(const std::string& filepath);
  std::map<std::string, uint32_t, std::less<>> ObjectStats() const;
  void PrintStats(std::ostream& strm, std::string_view infoText) const;

#ifndef SWIG
 private:
  template <typename T>
  T* Make(Factory* factory);

  template <typename T>
  void Make(Factory* factory, uint32_t count);

  template <typename T>
  std::vector<T*>* MakeVec(Factory* factory) {
    return factory->MakeVec<T>();
  }

 public:
<FACTORY_FUNCTION_DECLARATIONS>
  std::vector<any*>* MakeAnyVec() {
    return MakeVec<any>(&anyMaker);
  }

  SymbolId MakeSymbol(std::string_view symbol);
  std::string_view GetSymbol(SymbolId id) const;
  SymbolId GetSymbolId(std::string_view symbol) const;

  vpiHandle MakeUhdmHandle(UHDM_OBJECT_TYPE type, const void* object);

  template <typename T>
  T* Clone(const T* source);

  template <typename T>
  std::vector<T*>* MakeVec();

  bool Erase(const BaseClass* p);

 private:
  struct SaveAdapter;
  friend struct SaveAdapter;

  struct RestoreAdapter;
  friend struct RestoreAdapter;

 private:
  BaseClass* GetObject(uint32_t objectType, uint32_t index) const;

  uint64_t m_version = 0;
  uint32_t m_objId = 0;
  bool m_enableGC = true;
  ErrorHandler m_errorHandler = DefaultErrorHandler;

  Factory anyMaker;
  SymbolFactory symbolMaker;
  uhdm_handleFactory uhdm_handleMaker;
<FACTORY_DATA_MEMBERS>
#endif
};

template <typename T>
T* Serializer::Clone(const T* source) {
  T* target = nullptr;
  // clang-format off
  switch (T::kUhdmType) {
//<FACTORY_MAKE_CASE_STATEMENTS>
    default: return nullptr;
  }
  // clang-format on
  *target = *source;
  target->SetSerializer(this);
  target->UhdmId(++m_objId);
  return target;
}

template <typename T>
std::vector<T*>* Serializer::MakeVec() {
  // clang-format off
  switch (T::kUhdmType) {
    case BaseClass::kUhdmType: return anyMaker.template MakeVec<T>();
//<FACTORY_MAKEVEC_CASE_STATEMENTS>
    default: return nullptr;
  }
  // clang-format on
}
}  // namespace UHDM

#endif
