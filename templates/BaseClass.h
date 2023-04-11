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
 * File:   BaseClass.h
 * Author:
 *
 * Created on December 14, 2019, 10:03 PM
 */

#ifndef UHDM_BASE_CLASS_H
#define UHDM_BASE_CLASS_H

#include <deque>
#include <set>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#include <uhdm/uhdm_types.h>
#include <uhdm/RTTI.h>
#include <uhdm/SymbolFactory.h>

namespace UHDM {
  class BaseClass;
  class Serializer;
  class ElaboratorListener;
  static inline constexpr std::string_view kEmpty("");

#ifdef STANDARD_VPI
  typedef std::set<vpiHandle> VisitedContainer;
#else
  typedef std::set<const BaseClass*> VisitedContainer;
#endif
  typedef std::set<const BaseClass*> AnySet;

  class ClientData {
  public:
    virtual ~ClientData() = default;
  };

  class BaseClass : public RTTI {
    UHDM_IMPLEMENT_RTTI(BaseClass, RTTI)
    friend Serializer;

  public:
    // Use implicit constructor to initialize all members
    // BaseClass()

    virtual ~BaseClass() = default;

    Serializer* GetSerializer() const { return serializer_; }

    virtual UHDM_OBJECT_TYPE UhdmType() const = 0;

    virtual const BaseClass* VpiParent() const = 0;

    virtual bool VpiParent(BaseClass* data) = 0;

    virtual std::string_view VpiFile() const = 0;

    virtual bool VpiFile(std::string_view data) = 0;

    virtual uint32_t VpiLineNo() const final { return vpiLineNo_; }

    virtual bool VpiLineNo(uint32_t data) final { vpiLineNo_ = data; return true; }

    virtual uint16_t VpiColumnNo() const final { return vpiColumnNo_; }

    virtual bool VpiColumnNo(uint16_t data) final { vpiColumnNo_ = data; return true; }

    virtual uint32_t VpiEndLineNo() const final { return vpiEndLineNo_; }

    virtual bool VpiEndLineNo(uint32_t data) final { vpiEndLineNo_ = data; return true; }

    virtual uint16_t VpiEndColumnNo() const final { return vpiEndColumnNo_; }

    virtual bool VpiEndColumnNo(uint16_t data) final { vpiEndColumnNo_ = data; return true; }

    virtual std::string_view VpiName() const { return kEmpty; }

    virtual std::string_view VpiDefName() const { return kEmpty; }

    virtual uint32_t VpiType() const = 0;

    ClientData* Data() const { return clientData_; }

    void Data(ClientData* data) { clientData_ = data; }

    virtual uint32_t UhdmId() const = 0;

    virtual bool UhdmId(uint32_t id) = 0;

    // TODO: Make the next three functions pure-virtual after transition to pygen.
    virtual const BaseClass* GetByVpiName(std::string_view name) const;

    virtual std::tuple<const BaseClass*, UHDM_OBJECT_TYPE,
                       const std::vector<const BaseClass*>*>
    GetByVpiType(int32_t type) const;

    typedef std::variant<int64_t, const char*> vpi_property_value_t;
    virtual vpi_property_value_t GetVpiPropertyValue(int32_t property) const;

    // Create a deep copy of this object.
    virtual BaseClass* DeepClone(Serializer* serializer,
                                 ElaboratorListener* elaborator,
                                 BaseClass* parent) const = 0;

    virtual int32_t Compare(const BaseClass* const other,
                            AnySet& visited) const;
    int32_t Compare(const BaseClass* const other) const;

  protected:
    void DeepCopy(BaseClass* clone, Serializer* serializer,
                  ElaboratorListener* elaborator, BaseClass* parent) const;

    void SetSerializer(Serializer* serial) { serializer_ = serial; }

    Serializer* serializer_ = nullptr;

    ClientData* clientData_ = nullptr;

  private:
    uint32_t vpiLineNo_ = 0;
    uint32_t vpiEndLineNo_ = 0;
    uint16_t vpiColumnNo_ = 0;
    uint16_t vpiEndColumnNo_ = 0;
  };

  template<typename T>
  class FactoryT final {
    friend Serializer;
    // TODO: make this an arena: iinstead of pointers, store the
    // objects directly with placement-new'ed object using emplace_back()
    // One less indirection and a lot less overhead.
    // (dqque guarantees pointer stability; however, we'd need to
    // do something special in Erase() - can't re-arrange).
    typedef std::deque<T *> objects_t;

    public:
      T* Make() {
        T* obj = new T;
        objects_.push_back(obj);
        return obj;
      }

      bool Erase(T* tps) {
        for (typename objects_t::const_iterator itr = objects_.begin();
             itr != objects_.end(); ++itr) {
          if ((*itr) == tps) {
            objects_.erase(itr);
            return true;
          }
        }
        return false;
      }

      void Purge() {
        for (typename objects_t::reference obj : objects_) {
          delete obj;
        }
        objects_.clear();
      }

    private:
      objects_t objects_;
  };

  typedef FactoryT<std::vector<BaseClass*>> VectorOfBaseClassFactory;
  typedef FactoryT<std::vector<BaseClass*>> VectorOfanyFactory;

}  // namespace UHDM

UHDM_IMPLEMENT_RTTI_CAST_FUNCTIONS(any_cast, UHDM::BaseClass)

#endif
