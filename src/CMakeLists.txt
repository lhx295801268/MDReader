qt_add_executable(mdreader
    app/main.cpp
)

target_link_libraries(mdreader PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

target_include_directories(mdreader PRIVATE src)