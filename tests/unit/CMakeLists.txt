# Qt Test helper: every test source becomes its own executable.
# This makes failures isolated and lets us use ctest per-test.

find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

set(UNIT_TESTS
    SanityTest
)

foreach(test_name ${UNIT_TESTS})
    qt_add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE Qt6::Core Qt6::Test)
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME unit_${test_name} COMMAND ${test_name})
endforeach()
