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
 * File:   vpi_uhdm.h
 * Author:
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef UHDM_VPI_UHDM_H
#define UHDM_VPI_UHDM_H

#include <uhdm/uhdm_types.h>
#include <uhdm/BaseClass.h>

#include <string>
#include <string_view>

namespace uhdm {
  class BaseClass;
  class Design;
  class Serializer;
};  // namespace uhdm

struct UhdmHandle final {
  explicit UhdmHandle(const uhdm::BaseClass* object, uint32_t index = 0)
      : UhdmHandle(object->getUhdmType(), object, index) {}
  UhdmHandle(uhdm::UhdmType type, const void* object, uint32_t index = 0)
      : type(type), object(object), index(index) {}

  const uhdm::UhdmType type = uhdm::UhdmType::Any;
  const void* const object = nullptr;
  uint32_t index = 0;
};

class UhdmHandleFactory final {
 public:
  UhdmHandle* make(const uhdm::BaseClass* object, uint32_t index = 0) {
    return new UhdmHandle(object, index);
  }

  UhdmHandle* make(uhdm::UhdmType type, const void* object, uint32_t index = 0) {
    return new UhdmHandle(type, object, index);
  }

  bool erase(UhdmHandle* handle) {
    delete handle;
    return true;
  }

  void purge() {}
};

class ScopedUhdmHandle final {
 public:
  explicit ScopedUhdmHandle(const uhdm::BaseClass* object, uint32_t index = 0)
      : m_handle(new UhdmHandle(object, index)) {}
  ~ScopedUhdmHandle() {
    if (m_handle != nullptr) delete m_handle;
  }

  operator vpiHandle() const { return (vpiHandle)m_handle; }

 private:
  const UhdmHandle* const m_handle = nullptr;
};
using ScopedVpiHandle = ScopedUhdmHandle;

/** Obtain a vpiHandle from a BaseClass (any) object */
vpiHandle NewVpiHandle(const uhdm::BaseClass* object);

void String2VpiValue(std::string_view sv, int32_t constType, s_vpi_value* value);
void VpiDestroyValue(s_vpi_value& value);

void String2VpiDelay(std::string_view sv, s_vpi_delay* delay);
void VpiDestroyDelay(s_vpi_delay& delay);

std::string VpiValue2String(const s_vpi_value* value);

std::string VpiDelay2String(const s_vpi_delay* delay);

/** Obtain a uhdm::design pointer from a vpiHandle */
uhdm::Design* UhdmDesignFromVpiHandle(vpiHandle hdesign);

/** Shows unique IDs in vpi_visitor dump (uhdmdump) */
void vpi_show_ids(bool show);

#endif  // UHDM_VPI_UHDM_H
