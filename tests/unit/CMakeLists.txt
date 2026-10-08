# Qt Test helper: every test source becomes its own executable.
# This makes failures isolated and lets us use ctest per-test.

find_package(Qt6 6.5 REQUIRED COMPONENTS Test Concurrent)

set(UNIT_TESTS
    SanityTest
    CmarkSmokeTest
    DocumentTest
    DocumentLockTest
    WorkerThreadTest
    MarkdownRendererTest
    MarkdownRendererHeadingIdTest
    ImageHandlerTest
    RenderFrameDropTest
    WordCounterTest
    OutlineExtractorTest
    FileWatcherDebounceTest
    ThemeMenuTest
    MainWindowThemeTest
    DropHandlerTest
    EditorPaletteTest
    FindBarTest
    TitleBarFullscreenTest
    MermaidThemeTest
    MermaidInjectionTest
    HtmlInlinerTest
)

# Per-test extra implementation sources (the foreach only globs the test's
# own .cpp; any library code a test exercises must be listed here).
set(TEST_EXTRA_SOURCES_DocumentTest     ${CMAKE_SOURCE_DIR}/src/documents/Document.cpp)
set(TEST_EXTRA_SOURCES_DocumentLockTest ${CMAKE_SOURCE_DIR}/src/documents/Document.cpp)
set(TEST_EXTRA_SOURCES_MarkdownRendererTest
    ${CMAKE_SOURCE_DIR}/src/services/MarkdownRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/services/OutlineExtractor.cpp)
set(TEST_EXTRA_SOURCES_MarkdownRendererHeadingIdTest
    ${CMAKE_SOURCE_DIR}/src/services/MarkdownRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/services/OutlineExtractor.cpp)
set(TEST_EXTRA_SOURCES_ImageHandlerTest ${CMAKE_SOURCE_DIR}/src/services/ImageHandler.cpp)
set(TEST_EXTRA_SOURCES_WordCounterTest ${CMAKE_SOURCE_DIR}/src/services/WordCounter.cpp)
set(TEST_EXTRA_SOURCES_OutlineExtractorTest ${CMAKE_SOURCE_DIR}/src/services/OutlineExtractor.cpp)
set(TEST_EXTRA_SOURCES_FileWatcherDebounceTest ${CMAKE_SOURCE_DIR}/src/services/FileWatcher.cpp)
set(TEST_EXTRA_SOURCES_ThemeMenuTest
    ${CMAKE_SOURCE_DIR}/src/ui/ThemeMenu.cpp)
# (MainWindowThemeTest only needs the header-only ThemeResolution.h — no .cpp.)
set(TEST_EXTRA_SOURCES_RenderFrameDropTest
    ${CMAKE_SOURCE_DIR}/src/services/RenderCoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/services/MarkdownRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/services/OutlineExtractor.cpp
    ${CMAKE_SOURCE_DIR}/src/services/WordCounter.cpp
    ${CMAKE_SOURCE_DIR}/src/services/ImageHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/documents/Document.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/LoadingOverlay.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/DocumentTab.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/EditorView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/OutlineView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/InfoView.cpp)
# (WorkerThread is header-only, no .cpp needed)

# WorkerThreadTest exercises QtConcurrent::run, so it needs Qt6::Concurrent.
# Must be set BEFORE the foreach loop, since target_link_libraries inside
# the loop expands ${TEST_EXTRA_LIBS_${test_name}} at config time.
set(TEST_EXTRA_LIBS_WorkerThreadTest Qt6::Concurrent)
set(TEST_EXTRA_LIBS_FileWatcherDebounceTest Qt6::Concurrent)

# MarkdownRendererTest uses cmark-gfm's GFM table extension; the registration
# function lives in libcmark-gfm-extensions_static. QThread::create is in
# Qt6::Core (already linked in the foreach), no Qt6::Concurrent needed.
set(TEST_EXTRA_LIBS_MarkdownRendererTest libcmark-gfm-extensions_static)
set(TEST_EXTRA_LIBS_MarkdownRendererHeadingIdTest libcmark-gfm-extensions_static)

# RenderCoordinator.h transitively includes ui/PreviewView.h → QWebEngineView.
# WorkerThread.h pulls in QtConcurrent for runOnWorker/postToMain.
# MarkdownRenderer pulls in libcmark-gfm-extensions_static.
set(TEST_EXTRA_LIBS_RenderFrameDropTest
    Qt6::WebEngineWidgets Qt6::Widgets Qt6::Concurrent
    libcmark-gfm-extensions_static)

# ThemeMenu is a QMenu (QWidget); QActionGroup + addAction require
# Qt6::Widgets. Test exercises signals/slots programmatically — works
# under QT_QPA_PLATFORM=offscreen.
set(TEST_EXTRA_LIBS_ThemeMenuTest Qt6::Widgets)
# DropHandler is header-only; no extra sources. Test uses QtTest + QMimeData
# (already in Qt6::Core via QObject linkage).
set(TEST_EXTRA_LIBS_DropHandlerTest Qt6::Widgets)
# FindBar lives under src/ui/ and embeds QLineEdit/QLabel/QToolButton, so
# it needs Qt6::Widgets to link. FindBar.cpp itself is bundled into the
# test below (TEST_EXTRA_SOURCES_FindBarTest).
set(TEST_EXTRA_SOURCES_FindBarTest
    ${CMAKE_SOURCE_DIR}/src/ui/FindBar.cpp)
set(TEST_EXTRA_LIBS_FindBarTest Qt6::Widgets)
# EditorPalette is header-only but uses QColor (QtGui), not just QObject.
# The header-only function table lives in src/app/, but QColor's API is in
# QtGui — without Qt6::Gui in the include path the test won't compile.
set(TEST_EXTRA_LIBS_EditorPaletteTest Qt6::Gui)

# MermaidInjectionTest asserts on what wrapHtml() puts in the page, so it
# needs the renderer itself (plus OutlineExtractor for the heading-id pass
# that builds the body it inspects).
set(TEST_EXTRA_SOURCES_MermaidInjectionTest
    ${CMAKE_SOURCE_DIR}/src/services/MarkdownRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/services/OutlineExtractor.cpp)
set(TEST_EXTRA_LIBS_MermaidInjectionTest libcmark-gfm-extensions_static)

# MermaidTheme.h and HtmlInliner.h are header-only over QString/QFile — the
# default Qt6::Core linkage is enough. Both are listed here for symmetry so a
# future change that pulls in QtGui/QtWidgets has an obvious place to land.
set(TEST_EXTRA_LIBS_MermaidThemeTest Qt6::Core)
set(TEST_EXTRA_LIBS_HtmlInlinerTest Qt6::Core)

foreach(test_name ${UNIT_TESTS})
    qt_add_executable(${test_name} ${test_name}.cpp ${TEST_EXTRA_SOURCES_${test_name}})
    # libcmark-gfm_static must be listed AFTER any extension libs that depend
    # on it (libcmark-gfm-extensions_static pulls in arena.c via
    # cmark_arena_push/pop). With static libs the linker scans left-to-right
    # and only pulls objects that resolve currently-undefined symbols, so
    # core must be last to satisfy extension references.
    target_link_libraries(${test_name} PRIVATE
        Qt6::Core Qt6::Test ${TEST_EXTRA_LIBS_${test_name}} libcmark-gfm_static)
    # cmark-gfm generates headers (cmark-gfm_export.h, cmark-gfm_version.h,
    # config.h) into BINARY_DIR; the source headers live in SOURCE_DIR/src.
    # cmark-gfm-core-extensions.h lives in SOURCE_DIR/extensions.
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${cmark-gfm_SOURCE_DIR}/src
        ${cmark-gfm_SOURCE_DIR}/extensions
        ${cmark-gfm_BINARY_DIR}/src)
    add_test(NAME unit_${test_name} COMMAND ${test_name})
endforeach()

# RenderFrameDropTest pulls in QtWebEngineWidgets, which needs a GUI platform.
# Use offscreen in headless CI environments (libxcb-cursor0 is missing here).
set_tests_properties(unit_RenderFrameDropTest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")