cmake_minimum_required(VERSION 3.21)
# cmark-gfm's CMakeLists.txt uses `cmake_minimum_required(VERSION 3.0)`,
# which CMake >= 4.0 rejects by default. Setting CMAKE_POLICY_VERSION_MINIMUM
# to 3.5 downgrades the policy floor just enough to accept it.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(MDReader VERSION 0.1.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Phase 7.1 note: we deliberately require Qt 6.5 here. The .deb ships a
# self-contained copy of Qt 6.5.3 (libQt6*.so + QtWebEngineProcess + .pak
# resources) under /usr/lib/mdreader/qt6 so the binary has matching ABI
# without relying on the host distro's Qt packaging. Ubuntu 22.04 (jammy)
# ships a Qt 6.2.4 that is missing QtWebEngineProcess entirely, which
# makes every WebEngine render abort the process; bundling is the
# simplest fix. See cmake/BundleQt653.cmake for the bundling logic.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets)
# Always force AUTOMOC/AUTORCC/AUTOUIC on explicitly. qt_standard_project_setup()
# is Qt 6.3+ and should set these on its own, but in practice (and when
# the helper is missing from older Qt installs) the variables can end up
# uninitialized, which silently breaks Q_OBJECT linking. Setting them
# unconditionally here is safe — qt_standard_project_setup() also sets
# the same values, so this is just a redundant belt-and-braces.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(FetchContent)
# Prefer the local cmark-gfm source tree if it already exists (so builds
# work on machines without GitHub network access). Falls back to the
# upstream repo when the local copy isn't present.
set(_cmark_gfm_local "${CMAKE_SOURCE_DIR}/_deps/cmark-gfm-src")
if(EXISTS "${_cmark_gfm_local}/CMakeLists.txt")
    message(STATUS "MDReader: using local cmark-gfm source at ${_cmark_gfm_local}")
    FetchContent_Declare(
        cmark-gfm
        SOURCE_DIR "${_cmark_gfm_local}"
    )
else()
    FetchContent_Declare(
        cmark-gfm
        GIT_REPOSITORY https://github.com/github/cmark-gfm.git
        GIT_TAG 0.29.0.gfm.13
        GIT_SHALLOW TRUE
    )
endif()
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

if(UNIX AND NOT APPLE)
    include(cmake/LinuxDeb.cmake OPTIONAL)
endif()

set(CPACK_PACKAGE_NAME "MDReader")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "MDReader")
set(CPACK_PACKAGE_CONTACT "MDReader <noreply@example.com>")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_FILE_NAME "MDReader-${PROJECT_VERSION}-Linux.deb")
# Phase 8: the deb ships a self-contained Qt 6.5.3 under
# /usr/lib/mdreader/qt6/, so the libqt6* deps below are mostly
# cosmetic — but apt still uses them for install-time verification.
# libxcb-cursor0 is required by Qt 6.5+ to load the xcb platform plugin.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6gui6, libqt6widgets6, libqt6webenginewidgets6, libqt6concurrent6, libqt6webenginecore6, libqt6webengine6-data, libqt6quick6, libqt6quickwidgets6, libqt6qml6, libqt6qmlmodels6, libqt6network6, libqt6printsupport6, libqt6dbus6, libqt6webchannel6, libqt6positioning6, libqt6opengl6, libxcb-cursor0")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Markdown reader/editor")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "MDReader ${PROJECT_VERSION}")
    set(CPACK_NSIS_FILE_NAME "MDReader-${PROJECT_VERSION}-win64.exe")
endif()

include(CPack)
