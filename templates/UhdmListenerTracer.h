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
 * File:   UhdmListenerTracer.h
 * Author: hs
 *
 * Created on October 11, 2020, 9:00 PM
 */

#ifndef UHDM_UHDMLISTENERTRACER_H
#define UHDM_UHDMLISTENERTRACER_H

#include "UhdmListener.h"

#include "uhdm/uhdm.h"  // Needed to know how to access VPI line/column

#include <ostream>
#include <string>

#define TRACE_CONTEXT                   \
  "[" << object->getStartLine() <<      \
  "," << object->getStartColumn() <<    \
  ":" << object->getEndLine() <<        \
  "," << object->getEndColumn() <<      \
  "]"

#define TRACE_ENTER strm                \
  << std::string(++indent * 2, ' ')     \
  << __func__ << ": " << TRACE_CONTEXT  \
  << std::endl
#define TRACE_LEAVE strm                \
  << std::string(2 * indent--, ' ')     \
  << __func__ << ": " << TRACE_CONTEXT  \
  << std::endl

namespace uhdm {
class UhdmListenerTracer : public UhdmListener {
  public:
    UhdmListenerTracer(std::ostream &strm) : strm(strm) {}
    ~UhdmListenerTracer() final = default;

<UHDM_LISTENER_OBJECT_TRACER_METHODS>
<UHDM_LISTENER_COLLECTION_TRACER_METHODS>
  protected:
   std::ostream &strm;
   int32_t indent = -1;
};
} // namespace uhdm

#endif  // UHDM_UHDMLISTENERTRACER_H
