qt_add_executable(mdreader
    app/main.cpp
    documents/Document.h documents/Document.cpp
)

target_link_libraries(mdreader PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

target_include_directories(mdreader PRIVATE ${CMAKE_SOURCE_DIR}/src)
