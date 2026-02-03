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

#include <uhdm/Reducer.h>
#include <uhdm/Utils.h>
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
using statistics_t = std::map<std::string_view, std::set<uint32_t>, std::less<>>;

static int32_t usage(const char* progName) {
  std::cerr << "Usage:" << std::endl
            << "  " << progName << " <input.uhdm> [reduced.uhdm] [-v] [-d <type>]..." << std::endl
            << std::endl
            << "Reads an input UHDM binary file, reduces it, and optionally "
               "writes the reduced output to a file. (Version: "
            << UHDM_VERSION_MAJOR << "." << UHDM_VERSION_MINOR << ") " << std::endl
            << std::endl
            << "Arguments:" << std::endl
            << "  <input.uhdm>    Input UHDM binary file (required)" << std::endl
            << "  [reduced.uhdm]  Output UHDM file after reduction (optional)" << std::endl
            << "  [-v]            Verbose output (optional)" << std::endl
            << "  [-d <type>]     Dump type (can be repeated, e.g. -d uhdm -d ast)" << std::endl
            << std::endl
            << "Exit codes:" << std::endl
            << "  0   Success" << std::endl
            << "  >0  Failure" << std::endl;
  return 0;
}

static void printReductionStatistics(std::ostream& os, const statistics_t& before, const statistics_t& after) {
  constexpr int32_t kTypeW = 30;
  constexpr int32_t kCol1W = 8;
  constexpr int32_t kCol2W = 8;
  constexpr int32_t kCol3W = 8;
  constexpr int32_t kCol4W = 10;
  constexpr int32_t kTableW = kTypeW + kCol1W + kCol2W + kCol3W + kCol4W;

  auto printBanner = [&](std::string_view txt) {
    uint32_t pad = kTableW - txt.size();
    os << std::string(pad / 2, '=') << txt << std::string(pad - (pad / 2), '=') << '\n';
  };

  printBanner(" BEGIN REDUCTION RESULT ");

  os << std::left << std::setw(kTypeW) << "Name" << std::right << std::setw(kCol1W) << "Before" << std::setw(kCol2W)
     << "After" << std::setw(kCol3W) << "Added" << std::setw(kCol4W) << "Removed" << '\n';
  os << std::string(kTableW, '-') << '\n';

  std::set<std::string_view, std::less<>> allTypes;
  for (auto& e : before) allTypes.emplace(e.first);
  for (auto& e : after) allTypes.emplace(e.first);

  const std::set<uint32_t> empty{};

  uint32_t beforeCount = 0;
  uint32_t afterCount = 0;
  uint32_t addedCount = 0;
  uint32_t removedCount = 0;
  uint32_t othersCount = 0;

  for (std::string_view type : allTypes) {
    const std::set<uint32_t>& bset = (before.count(type) > 0) ? before.at(type) : empty;
    const std::set<uint32_t>& aset = (after.count(type) > 0) ? after.at(type) : empty;
    if (bset.empty() && aset.empty()) continue;

    beforeCount += bset.size();
    afterCount += aset.size();

    const uint32_t added =
        std::count_if(aset.cbegin(), aset.cend(), [&bset](uint32_t id) { return bset.count(id) == 0; });
    const uint32_t removed =
        std::count_if(bset.cbegin(), bset.cend(), [&aset](uint32_t id) { return aset.count(id) == 0; });
    if ((added == 0) && (removed == 0)) {
      othersCount += aset.size();
      continue;
    }

    addedCount += added;
    removedCount += removed;

    os << std::left << std::setw(kTypeW) << type << std::right << std::setw(kCol1W) << bset.size() << std::setw(kCol2W)
       << aset.size() << std::setw(kCol3W) << added << std::setw(kCol4W) << removed << '\n';
  }

  if (othersCount > 0) {
    os << std::string(kTableW, '-') << '\n';
    os << std::left << std::setw(kTypeW) << "Others" << std::right << std::setw(kCol1W) << othersCount
       << std::setw(kCol2W) << othersCount << std::setw(kCol3W) << 0 << std::setw(kCol4W) << 0 << '\n';
  }

  os << std::string(kTableW, '-') << '\n';
  os << std::right << std::setw(kTypeW + kCol1W) << beforeCount << std::setw(kCol2W) << afterCount << std::setw(kCol3W)
     << addedCount << std::setw(kCol4W) << removedCount << "\n";

  printBanner(" END REDUCTION RESULT ");
}

int32_t private_main(int32_t argc, char** argv) {
  fs::path inputFile = argv[1];
  fs::path outputFile;
  bool hasOutputFile = false;
  bool verbose = false;

  std::vector<std::string> debugArgs;

  for (int32_t i = 2; i < argc; ++i) {
    std::string_view arg(argv[i]);

    if (arg == "-v") {
      verbose = true;
    } else if (arg == "-d") {
      if (i + 1 >= argc) {
        return usage(argv[0]);
      }
      debugArgs.emplace_back(argv[++i]);
    } else if (!hasOutputFile) {
      outputFile = arg;
      hasOutputFile = true;
    } else {
      return usage(argv[0]);
    }
  }

  std::cout << "COMMAND:";
  for (int32_t i = 1; i < argc; ++i) {
    std::cout << " " << argv[i];
  }
  std::cout << std::endl << std::endl;

  std::error_code ec;
  if (!fs::is_regular_file(inputFile, ec) || ec) {
    std::cerr << inputFile << ": File does not exist!" << std::endl;
    return usage(argv[0]);
  }

  std::unique_ptr<uhdm::Serializer> serializer(new uhdm::Serializer);
  std::vector<vpiHandle> handles = serializer->restore(inputFile);

  if (handles.empty()) {
    std::cerr << inputFile << ": Failed to load." << std::endl;
    return 1;
  }

#if _DEBUG
  if (verbose) vpi_show_ids(true);
#endif

  const bool argUhdmRequested = std::find(debugArgs.cbegin(), debugArgs.cend(), "uhdm") != debugArgs.cend();

  if (argUhdmRequested) {
    std::cout << "====== UHDM (Flat) =======\n";
    uhdm::visit_designs(handles, std::cout);
    std::cout << "===================\n";
  }

  const statistics_t beforeIds = serializer->getObjectIdSets();
  {
    uhdm::Reducer(serializer.get()).reduce(verbose);
    serializer->collectGarbage();
    serializer->setGCEnabled(false);
  }
  const statistics_t afterIds = serializer->getObjectIdSets();

  if (hasOutputFile) {
    if (verbose) {
      std::cout << "Saving reduced model to: " << outputFile << "\n";
    }
    serializer->save(outputFile);
  }

  if (verbose) {
    std::cout << "====== UHDM (Reduced) =======\n";
    uhdm::visit_designs(handles, std::cout);
    std::cout << "===================\n\n";
  }
  printReductionStatistics(std::cout, beforeIds, afterIds);

  return 0;
}

int32_t main(int32_t argc, char** argv) {
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
