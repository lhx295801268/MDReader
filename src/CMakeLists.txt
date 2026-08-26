find_package(Qt6 6.5 REQUIRED COMPONENTS Concurrent)

qt_add_executable(mdreader
    app/main.cpp
    documents/Document.h documents/Document.cpp
    services/WorkerThread.h
    services/MarkdownRenderer.h services/MarkdownRenderer.cpp
    services/ImageHandler.h services/ImageHandler.cpp
    services/RenderCoordinator.h services/RenderCoordinator.cpp
    ui/PreviewView.h ui/PreviewView.cpp
    ui/EditorView.h ui/EditorView.cpp
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