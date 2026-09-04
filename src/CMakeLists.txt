find_package(Qt6 6.5 REQUIRED COMPONENTS Concurrent)

qt_add_executable(mdreader
    app/main.cpp
    app/MainWindow.h app/MainWindow.cpp
    documents/Document.h documents/Document.cpp
    documents/DocumentManager.h documents/DocumentManager.cpp
    services/WorkerThread.h
    services/MarkdownRenderer.h services/MarkdownRenderer.cpp
    services/ImageHandler.h services/ImageHandler.cpp
    services/RenderCoordinator.h services/RenderCoordinator.cpp
    services/AppTranslator.h services/AppTranslator.cpp
    services/OutlineExtractor.h services/OutlineExtractor.cpp
    services/WordCounter.h services/WordCounter.cpp
    services/FileWatcher.h services/FileWatcher.cpp
    ui/PreviewView.h ui/PreviewView.cpp
    ui/LoadingOverlay.h ui/LoadingOverlay.cpp
    ui/EditorView.h ui/EditorView.cpp
    ui/InfoView.h ui/InfoView.cpp
    ui/OutlineView.h ui/OutlineView.cpp
    ui/DocumentTab.h ui/DocumentTab.cpp
    ui/ThemeMenu.h ui/ThemeMenu.cpp
    ui/FindBar.h ui/FindBar.cpp
)

qt_add_resources(mdreader "mdreader_resources"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/themes/github.css"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/themes/github-dark.css"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/themes/dracula.css"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/themes/solarized-light.css"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/themes/solarized-dark.css"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/themes/one-dark.css"
        # --- vendored 3rd-party assets (see scripts/fetch_vendor.sh) ---
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/vendor/highlight/highlight.min.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/vendor/mathjax/tex-mml-chtml.js"
)

target_link_libraries(mdreader PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::WebEngineWidgets
    Qt6::Concurrent
    libcmark-gfm-extensions_static libcmark-gfm_static
)

target_include_directories(mdreader PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${cmark-gfm_SOURCE_DIR}/src
    ${cmark-gfm_SOURCE_DIR}/extensions
    ${cmark-gfm_BINARY_DIR}/src
)

install(TARGETS mdreader RUNTIME DESTINATION bin)

# Phase 8: bundle Qt 6.5.3 webengine runtime alongside the binary so the
# deb is self-contained. Runs at install(CODE ...) time so it sees the
# freshly-staged mdreader at ${CMAKE_INSTALL_PREFIX}/bin/mdreader.
# Set QT653_SRC_ROOT to the local Qt SDK root when configuring; the
# default below auto-discovers /home/lhx/Qt/6.5.3/gcc_64 for the user's
# dev setup. Override with -DQT653_SRC_ROOT=/path/to/Qt/6.5.x/gcc_64 for
# other machines.
if(NOT DEFINED QT653_SRC_ROOT)
    set(_qt653_default "/home/lhx/Qt/6.5.3/gcc_64")
    if(EXISTS "${_qt653_default}/lib/cmake/Qt6/Qt6Config.cmake")
        set(QT653_SRC_ROOT "${_qt653_default}")
    endif()
endif()
if(DEFINED QT653_SRC_ROOT AND EXISTS "${QT653_SRC_ROOT}/lib/cmake/Qt6/Qt6Config.cmake")
    install(CODE "
        set(QT653_SRC_ROOT \"${QT653_SRC_ROOT}\")
        # At install time the actual on-disk prefix is $DESTDIR/$prefix,
        # because CMake applies DESTDIR when staging (so a single source
        # tree can produce multiple debs without rebuilding). $ENV{DESTDIR}
        # is the env var the user (or CPack) passes to `cmake --install`;
        # default to empty when unset.
        if(\"\$ENV{DESTDIR}\" STREQUAL \"\")
            set(STAGE_DIR \"\${CMAKE_INSTALL_PREFIX}\")
        else()
            set(STAGE_DIR \"\$ENV{DESTDIR}/\${CMAKE_INSTALL_PREFIX}\")
        endif()
        set(QT653_BIN_SRC \"${CMAKE_BINARY_DIR}/bin/mdreader\")
        include(\"${CMAKE_SOURCE_DIR}/cmake/BundleQt653.cmake\")
    ")
endif()