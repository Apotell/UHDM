#include <pybind11/pybind11.h>

// System headers
#include <algorithm>
#include <vector>

template <typename T>
static std::vector<T *> getCollection(const std::vector<T *> *from) {
  std::vector<T *> to;
  if (from != nullptr) {
    to.reserve(from->size());
    to.insert(to.cend(), from->cbegin(), from->cend());
  }
  return to;
}

template <typename T>
static void setCollection(const std::vector<T *> &from, std::vector<T *> *to) {
  if (to != nullptr) {
    to->reserve(to->size() + from.size());
    to->insert(to->cend(), from.cbegin(), from.cend());
  }
}

// Implementation headers
// <IMPLEMENTATION_INCLUDES>

// Forward declarations
extern void bind_Serializer(pybind11::module_ &m);
extern void bind_UhdmType(pybind11::module_ &m);
extern void bind_BaseClass(pybind11::module_ &m);
extern void bind_UhdmListener(pybind11::module_ &m);
extern void bind_UhdmVisitor(pybind11::module_ &m);
extern void bind_VpiListener(pybind11::module_ &m);

PYBIND11_MODULE(pyuhdm, m) {
  m.doc() = "UHDM Python bindings";

  bind_Serializer(m);
  bind_UhdmType(m);
  bind_BaseClass(m);
// <BIND_CLASSES_INVOCATIONS>
  bind_UhdmListener(m);
  bind_UhdmVisitor(m);
  bind_VpiListener(m);
}
