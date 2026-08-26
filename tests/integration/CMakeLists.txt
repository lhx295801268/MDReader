# Integration tests: exercise the UI stack end-to-end (QWebEngineView + the
# MarkdownRenderer / file I/O path). Each test gets its own executable so a
# crash in one doesn't sink the others; ctest runs them with name prefix
# `integration_` so they're easy to filter separately from `unit_`.

find_package(Qt6 6.5 REQUIRED COMPONENTS Test Concurrent)

# Task 26 registers only HtmlExportTest; the other tests listed in the plan
# (EditToPreviewTest, ThemeSwitchTest, ExternalReloadTest,
# RenderCoordinatorWorkerTest) are added to this list in tasks 27 / 27a /
# 27b / 27c. We deliberately do NOT pre-register them here because
# `qt_add_executable(<name> <name>.cpp)` with a missing source fails at
# configure time — we register each test in the task that creates it.
set(INTEGRATION_TESTS
    HtmlExportTest
    EditToPreviewTest
)

# Per-test extra implementation sources (the foreach only globs the test's
# own .cpp; any library code a test exercises must be listed here).
set(TEST_EXTRA_SOURCES_HtmlExportTest
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewView.cpp)

# Task 27: EditToPreviewTest exercises the full UI stack (EditorView,
# DocumentTab, PreviewView, OutlineView, InfoView) plus the render pipeline
# (RenderCoordinator, MarkdownRenderer, OutlineExtractor, WordCounter),
# Document, and ImageHandler (EditorView::insertFromMimeData uses it).
set(TEST_EXTRA_SOURCES_EditToPreviewTest
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/DocumentTab.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/EditorView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/OutlineView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/InfoView.cpp
    ${CMAKE_SOURCE_DIR}/src/services/RenderCoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/services/MarkdownRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/services/OutlineExtractor.cpp
    ${CMAKE_SOURCE_DIR}/src/services/WordCounter.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ImageHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/documents/Document.cpp)

set(TEST_EXTRA_LIBS_EditToPreviewTest
    Qt6::WebEngineWidgets Qt6::Widgets Qt6::Concurrent)

# cmark-gfm-extensions references cmark-gfm symbols (e.g. cmark_arena_push),
# so extensions must precede cmark-gfm_static on the link line. This matches
# src/CMakeLists.txt. Unused by HtmlExportTest but cheap to keep here.
set(_INTEGRATION_CMARK_LIBS libcmark-gfm-extensions_static libcmark-gfm_static)

foreach(test_name ${INTEGRATION_TESTS})
    qt_add_executable(${test_name} ${test_name}.cpp
        ${TEST_EXTRA_SOURCES_${test_name}})
    target_link_libraries(${test_name} PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::WebEngineWidgets Qt6::Test
        ${_INTEGRATION_CMARK_LIBS}
        ${TEST_EXTRA_LIBS_${test_name}})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${cmark-gfm_SOURCE_DIR}/src
        ${cmark-gfm_SOURCE_DIR}/extensions
        ${cmark-gfm_BINARY_DIR}/src)
    add_test(NAME integration_${test_name} COMMAND ${test_name})
endforeach()

# Integration tests use QtWebEngineWidgets, which needs a GUI platform.
# Use offscreen in headless CI environments (libxcb-cursor0 is missing here).
set_tests_properties(integration_HtmlExportTest integration_EditToPreviewTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")