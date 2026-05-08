#include <uhdm/UhdmListener.h>
#include <uhdm/UhdmPrinter.h>
#include <uhdm/uhdm.h>
#include <uhdm/vpi_uhdm.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace UHDM {
static constexpr int32_t kLevelIndent = 2;
bool UhdmPrinter::s_printIds = false;

inline std::ostream& UhdmPrinter::printIndent(std::ostream& out,
                                              size_t indent) {
  return out << std::string(indent, ' ');
}

void UhdmPrinter::printTree(const any* object, size_t indent /* = 0 */) {
  printAny(m_out, object, indent, "");
}

void UhdmPrinter::printTree(vpiHandle handle, size_t indent /* = 0 */) {
  printAny(m_out, (const any*)((const uhdm_handle*)handle)->object, indent, "");
}

void UhdmPrinter::printTree(const std::vector<vpiHandle>& handles,
                            size_t indent /* = 0 */) {
  for (const vpiHandle h : handles) {
    printTree(h, indent);
  }
}

void UhdmPrinter::printList(const any* object, size_t indent /* = 0 */) {
  UhdmListener listener;
  listener.listenAny(object);

  const OrderedAnySet visited(listener.getVisited().cbegin(),
                              listener.getVisited().cend());

  const bool forceShallowVisit = m_forceShallowVisit;
  m_forceShallowVisit = true;
  for (const any* object : visited) {
    printAny(m_out, object, indent, "");
  }
  m_forceShallowVisit = forceShallowVisit;
}

void UhdmPrinter::printList(vpiHandle handle, size_t indent /* = 0 */) {
  printList((const any*)((const uhdm_handle*)handle)->object, indent);
}

void UhdmPrinter::printList(const std::vector<vpiHandle>& handles,
                            size_t indent /* = 0 */) {
  UhdmListener listener;
  for (const vpiHandle h : handles) {
    listener.listenAny((const any*)((const uhdm_handle*)h)->object);
  }

  const OrderedAnySet visited(listener.getVisited().cbegin(),
                              listener.getVisited().cend());

  const bool forceShallowVisit = m_forceShallowVisit;
  m_forceShallowVisit = true;
  for (const any* object : visited) {
    printAny(m_out, object, indent, "");
  }
  m_forceShallowVisit = forceShallowVisit;
}

std::ostream& UhdmPrinter::beginPrintAny(std::ostream& out, const any* object,
                                         size_t& indent,
                                         std::string_view relation,
                                         bool shallowVisit) {
  if (!relation.empty()) {
    printIndent(out, indent) << "|" << relation << ":";
    if (!shallowVisit) {
      out << "\n";
    }
  }

  const UHDM_OBJECT_TYPE uhdmType = object->UhdmType();
  const std::string typeName = UhdmName(uhdmType);
  if (shallowVisit || (!shallowVisit && !m_forceShallowVisit &&
                       (uhdmType == UHDM_OBJECT_TYPE::uhdmdesign))) {
    out << typeName << ":";
  } else {
    printIndent(out, indent) << "\\_" << typeName << ":";
  }

  std::string_view defName = object->VpiDefName();
  if (!defName.empty()) {
    out << " " << defName;
  }

  BaseClass::vpi_property_value_t value =
      object->GetVpiPropertyValue(vpiFullName);
  const char* const fullName =
      std::holds_alternative<const char*>(value)
          ? const_cast<char*>(std::get<const char*>(value))
          : nullptr;
  if (fullName != nullptr) {
    out << " (" << fullName << ")";
  } else {
    std::string_view name = object->VpiName();
    if (!name.empty()) {
      out << " (" << name << ")";
    }
  }

  if (m_printIds) {
    out << " id:" << object->UhdmId();
  }

  switch (uhdmType) {
    case UHDM_OBJECT_TYPE::uhdmmodule_inst:
    case UHDM_OBJECT_TYPE::uhdmprogram:
    case UHDM_OBJECT_TYPE::uhdmclass_defn:
    case UHDM_OBJECT_TYPE::uhdmpackage:
    case UHDM_OBJECT_TYPE::uhdminterface_inst:
    case UHDM_OBJECT_TYPE::uhdmudp:
    case UHDM_OBJECT_TYPE::uhdminclude_file_info: {
      if (const std::string_view file = object->VpiFile(); !file.empty()) {
        out << " file:" << file;
      }
    } break;
    default:
      break;
  }

  const uint32_t sl = object->VpiLineNo(), el = object->VpiEndLineNo();
  const uint16_t sc = object->VpiColumnNo(), ec = object->VpiEndColumnNo();
  if ((sl != 0) || (sc != 0)) {
    out << " line:" << sl << ":" << sc;

    if ((el != 0) || (ec != 0)) {
      out << " endln:" << el << ":" << ec;
    }
  }

  if (!shallowVisit &&
      ((uhdmType != UHDM_OBJECT_TYPE::uhdmdesign) || m_forceShallowVisit)) {
    indent += kLevelIndent;
  }
  return out << "\n";
}

std::ostream& UhdmPrinter::printProperty(std::ostream& out,
                                         std::string_view name, bool value,
                                         size_t indent) {
  return printIndent(out, indent) << "|" << name << ":" << value << "\n";
}

std::ostream& UhdmPrinter::printProperty(std::ostream& out,
                                         std::string_view name, int32_t value,
                                         size_t indent) {
  return printIndent(out, indent) << "|" << name << ":" << value << "\n";
}

std::ostream& UhdmPrinter::printProperty(std::ostream& out,
                                         std::string_view name, uint32_t value,
                                         size_t indent) {
  return printIndent(out, indent) << "|" << name << ":" << value << "\n";
}

std::ostream& UhdmPrinter::printProperty(std::ostream& out,
                                         std::string_view name,
                                         std::string_view value,
                                         size_t indent) {
  return printIndent(out, indent) << "|" << name << ":" << value << "\n";
}

std::ostream& UhdmPrinter::printProperty(std::ostream& out,
                                         std::string_view name,
                                         const s_vpi_value* value,
                                         size_t indent) {
  if (value == nullptr) return out;
  printIndent(out, indent) << "|" << name << ":";
  switch (value->format) {
    case vpiIntVal:
      return out << value->value.integer << "\n";
    case vpiUIntVal:
      return out << value->value.uint << "\n";
    case vpiRealVal:
      return out << value->value.real << "\n";
    case vpiScalarVal:
      return out << value->value.scalar << "\n";
    case vpiStringVal:
      return out << (const char*)value->value.str << "\n";
    case vpiBinStrVal:
      return out << (const char*)value->value.str << "\n";
    case vpiHexStrVal:
      return out << (const char*)value->value.str << "\n";
    case vpiOctStrVal:
      return out << (const char*)value->value.str << "\n";
    case vpiDecStrVal:
      return out << (const char*)value->value.str << "\n";
    default:
      return out << "\n";
  }
}

std::ostream& UhdmPrinter::printProperty(std::ostream& out,
                                         std::string_view name,
                                         s_vpi_delay* delay, size_t indent) {
  if (delay == nullptr) return out;
  printIndent(out, indent - 2) << "|#";
  switch (delay->time_type) {
    case vpiScaledRealTime:
      return out << delay->da[0].low << "\n";
    default:
      return out << "\n";
  }
}

std::ostream& UhdmPrinter::endPrintAny(std::ostream& out, const any* object,
                                       size_t& indent,
                                       std::string_view relation,
                                       bool shallowVisit) {
  if (!shallowVisit && (object->UhdmType() != UHDM_OBJECT_TYPE::uhdmdesign))
    indent -= kLevelIndent;
  return out;
}

template <typename T, typename>
std::ostream& UhdmPrinter::printCollection(std::ostream& out,
                                           const std::vector<T*>& collection,
                                           size_t indent,
                                           std::string_view relation) {
  if (collection.empty()) return out;
  for (const T* object : collection) {
    printAny(out, object, indent, relation);
  }
  return out;
}

void UhdmPrinter::visitAny(std::ostream& out, const any* object,
                           size_t indent) {
  if (const any* const parent = object->VpiParent()) {
    printAny(out, parent, indent, "vpiParent");
  }
}

std::ostream& UhdmPrinter::printAnyCollection(std::ostream& out,
                                              const VectorOfany& collection,
                                              size_t indent,
                                              std::string_view relation) {
  return printCollection(out, collection, indent, relation);
}

// clang-format off
//<UHDMPRINTER_VISIT_ANY_IMPLEMENTATIONS>
// clang-format on

// clang-format off
//<UHDMPRINTER_PRINT_MANY_IMPLEMENTATIONS>
// clang-format on

void UhdmPrinter::printAny(std::ostream& out, const any* object, size_t indent,
                           std::string_view relation) {
  if (object == nullptr) return;

  const bool shallowVisit =
      (m_forceShallowVisit && !m_callstack.empty()) ||
      ((!m_callstack.empty() && (m_callstack.back() != object->VpiParent())) ||
       !m_visited.emplace(object).second);
  beginPrintAny(out, object, indent, relation, shallowVisit);

  if (!shallowVisit) {
    m_callstack.emplace_back(object);
    // clang-format off
    switch (object->UhdmType()) {
//<UHDMPRINTER_PRINTANY_CASE_STATEMENTS>
    }
    // clang-format on
    m_callstack.pop_back();
  }

  endPrintAny(out, object, indent, relation, shallowVisit);
}

// Print object(s) to input stream as a tree
void print_tree(const any* object, std::ostream& out) {
  if (UhdmPrinter* const printer = new UhdmPrinter(out)) {
    printer->printTree(object);
    delete printer;
  }
}
std::string print_tree(const any* object) {
  std::ostringstream strm;
  print_tree(object, strm);
  return strm.str();
}

void print_tree(vpiHandle handle, std::ostream& out) {
  print_tree((const any*)((const uhdm_handle*)handle)->object, out);
}
std::string print_tree(vpiHandle handle) {
  std::ostringstream strm;
  print_tree(handle, strm);
  return strm.str();
}

void print_tree(const std::vector<vpiHandle>& handles, std::ostream& out) {
  if (UhdmPrinter* const printer = new UhdmPrinter(out)) {
    printer->printTree(handles);
    delete printer;
  }
}

// Print object(s) to input stream as list
void print_list(const any* object, std::ostream& out) {
  if (UhdmPrinter* const printer = new UhdmPrinter(out)) {
    printer->printList(object);
    delete printer;
  }
}
std::string print_list(const any* object) {
  std::ostringstream strm;
  print_list(object, strm);
  return strm.str();
}

void print_list(vpiHandle handle, std::ostream& out) {
  print_list((const any*)((const uhdm_handle*)handle)->object, out);
}
std::string print_list(vpiHandle handle) {
  std::ostringstream strm;
  print_list(handle, strm);
  return strm.str();
}

void print_list(const std::vector<vpiHandle>& handles, std::ostream& out) {
  if (UhdmPrinter* const printer = new UhdmPrinter(out)) {
    printer->printList(handles);
    delete printer;
  }
}
}  // namespace UHDM
