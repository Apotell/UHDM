/*
 * Copyright 2023 Alain Dargelas
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

#include <uhdm/uhdm.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace fs = std::filesystem;

static int32_t usage(const char* progName) {
  std::cerr << "Usage:" << std::endl
            << "  " << progName << " <uhdm-file>" << std::endl
            << std::endl
            << "Creates a design and writes it to the input <uhdm-file> path" << std::endl;
  return 0;
}

int32_t main(int32_t argc, char** argv) {
  if (argc != 2) {
    return usage(argv[0]);
  }

  fs::path filepath = argv[1];

  std::unique_ptr<uhdm::Serializer> serializer(new uhdm::Serializer);
  uhdm::Design* const design = serializer->make<uhdm::Design>();
  design->setName("unnamed");

  uhdm::Module* const module = serializer->make<uhdm::Module>();
  module->setName("work@top");
  module->setDefName("work@top");
  module->setParent(design);

  uhdm::Net* const net = serializer->make<uhdm::Net>();
  net->setName("a");
  net->setParent(module);

  uhdm::Port* const port1 = serializer->make<uhdm::Port>();
  port1->setName("a");
  port1->setParent(module);

  uhdm::RefModule* const refModule = serializer->make<uhdm::RefModule>();
  refModule->setName("b");
  refModule->setParent(module);

  uhdm::Port* const port2 = serializer->make<uhdm::Port>();
  port2->setName("a");
  port2->setParent(refModule);
  refModule->getPorts(true)->emplace_back(port2);

  serializer->save(filepath);

  return 0;
}
