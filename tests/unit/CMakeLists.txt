# Qt Test helper: every test source becomes its own executable.
# This makes failures isolated and lets us use ctest per-test.

find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

set(UNIT_TESTS
    SanityTest
    CmarkSmokeTest
    DocumentTest
    DocumentLockTest
)

# Per-test extra implementation sources (the foreach only globs the test's
# own .cpp; any library code a test exercises must be listed here).
set(TEST_EXTRA_SOURCES_DocumentTest     ${CMAKE_SOURCE_DIR}/src/documents/Document.cpp)
set(TEST_EXTRA_SOURCES_DocumentLockTest ${CMAKE_SOURCE_DIR}/src/documents/Document.cpp)

foreach(test_name ${UNIT_TESTS})
    qt_add_executable(${test_name} ${test_name}.cpp ${TEST_EXTRA_SOURCES_${test_name}})
    target_link_libraries(${test_name} PRIVATE Qt6::Core Qt6::Test libcmark-gfm_static)
    # cmark-gfm generates headers (cmark-gfm_export.h, cmark-gfm_version.h,
    # config.h) into BINARY_DIR; the source headers live in SOURCE_DIR/src.
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${cmark-gfm_SOURCE_DIR}/src
        ${cmark-gfm_BINARY_DIR}/src)
    add_test(NAME unit_${test_name} COMMAND ${test_name})
endforeach()
