# Integration tests: exercise the UI stack end-to-end (QWebEngineView + the
# MarkdownRenderer / file I/O path). Each test gets its own executable so a
# crash in one doesn't sink the others; ctest runs them with name prefix
# `integration_` so they're easy to filter separately from `unit_`.

find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

# Task 26 registers only HtmlExportTest; the other tests listed in the plan
# (EditToPreviewTest, ThemeSwitchTest, ExternalReloadTest,
# RenderCoordinatorWorkerTest) are added to this list in tasks 27 / 27a /
# 27b / 27c. We deliberately do NOT pre-register them here because
# `qt_add_executable(<name> <name>.cpp)` with a missing source fails at
# configure time — we register each test in the task that creates it.
set(INTEGRATION_TESTS
    HtmlExportTest
)

# Per-test extra implementation sources (the foreach only globs the test's
# own .cpp; any library code a test exercises must be listed here).
set(TEST_EXTRA_SOURCES_HtmlExportTest
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewView.cpp)

foreach(test_name ${INTEGRATION_TESTS})
    qt_add_executable(${test_name} ${test_name}.cpp
        ${TEST_EXTRA_SOURCES_${test_name}})
    target_link_libraries(${test_name} PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::WebEngineWidgets Qt6::Test
        libcmark-gfm_static)
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${cmark-gfm_SOURCE_DIR}/src
        ${cmark-gfm_SOURCE_DIR}/extensions
        ${cmark-gfm_BINARY_DIR}/src)
    add_test(NAME integration_${test_name} COMMAND ${test_name})
endforeach()

# Integration tests use QtWebEngineWidgets, which needs a GUI platform.
# Use offscreen in headless CI environments (libxcb-cursor0 is missing here).
set_tests_properties(integration_HtmlExportTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")