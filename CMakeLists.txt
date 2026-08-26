cmake_minimum_required(VERSION 3.21)
# cmark-gfm's CMakeLists.txt uses `cmake_minimum_required(VERSION 3.0)`,
# which CMake >= 4.0 rejects by default. Setting CMAKE_POLICY_VERSION_MINIMUM
# to 3.5 downgrades the policy floor just enough to accept it.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(MDReader VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets)
qt_standard_project_setup()

include(FetchContent)
FetchContent_Declare(
    cmark-gfm
    GIT_REPOSITORY https://github.com/github/cmark-gfm.git
    GIT_TAG 0.29.0.gfm.13
    GIT_SHALLOW TRUE
)
# Upstream uses CMARK_* options (not CMARK_GFM_*). Disable shared/tests/examples
# to keep our build lean; install rules remain but are harmless because we
# never invoke `make install` (FetchContent_MakeAvailable embeds it).
set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cmark-gfm)

add_subdirectory(src)

option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests/unit)
    add_subdirectory(tests/integration)
endif()

include(CPack)
set(CPACK_PACKAGE_NAME "MDReader")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "MDReader")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME "MDReader-${PROJECT_VERSION}-Linux.deb")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6gui6, libqt6widgets6")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Markdown reader/editor")
