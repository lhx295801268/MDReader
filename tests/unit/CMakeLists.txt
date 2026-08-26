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
    ImageHandlerTest
    RenderFrameDropTest
)

# Per-test extra implementation sources (the foreach only globs the test's
# own .cpp; any library code a test exercises must be listed here).
set(TEST_EXTRA_SOURCES_DocumentTest     ${CMAKE_SOURCE_DIR}/src/documents/Document.cpp)
set(TEST_EXTRA_SOURCES_DocumentLockTest ${CMAKE_SOURCE_DIR}/src/documents/Document.cpp)
set(TEST_EXTRA_SOURCES_MarkdownRendererTest ${CMAKE_SOURCE_DIR}/src/services/MarkdownRenderer.cpp)
set(TEST_EXTRA_SOURCES_ImageHandlerTest ${CMAKE_SOURCE_DIR}/src/services/ImageHandler.cpp)
set(TEST_EXTRA_SOURCES_RenderFrameDropTest
    ${CMAKE_SOURCE_DIR}/src/services/RenderCoordinator.cpp
    ${CMAKE_SOURCE_DIR}/src/services/MarkdownRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewView.cpp)
# (WorkerThread is header-only, no .cpp needed)

# WorkerThreadTest exercises QtConcurrent::run, so it needs Qt6::Concurrent.
# Must be set BEFORE the foreach loop, since target_link_libraries inside
# the loop expands ${TEST_EXTRA_LIBS_${test_name}} at config time.
set(TEST_EXTRA_LIBS_WorkerThreadTest Qt6::Concurrent)

# MarkdownRendererTest uses cmark-gfm's GFM table extension; the registration
# function lives in libcmark-gfm-extensions_static. QThread::create is in
# Qt6::Core (already linked in the foreach), no Qt6::Concurrent needed.
set(TEST_EXTRA_LIBS_MarkdownRendererTest libcmark-gfm-extensions_static)

# RenderCoordinator.h transitively includes ui/PreviewView.h → QWebEngineView.
# WorkerThread.h pulls in QtConcurrent for runOnWorker/postToMain.
# MarkdownRenderer pulls in libcmark-gfm-extensions_static.
set(TEST_EXTRA_LIBS_RenderFrameDropTest
    Qt6::WebEngineWidgets Qt6::Widgets Qt6::Concurrent
    libcmark-gfm-extensions_static)

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