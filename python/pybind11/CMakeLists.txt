

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

# Use pybind11 from third_party submodule (consistent with UHDM conventions)
add_subdirectory(../../third_party/pybind11
                 ${CMAKE_CURRENT_BINARY_DIR}/pybind11)

# Collect source files explicitly (preferred over GLOB_RECURSE for CMake)


# Define output directory for generated bindings
set(PYUHDM_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/bindings/autogen")
file(MAKE_DIRECTORY ${PYUHDM_GENERATED_DIR})

# 1. Get the list of files to be generated (Fast, Configure-time)
message(STATUS "Resolving generated file list via scripts/pybind_gen_poc.py --print-files...")
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/pybind_gen_poc.py --print-files
    OUTPUT_VARIABLE AUTOGEN_FILES_Raw
    RESULT_VARIABLE GEN_LIST_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT GEN_LIST_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to resolve generated file list")
endif()

# Convert semicolon-separated string to list
set(AUTOGEN_FILES_LIST "${AUTOGEN_FILES_Raw}")
# Prepend directory to each filename
set(AUTOGEN_SOURCES "")
foreach(F ${AUTOGEN_FILES_LIST})
    list(APPEND AUTOGEN_SOURCES "${PYUHDM_GENERATED_DIR}/${F}")
endforeach()

# 2. Add Custom Command to generate files (slow, Build-time)
# This creates the dependency: If script changes, files are regenerated.
add_custom_command(
    OUTPUT ${AUTOGEN_SOURCES}
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/pybind_gen_poc.py --out_dir ${PYUHDM_GENERATED_DIR}
    DEPENDS ${PROJECT_SOURCE_DIR}/scripts/pybind_gen_poc.py
    COMMENT "Generating pybind11 bindings..."
)

# 3. Add Custom Target to ensure generation happens 
# (Strictly speaking, the dependency from pyuhdm -> AUTOGEN_SOURCES is enough, 
# but a target makes it explicit and allows 'cmake --build . --target generate_pyuhdm')
add_custom_target(generate_pyuhdm DEPENDS ${AUTOGEN_SOURCES})

# Note: We rely on the explicit list ${AUTOGEN_SOURCES} derived above.
# We do NOT use file(GLOB ...) because the files might not exist at first configure.
# This satisfies the requirement for correct dependency tracking while handling dynamic outputs.

set(PYUHDM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/module.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/bind_serializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/bind_BaseClass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/bind_native_visitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/bind_native_listener.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/bind_native_vpi_listener.cpp
    ${AUTOGEN_SOURCES}

)

# Create the pyuhdm module
pybind11_add_module(pyuhdm ${PYUHDM_SOURCES})

# IMPORTANT: ensure bindings are generated before compiling pyuhdm
add_dependencies(pyuhdm generate_pyuhdm)

# Link against UHDM
target_link_libraries(pyuhdm PRIVATE uhdm)

# Add include directories for project-absolute includes
target_include_directories(pyuhdm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
