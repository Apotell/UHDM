#ifndef UHDM_UHDMPRINTER_H
#define UHDM_UHDMPRINTER_H
#pragma once

#include <UHDM/containers.h>
#include <uhdm/BaseClass.h>
#include <uhdm/uhdm_forward_decl.h>

#include <ostream>
#include <string>
#include <vector>

namespace UHDM {
class UhdmPrinter final {
 protected:
  using Callstack = std::vector<const any*>;
  static bool s_printIds;

 public:
  explicit UhdmPrinter(std::ostream& out) : m_out(out) {}
  ~UhdmPrinter() = default;
  UhdmPrinter(const UhdmPrinter& rhs) = delete;
  UhdmPrinter& operator=(const UhdmPrinter& rhs) = delete;

  static void setPrintIds(bool showOrHide) { s_printIds = showOrHide; }

  void printTree(const any* object, size_t indent = 0);
  void printTree(vpiHandle handle, size_t indent = 0);

  template <typename T, typename = std::enable_if_t<std::is_base_of_v<any, T>>>
  void printTree(const std::vector<T*>& collection, size_t indent = 0);
  void printTree(const std::vector<vpiHandle>& handles, size_t indent = 0);

  void printList(const any* object, size_t indent = 0);
  void printList(vpiHandle handle, size_t indent = 0);

  template <typename T, typename = std::enable_if_t<std::is_base_of_v<any, T>>>
  void printList(const std::vector<T*>& collection, size_t indent = 0);
  void printList(const std::vector<vpiHandle>& handles, size_t indent = 0);

 protected:
  std::ostream& beginPrintAny(std::ostream& out, const any* object,
                              size_t& indent, std::string_view relation,
                              bool shallowVisit);
  std::ostream& printIndent(std::ostream& out, size_t indent);
  std::ostream& printProperty(std::ostream& out, std::string_view name,
                              bool value, size_t indent);
  std::ostream& printProperty(std::ostream& out, std::string_view name,
                              int32_t value, size_t indent);
  std::ostream& printProperty(std::ostream& out, std::string_view name,
                              uint32_t value, size_t indent);
  std::ostream& printProperty(std::ostream& out, std::string_view name,
                              std::string_view value, size_t indent);
  std::ostream& printProperty(std::ostream& out, std::string_view name,
                              const s_vpi_value* value, size_t indent);
  std::ostream& printProperty(std::ostream& out, std::string_view name,
                              s_vpi_delay* delay, size_t indent);
  std::ostream& endPrintAny(std::ostream& out, const any* object,
                            size_t& indent, std::string_view relation,
                            bool shallowVisit);

  template <typename T>
  std::ostream& printProperty(std::string_view name, T value,
                              size_t indent) = delete;

  void setPrintId(bool showOrHide) { m_printIds = showOrHide; }

 private:
  // clang-format off
  void visitAny(std::ostream &out, const any *object, size_t indent);
//<UHDMPRINTER_VISIT_ANY_DECLARATIONS>
  // clang-format on

 protected:
  struct AnyComparer final {
    bool operator()(const any* lhs, const any* rhs) const {
      return (lhs->UhdmType() == rhs->UhdmType())
                 ? (lhs->UhdmId() < rhs->UhdmId())
                 : (lhs->UhdmType() < rhs->UhdmType());
    }
  };
  using OrderedAnySet = std::set<const any*, AnyComparer>;

  void printAny(std::ostream& out, const any* object, size_t indent,
                std::string_view relation);
  template <typename T, typename = std::enable_if_t<std::is_base_of_v<any, T>>>
  std::ostream& printCollection(std::ostream& out,
                                const std::vector<T*>& collection,
                                size_t indent, std::string_view relation);

  // clang-format off
  std::ostream &printAnyCollection(std::ostream &out, const VectorOfany &collection, size_t indent, std::string_view relation);
//<UHDMPRINTER_PRINT_MANY_DECLARATIONS>
  // clang-format on

 protected:
  std::ostream& m_out;
  Callstack m_callstack;
  OrderedAnySet m_visited;
  bool m_forceShallowVisit = false;
  bool m_printIds = s_printIds;
};

template <typename T, typename>
inline void UhdmPrinter::printTree(const std::vector<T*>& collection,
                                   size_t indent /* = 0 */) {
  for (const T* any : collection) {
    printTree(any, indent);
  }
}

template <typename T, typename = std::enable_if_t<std::is_base_of_v<any, T>>>
inline void UhdmPrinter::printList(const std::vector<T*>& collection,
                                   size_t indent /* = 0 */) {
  UhdmListener listener;
  for (const any* object : collection) {
    listener.listenAny(object);
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

// Print object(s) to input stream as a tree
void print_tree(const any* object, std::ostream& out);
std::string print_tree(const any* object);

void print_tree(vpiHandle handle, std::ostream& out);
std::string print_tree(vpiHandle handle);

void print_tree(const std::vector<vpiHandle>& handles, std::ostream& out);

template <typename T, typename = std::enable_if_t<std::is_base_of_v<any, T>>>
inline void print_tree(const std::vector<T*>& objects, std::ostream& out) {
  if (UhdmPrinter* const printer = new UhdmPrinter(out)) {
    printer->printTree(objects);
    delete printer;
  }
}

// Print object(s) to input stream as list
void print_list(const any* object, std::ostream& out);
std::string print_list(const any* object);

void print_list(vpiHandle handle, std::ostream& out);
std::string print_list(vpiHandle handle);

void print_list(const std::vector<vpiHandle>& handles, std::ostream& out);

template <typename T, typename = std::enable_if_t<std::is_base_of_v<any, T>>>
inline void print_list(const std::vector<T*>& objects, std::ostream& out) {
  if (UhdmPrinter* const printer = new UhdmPrinter(out)) {
    printer->printList(objects);
    delete printer;
  }
}
}  // namespace UHDM

#endif  // UHDM_UHDMPRINTER_H
