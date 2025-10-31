/*
 * Copyright 2019 Alain Dargelas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <uhdm/UhdmComparer.h>
#include <uhdm/uhdm-version.h>
#include <uhdm/uhdm.h>
#include <uhdm/vpi_visitor.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <Windows.h>
#endif

namespace fs = std::filesystem;

static int32_t usage(const char* progName) {
  std::cerr << "Usage:" << std::endl
            << "  " << progName << " <uhdm-file> <uhdm-file>" << std::endl
            << std::endl
            << "Reads input uhdm binary representations of two files and "
               "compares them topographically. (Version: "
            << UHDM_VERSION_MAJOR << "." << UHDM_VERSION_MINOR << ") "
            << std::endl
            << std::endl
            << "Exits with code" << std::endl
            << "  = 0, if input files are equal" << std::endl
            << "  < 0, if input files are not equal" << std::endl
            << "  > 0, for any failures" << std::endl;
  return 0;
}

struct AnyComparer final {
  bool operator()(const uhdm::Any* const lp, const uhdm::Any* const rp) const {
    int32_t r = static_cast<size_t>(lp->getUhdmType()) -
                static_cast<size_t>(rp->getUhdmType());
    if (r == 0) r = lp->getUhdmId() - rp->getUhdmId();
    return r < 0;
  }
};

class EquivalenceComparer final : public uhdm::UhdmComparer {
 private:
  template <typename T>
  int32_t compareT(const uhdm::Any* plhs, const std::vector<T*>* lhs,
                   const uhdm::Any* prhs, const std::vector<T*>* rhs,
                   uint32_t relation, int32_t r) {
    if (isRelationIgnored(relation)) return r;
    if ((lhs != nullptr) && (rhs != nullptr)) {
      std::vector<T*> vi(*lhs);
      std::vector<T*> vj(*rhs);
      if (vi.size() < vj.size()) vi.swap(vj);

      AnyComparer comparer;
      std::sort(vi.begin(), vi.end(), comparer);
      std::sort(vj.begin(), vj.end(), comparer);

      std::vector<bool> foundj(vj.size(), false);

      size_t sj = 0;
      for (size_t i = 0, ni = vi.size(), nj = vj.size(); i < ni; ++i) {
        const uhdm::Any* const iany = vi[i];
        while ((sj < nj) && (vj[sj]->getUhdmType() != iany->getUhdmType())) {
          ++sj;
        }

        bool foundi = false;
        for (size_t j = sj; j < nj; ++j) {
          const uhdm::Any* const jany = vj[j];
          if (iany->getUhdmType() != jany->getUhdmType()) break;

          if ((!foundi || !foundj[j]) &&
              (compare(iany, jany, relation, r) == 0)) {
            foundi = foundj[j] = true;
          }
        }

        if (!foundi) {
          setFailed(iany, nullptr, relation, true);
          r = 1;
          break;
        }
      }

      if (r == 0) {
        std::vector<bool>::const_iterator it =
            std::find(foundj.cbegin(), foundj.cend(), false);
        if (it != foundj.cend()) {
          size_t index = std::distance(foundj.cbegin(), it);
          setFailed(nullptr, vj[index], relation, true);
          r = -1;
        }
      }
    }
    return r;
  }

 public:
  using UhdmComparer::compare;

  int32_t compare(const uhdm::Any *plhs, const uhdm::TypespecCollection *lhs,
                  const uhdm::Any *prhs, const uhdm::TypespecCollection *rhs,
                  uint32_t relation, int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::ArrayTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::ArrayTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs, const uhdm::BitTypespecCollection *lhs,
                  const uhdm::Any *prhs, const uhdm::BitTypespecCollection *rhs,
                  uint32_t relation, int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::ByteTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::ByteTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::ChandleTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::ChandleTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::ClassTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::ClassTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::EnumTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::EnumTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::EventTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::EventTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::ImportTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::ImportTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs, const uhdm::IntTypespecCollection *lhs,
                  const uhdm::Any *prhs, const uhdm::IntTypespecCollection *rhs,
                  uint32_t relation, int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::IntegerTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::IntegerTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::InterfaceTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::InterfaceTypespecCollection *rhs,
                  uint32_t relation, int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::LogicTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::LogicTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::LongIntTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::LongIntTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::ModuleTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::ModuleTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::PropertyTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::PropertyTypespecCollection *rhs,
                  uint32_t relation, int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::RealTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::RealTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::SequenceTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::SequenceTypespecCollection *rhs,
                  uint32_t relation, int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::ShortIntTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::ShortIntTypespecCollection *rhs,
                  uint32_t relation, int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::ShortRealTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::ShortRealTypespecCollection *rhs,
                  uint32_t relation, int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::StringTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::StringTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::StructTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::StructTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::TimeTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::TimeTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::TypeParameterCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::TypeParameterCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::TypedefTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::TypedefTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::UnionTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::UnionTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::UnsupportedTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::UnsupportedTypespecCollection *rhs,
                  uint32_t relation, int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
  int32_t compare(const uhdm::Any *plhs,
                  const uhdm::VoidTypespecCollection *lhs,
                  const uhdm::Any *prhs,
                  const uhdm::VoidTypespecCollection *rhs, uint32_t relation,
                  int32_t r) {
    return compareT(plhs, lhs, prhs, rhs, relation, r);
  }
};

int32_t private_main(int32_t argc, char** argv) {
  fs::path fileA = argv[1];
  fs::path fileB = argv[2];

  std::error_code ec;
  if (!fs::is_regular_file(fileA, ec) || ec) {
    std::cerr << fileA << ": File does not exist!" << std::endl;
    return usage(argv[0]);
  }

  if (!fs::is_regular_file(fileB, ec) || ec) {
    std::cerr << fileB << ": File does not exist!" << std::endl;
    return usage(argv[0]);
  }

  std::unique_ptr<uhdm::Serializer> serializerA(new uhdm::Serializer);
  std::vector<vpiHandle> handlesA = serializerA->restore(fileA);

  std::unique_ptr<uhdm::Serializer> serializerB(new uhdm::Serializer);
  std::vector<vpiHandle> handlesB = serializerB->restore(fileB);

  if (handlesA.empty()) {
    std::cerr << fileA << ": Failed to load." << std::endl;
    return 1;
  }

  if (handlesB.empty()) {
    std::cerr << fileB << ": Failed to load." << std::endl;
    return 1;
  }

  if (handlesA.size() != handlesB.size()) {
    std::cerr << "Number of designs mismatch." << std::endl;
    return -1;
  }

  std::function<const uhdm::Design*(vpiHandle handle)> to_design =
      [](vpiHandle handle) {
        return (const uhdm::Design*)((const uhdm_handle*)handle)->object;
      };

  std::vector<const uhdm::Design*> designsA;
  designsA.reserve(handlesA.size());
  std::transform(handlesA.begin(), handlesA.end(), std::back_inserter(designsA),
                 to_design);

  std::vector<const uhdm::Design*> designsB;
  designsB.reserve(handlesB.size());
  std::transform(handlesB.begin(), handlesB.end(), std::back_inserter(designsB),
                 to_design);

  for (size_t i = 0, ni = designsA.size(); i < ni; ++i) {
    EquivalenceComparer comparer;
    if (comparer.compare(designsA[i], designsB[i]) != 0) {
      std::cout << "Relation: " << comparer.getFailedRelation() << std::endl;
      if (const uhdm::Any *const lhs = comparer.getFailedLhs()) {
        std::cout << "LHS: " << lhs->getFile() << std::endl;
        uhdm::decompile(lhs);
      } else {
        std::cout << "LHS: <null>" << std::endl;
      }

      std::cout << std::string(80, '=') << std::endl;

      if (const uhdm::Any* const rhs = comparer.getFailedRhs()) {
        std::cout << "RHS: " << rhs->getFile() << std::endl;
        uhdm::decompile(rhs);
      } else {
        std::cout << "RHS: <null>" << std::endl;
      }
      return -1;
    }

    std::cout << "Cache size: " << comparer.getCache().size() << std::endl;
  }

  return 0;
}

int32_t main(int32_t argc, char** argv) {
  if (argc != 3) {
    return usage(argv[0]);
  }

#if defined(_MSC_VER) && defined(_DEBUG)
  // Redirect cout to file
  std::streambuf* cout_rdbuf = nullptr;
  std::streambuf* cerr_rdbuf = nullptr;
  std::ofstream file;
  if (IsDebuggerPresent() != 0) {
    file.open("cout.txt");
    cout_rdbuf = std::cout.rdbuf(file.rdbuf());
    cerr_rdbuf = std::cerr.rdbuf(file.rdbuf());
  }
#endif

  const int32_t r = private_main(argc, argv);

#if defined(_MSC_VER) && defined(_DEBUG)
  // Redirect cout back to screen
  if (cout_rdbuf != nullptr) {
    std::cout.rdbuf(cout_rdbuf);
  }
  if (cerr_rdbuf != nullptr) {
    std::cerr.rdbuf(cerr_rdbuf);
  }
  if (file.is_open()) {
    file.flush();
    file.close();
  }
#endif

  return r;
}
