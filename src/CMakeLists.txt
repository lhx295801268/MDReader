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
    services/OutlineExtractor.h services/OutlineExtractor.cpp
    services/WordCounter.h services/WordCounter.cpp
    ui/PreviewView.h ui/PreviewView.cpp
    ui/EditorView.h ui/EditorView.cpp
    ui/InfoView.h ui/InfoView.cpp
    ui/OutlineView.h ui/OutlineView.cpp
    ui/DocumentTab.h ui/DocumentTab.cpp
    ui/ThemeMenu.h ui/ThemeMenu.cpp
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