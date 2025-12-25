cmake_minimum_required(VERSION 3.20)
project(uhdm_py_poc)

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

# Use pybind11 from third_party submodule (consistent with UHDM conventions)
add_subdirectory(../../third_party/pybind11
                 ${CMAKE_CURRENT_BINARY_DIR}/pybind11)

pybind11_add_module(uhdm_py uhdm_py.cpp)
target_link_libraries(uhdm_py PRIVATE uhdm)
