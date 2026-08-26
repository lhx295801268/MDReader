find_package(Qt6 6.5 REQUIRED COMPONENTS Concurrent)

qt_add_executable(mdreader
    app/main.cpp
    documents/Document.h documents/Document.cpp
    services/WorkerThread.h
)

target_link_libraries(mdreader PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
)

target_include_directories(mdreader PRIVATE ${CMAKE_SOURCE_DIR}/src)
