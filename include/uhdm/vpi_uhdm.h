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

#include <string>
#include <string_view>

namespace uhdm {
  class BaseClass;
  class Design;
  class Serializer;
};  // namespace uhdm

struct uhdm_handle final {
  uhdm_handle(uhdm::UhdmType type, const void* object) :
    type(type), object(object), index(0) {}
  const uhdm::UhdmType type;
  const void* object;
  uint32_t index;
};

class UhdmHandleFactory final {
  friend uhdm::Serializer;

 public:
  vpiHandle make(uhdm::UhdmType type, const void* object) {
    return (vpiHandle) new uhdm_handle(type, object);
  }

  bool erase(vpiHandle handle) {
    delete (uhdm_handle*)handle;
    return true;
  }

  void purge() {}
};

/** Obtain a vpiHandle from a BaseClass (any) object */
vpiHandle NewVpiHandle (const uhdm::BaseClass* object);

s_vpi_value* String2VpiValue(std::string_view sv);

s_vpi_delay* String2VpiDelays(std::string_view sv);

std::string VpiValue2String(const s_vpi_value* value);

std::string VpiDelay2String(const s_vpi_delay* delay);

/** Obtain a uhdm::design pointer from a vpiHandle */
uhdm::Design* UhdmDesignFromVpiHandle(vpiHandle hdesign);

/** Shows unique IDs in vpi_visitor dump (uhdmdump) */
void vpi_show_ids(bool show);

#endif  // UHDM_VPI_UHDM_H
