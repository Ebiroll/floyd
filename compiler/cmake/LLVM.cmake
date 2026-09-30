# This is an INTERFACE target for LLVM, usage:
#   target_link_libraries(${PROJECT_NAME} <PRIVATE|PUBLIC|INTERFACE> LLVM)
# The include directories and compile definitions will be properly handled.

set(CMAKE_FOLDER_LLVM "${CMAKE_FOLDER}")
if(CMAKE_FOLDER)
    set(CMAKE_FOLDER "${CMAKE_FOLDER}/LLVM")
else()
    set(CMAKE_FOLDER "LLVM")
endif()

# Find the installed LLVM package. Linux currently uses the LLVM version
# provided by the host distribution instead of building LLVM from source.
# FLOYD_LLVM_APT_VERSION lets Ubuntu builds pick between distro packages
# such as llvm-15-dev and llvm-18-dev (defaults to 18).
if(UNIX AND NOT APPLE)
    if(NOT FLOYD_LLVM_APT_VERSION)
        set(FLOYD_LLVM_APT_VERSION "18")
    endif()
    find_package(LLVM CONFIG REQUIRED HINTS /usr/lib/llvm-${FLOYD_LLVM_APT_VERSION}/lib/cmake/llvm)
elseif(APPLE)
    find_package(LLVM CONFIG REQUIRED HINTS /usr/local/opt/llvm@18/lib/cmake/llvm /opt/homebrew/opt/llvm@18/lib/cmake/llvm)
else()
    find_package(LLVM CONFIG REQUIRED)
endif()


add_definitions(
    -D_CRT_SECURE_NO_DEPRECATE
    -D_CRT_SECURE_NO_WARNINGS
    -D_CRT_NONSTDC_NO_DEPRECATE
    -D_CRT_NONSTDC_NO_WARNINGS
    -D_SCL_SECURE_NO_DEPRECATE
    -D_SCL_SECURE_NO_WARNINGS
    -DUNICODE
    -D_UNICODE
    -D__STDC_CONSTANT_MACROS
    -D__STDC_FORMAT_MACROS
    -D__STDC_LIMIT_MACROS
)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Expose the found LLVM major version to C++ so code can #if FLOYD_LLVM_VERSION_MAJOR >= N
# to handle API differences between supported LLVM releases (e.g. 15 vs 18).
add_definitions(-DFLOYD_LLVM_VERSION_MAJOR=${LLVM_VERSION_MAJOR})

# Split the definitions properly (https://weliveindetail.github.io/blog/post/2017/07/17/notes-setup.html)
separate_arguments(LLVM_DEFINITIONS)

# Some diagnostics (https://stackoverflow.com/a/17666004/1806760)
message(STATUS "LLVM libraries: ${LLVM_LIBRARIES}")
message(STATUS "LLVM includes: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM definitions: ${LLVM_DEFINITIONS}")
message(STATUS "LLVM tools: ${LLVM_TOOLS_BINARY_DIR}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS} -DNOMINMAX)

set(CMAKE_FOLDER "${CMAKE_FOLDER_LLVM}")
unset(CMAKE_FOLDER_LLVM)
