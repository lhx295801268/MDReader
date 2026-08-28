cmake_minimum_required(VERSION 3.21)
# cmark-gfm's CMakeLists.txt uses `cmake_minimum_required(VERSION 3.0)`,
# which CMake >= 4.0 rejects by default. Setting CMAKE_POLICY_VERSION_MINIMUM
# to 3.5 downgrades the policy floor just enough to accept it.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(MDReader VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets)
# qt_standard_project_setup() is Qt 6.3+; on 6.2 we set the same flags manually.
if(NOT COMMAND qt_standard_project_setup)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

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
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6gui6, libqt6widgets6, libqt6webenginewidgets6, libqt6concurrent6, libqt6webenginecore6, libqt6webengine6-data, libqt6quick6, libqt6quickwidgets6, libqt6qml6, libqt6qmlmodels6, libqt6network6, libqt6printsupport6, libqt6dbus6, libqt6webchannel6, libqt6positioning6, libqt6opengl6")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Markdown reader/editor")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "MDReader ${PROJECT_VERSION}")
    set(CPACK_NSIS_FILE_NAME "MDReader-${PROJECT_VERSION}-win64.exe")
endif()

include(CPack)
