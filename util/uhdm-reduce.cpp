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
#include <uhdm/uhdm-version.h>
#include <uhdm/uhdm.h>
#include <uhdm/vpi_visitor.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

static int32_t usage(const char *progName) {
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

int32_t main(int32_t argc, char **argv) {
  if (argc != 2) {
    return usage(argv[0]);
  }

  fs::path filepath = argv[1];

  std::error_code ec;
  if (!fs::is_regular_file(filepath, ec) || ec) {
    std::cerr << filepath << ": File does not exist!" << std::endl;
    return usage(argv[0]);
  }

  std::unique_ptr<uhdm::Serializer> serializer(new uhdm::Serializer);
  std::vector<vpiHandle> handles = serializer->restore(filepath);

  if (handles.empty()) {
    std::cerr << filepath << ": Failed to load." << std::endl;
    return 1;
  }

  std::function<const uhdm::Design *(vpiHandle handle)> to_design =
      [](vpiHandle handle) {
        return (const uhdm::Design *)((const uhdm_handle *)handle)->object;
      };

  std::vector<const uhdm::Design *> designs;
  designs.reserve(handles.size());
  std::transform(handles.cbegin(), handles.cend(), std::back_inserter(designs),
                 to_design);

  if (!designs.empty()) {
    vpi_show_ids(true);
    uhdm::visit_designs(handles, std::cout);
    std::cout << std::endl << std::endl;

    uhdm::Reducer reducer(serializer.get());
    reducer.reduce();

    uhdm::visit_designs(handles, std::cout);
    std::cout << std::endl << std::endl;

    return 0;
  }

  return -1;
}
