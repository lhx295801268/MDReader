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
    ThemeSwitchTest
    ExternalReloadTest
    RenderCoordinatorWorkerTest
    grab_screenshots
)

# Per-test extra implementation sources (the foreach only globs the test's
# own .cpp; any library code a test exercises must be listed here).
set(TEST_EXTRA_SOURCES_HtmlExportTest
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/LoadingOverlay.cpp)

# Task 27: EditToPreviewTest exercises the full UI stack (EditorView,
# DocumentTab, PreviewView, OutlineView, InfoView) plus the render pipeline
# (RenderCoordinator, MarkdownRenderer, OutlineExtractor, WordCounter),
# Document, and ImageHandler (EditorView::insertFromMimeData uses it).
set(TEST_EXTRA_SOURCES_EditToPreviewTest
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/LoadingOverlay.cpp
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

# Task 32: grab_screenshots builds a DocumentTab for each (fixture, theme)
# pair, drives a render through RenderCoordinator, then QWidget::grab()s
# the rendered preview into docs/screenshots/*.png. Used to generate
# smoke-test screenshots under offscreen QPA.
set(TEST_EXTRA_SOURCES_grab_screenshots
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/LoadingOverlay.cpp
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

set(TEST_EXTRA_LIBS_grab_screenshots
    Qt6::WebEngineWidgets Qt6::Widgets Qt6::Concurrent)

# Task 27a: ThemeSwitchTest renders Markdown with two different theme names
# and asserts the resulting HTML differs. Exercises MarkdownRenderer (which
# uses OutlineExtractor::slugify to set heading ids during wrapHtml) — no
# UI / web engine stack required, but the libcmark-gfm-extensions link is
# still required because MarkdownRenderer pulls in the table extension.
set(TEST_EXTRA_SOURCES_ThemeSwitchTest
    ${CMAKE_SOURCE_DIR}/src/services/MarkdownRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/services/OutlineExtractor.cpp)

set(TEST_EXTRA_LIBS_ThemeSwitchTest
    libcmark-gfm-extensions_static)

# Task 27b: ExternalReloadTest exercises the worker read path only —
# DocumentManager::openFile dispatches the file read to a worker thread
# (QtConcurrent via WorkerThread.h) and emits documentLoaded back on the
# main thread. The 3-button conflict modal that fires on dirty edits is
# intentionally NOT exercised here (headless QMessageBox is brittle);
# we just assert that openFile completes and the loaded Document carries
# the file bytes. No UI stack needed, so no preview/edit/tab sources.
set(TEST_EXTRA_SOURCES_ExternalReloadTest
    ${CMAKE_SOURCE_DIR}/src/documents/Document.cpp
    ${CMAKE_SOURCE_DIR}/src/documents/DocumentManager.cpp)

# WorkerThread.h pulls in Qt6::Concurrent (QtConcurrent::run / QThreadPool);
# DocumentManager.cpp uses it directly. The default foreach already links
# Qt6::Widgets (harmless) — only Concurrent needs to be added explicitly.
set(TEST_EXTRA_LIBS_ExternalReloadTest
    Qt6::Concurrent)

# Task 27c: RenderCoordinatorWorkerTest asserts that requestRender's
# enqueue path does not block the main thread. We bind a DocumentTab
# (so PreviewView is live) and time the requestRender call only — we
# do NOT wait for the worker to finish, so we only need sources for the
# main-thread stack: Document + DocumentTab + its four children
# (Editor/Preview/Outline/Info views) + the coordinator. RenderCoordinator
# also pulls MarkdownRenderer / OutlineExtractor / WordCounter via
# WorkerThread.h, and EditorView pulls ImageHandler. MarkdownRenderer in
# turn references the cmark-gfm table extension, so we need the same
# cmark libs as EditToPreviewTest (already in the default foreach).
set(TEST_EXTRA_SOURCES_RenderCoordinatorWorkerTest
    ${CMAKE_SOURCE_DIR}/src/documents/Document.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/DocumentTab.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/LoadingOverlay.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/EditorView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/OutlineView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/InfoView.cpp
    ${CMAKE_SOURCE_DIR}/src/services/RenderCoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/services/MarkdownRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/services/OutlineExtractor.cpp
    ${CMAKE_SOURCE_DIR}/src/services/WordCounter.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ImageHandler.cpp)

# DocumentTab pulls Qt6::Widgets; RenderCoordinator's worker path pulls
# Qt6::Concurrent (WorkerThread.h). PreviewView pulls
# Qt6::WebEngineWidgets. Widgets and WebEngineWidgets are already in the
# default foreach, but Concurrent must be added explicitly.
set(TEST_EXTRA_LIBS_RenderCoordinatorWorkerTest
    Qt6::Concurrent)

# cmark-gfm-extensions references cmark-gfm symbols (e.g. cmark_arena_push),
# so extensions must precede cmark-gfm_static on the link line. This matches
# src/CMakeLists.txt. Unused by HtmlExportTest but cheap to keep here.
set(_INTEGRATION_CMARK_LIBS libcmark-gfm-extensions_static libcmark-gfm_static)

# MarkdownRenderer::wrapHtml embeds resources via the `:/themes/<name>.css`,
# `:/vendor/highlight/highlight.min.js`, and `:/vendor/mathjax/tex-mml-chtml.js`
# qrc paths. Without registering these on the test executable, the rendered
# preview HTML is broken (no theme CSS, no syntax highlighting, no MathJax),
# which cascades into ThemeSwitchTest assertions. Mirror src/CMakeLists.txt.
set(_INTEGRATION_RESOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/resources/themes/github.css
    ${CMAKE_SOURCE_DIR}/src/resources/themes/github-dark.css
    ${CMAKE_SOURCE_DIR}/src/resources/themes/dracula.css
    ${CMAKE_SOURCE_DIR}/src/resources/themes/solarized-light.css
    ${CMAKE_SOURCE_DIR}/src/resources/themes/solarized-dark.css
    ${CMAKE_SOURCE_DIR}/src/resources/themes/one-dark.css
    ${CMAKE_SOURCE_DIR}/src/resources/vendor/highlight/highlight.min.js
    ${CMAKE_SOURCE_DIR}/src/resources/vendor/mathjax/tex-mml-chtml.js
)

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
    qt_add_resources(${test_name} "integration_${test_name}_resources"
        PREFIX "/"
        BASE "${CMAKE_SOURCE_DIR}/src/resources"
        FILES ${_INTEGRATION_RESOURCE_FILES})
    add_test(NAME integration_${test_name} COMMAND ${test_name})
endforeach()

# Integration tests use QtWebEngineWidgets, which needs a GUI platform.
# Use offscreen in headless CI environments (libxcb-cursor0 is missing here).
set_tests_properties(integration_HtmlExportTest integration_EditToPreviewTest integration_ThemeSwitchTest integration_ExternalReloadTest integration_RenderCoordinatorWorkerTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# integration_grab_screenshots is a manual helper, not a CI test: QWebEngineView
# does not paint under the offscreen QPA platform that CI uses, so the helper's
# load wait would time out, produce no PNGs, and fail the run. Keep it DISABLED
# for ctest and invoke the built `grab_screenshots` binary directly from a
# workstation with a real display (QT_QPA_PLATFORM=xcb) when the screenshots in
# docs/screenshots/ need regenerating.
set_tests_properties(integration_grab_screenshots PROPERTIES DISABLED ON)