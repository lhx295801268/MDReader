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
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/vendor/mermaid/mermaid.min.js"
        # First-party glue, lives beside the bundle it drives.
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/vendor/mermaid/mermaid-init.js"
        # First-party preview viewer: zoom/pan overlay for diagrams + images.
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/viewer/viewer-overlay.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/viewer/preview-zoom.js"
)

# The application icon, used for the window / task-bar entry. On macOS the
# bundle's own CFBundleIconFile (packaging/mdreader.icns) wins; this PNG
# is what Linux and Windows show.
qt_add_resources(mdreader "mdreader_app_resources"
    PREFIX "/app"
    BASE "${CMAKE_SOURCE_DIR}/packaging"
    FILES
        "${CMAKE_SOURCE_DIR}/packaging/mdreader-1024.png"
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

# The version lives in one place: the top-level project(). main.cpp reads it
# from here instead of hard-coding a string that would silently go stale.
target_compile_definitions(mdreader PRIVATE
    MDREADER_VERSION="${PROJECT_VERSION}")

if(APPLE)
    # --------------------------------------------------------------- .app
    # On macOS the executable becomes a real application bundle: the app menu
    # lives in the system menu bar, the icon comes from Contents/Resources,
    # and "Open With" in Finder needs a CFBundleDocumentTypes entry. All of
    # that is bundle metadata, so it is configured here rather than in code.
    set_target_properties(mdreader PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "MDReader"
        MACOSX_BUNDLE_DISPLAY_NAME "MDReader"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.mdreader.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "mdreader.icns"
        MACOSX_BUNDLE_COPYRIGHT "MDReader contributors · MIT"
        MACOSX_BUNDLE_INFO_STRING "MDReader ${PROJECT_VERSION} · Markdown reader/editor"
        # The bundle *directory* is named after the target's OUTPUT_NAME, so
        # it defaults to "mdreader.app". The .dmg is supposed to show
        # "MDReader.app" (cmake/MacDmg.cmake looks for exactly that name).
        # The executable inside the bundle follows the same name.
        OUTPUT_NAME "MDReader"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/Info.plist.in")

    # The icon has to sit in Contents/Resources for CFBundleIconFile to find
    # it. packaging/mdreader.icns is checked in (it is a binary asset, and
    # generating it needs macOS-only tools) — cmake/GenerateIcns.cmake
    # regenerates it from the 1024px PNG on a Mac.
    target_sources(mdreader PRIVATE "${CMAKE_SOURCE_DIR}/packaging/mdreader.icns")
    set_source_files_properties("${CMAKE_SOURCE_DIR}/packaging/mdreader.icns"
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    # The bundle goes to the root of the payload so the .dmg shows a single
    # draggable "MDReader.app" next to the Applications alias.
    # BUNDLE (not RUNTIME) is the destination kind for a MACOSX_BUNDLE
    # target: CMake errors out with "install TARGETS given no BUNDLE
    # DESTINATION" otherwise, and the .app never reaches the staging
    # area where macdeployqt expects to find it.
    install(TARGETS mdreader BUNDLE DESTINATION .)

    # Convenience target for regenerating the icon on a Mac:
    #   cmake --build build --target mdreader_regen_icon
    add_custom_target(mdreader_regen_icon
        COMMAND ${CMAKE_COMMAND}
                -DSRC_PNG=${CMAKE_SOURCE_DIR}/packaging/mdreader-1024.png
                -DOUT_ICNS=${CMAKE_SOURCE_DIR}/packaging/mdreader.icns
                -P ${CMAKE_SOURCE_DIR}/cmake/GenerateIcns.cmake
        COMMENT "Regenerating packaging/mdreader.icns from the 1024px PNG")
else()
    install(TARGETS mdreader RUNTIME DESTINATION bin)
endif()

# Phase 8: bundle Qt 6.5.3 webengine runtime alongside the binary so the
# deb is self-contained. Runs at install(CODE ...) time so it sees the
# freshly-staged mdreader at ${CMAKE_INSTALL_PREFIX}/bin/mdreader.
# Set QT653_SRC_ROOT to the local Qt SDK root when configuring; the
# default below auto-discovers /home/lhx/Qt/6.5.3/gcc_64 for the user's
# dev setup. Override with -DQT653_SRC_ROOT=/path/to/Qt/6.5.x/gcc_64 for
# other machines.
#
# Linux only: on macOS the equivalent job is done by macdeployqt (see
# cmake/MacDmg.cmake), which copies the frameworks into the .app bundle.
if(NOT APPLE AND NOT DEFINED QT653_SRC_ROOT)
    set(_qt653_default "/home/lhx/Qt/6.5.3/gcc_64")
    if(EXISTS "${_qt653_default}/lib/cmake/Qt6/Qt6Config.cmake")
        set(QT653_SRC_ROOT "${_qt653_default}")
    endif()
endif()
if(NOT APPLE AND DEFINED QT653_SRC_ROOT AND EXISTS "${QT653_SRC_ROOT}/lib/cmake/Qt6/Qt6Config.cmake")
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