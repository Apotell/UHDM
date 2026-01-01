

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

# Use pybind11 from third_party submodule (consistent with UHDM conventions)
add_subdirectory(../../third_party/pybind11
                 ${CMAKE_CURRENT_BINARY_DIR}/pybind11)

# Collect source files explicitly (preferred over GLOB_RECURSE for CMake)
file(GLOB AUTOGEN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/bindings/autogen/*.cpp")
set(PYUHDM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/module.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/bind_serializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/bind_native_visitor.cpp
    ${AUTOGEN_SOURCES}
)

# Create the pyuhdm module
pybind11_add_module(pyuhdm ${PYUHDM_SOURCES})

# Link against UHDM
target_link_libraries(pyuhdm PRIVATE uhdm)

# Add include directories for project-absolute includes
target_include_directories(pyuhdm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
