# macOS packaging: .app bundle + .dmg
#
# Included from the top-level CMakeLists under if(APPLE).
#
# Two things happen here:
#
#   1. macdeployqt runs during the install step. It copies the Qt frameworks,
#      the QWebEngine helper process and the .pak resources into the bundle
#      so the .app runs on a machine that has no Qt installed — the same job
#      BundleQt653.cmake does for the Linux .deb.
#
#   2. CPack is configured to produce a drag-and-drop .dmg (plus a .tar.gz
#      for people who prefer it).
#
# The bundle itself is created by the MACOSX_BUNDLE target properties in
# src/CMakeLists.txt; this file only fills it in and packages it.

# ---------------------------------------------------------------- macdeployqt

# macdeployqt lives in the same bin/ directory as the qmake this build was
# configured against, so derive it from Qt6Core_DIR (<qt>/lib/cmake/Qt6Core)
# rather than trusting PATH to point at the same Qt.
get_filename_component(_mdreader_qt_root "${Qt6Core_DIR}/../../.." ABSOLUTE)
find_program(MACDEPLOYQT_EXECUTABLE
    NAMES macdeployqt
    HINTS "${_mdreader_qt_root}/bin"
    DOC "Qt's macOS deployment tool")
if(NOT MACDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR
        "MDReader: macdeployqt not found. It ships with the Qt SDK; point "
        "CMAKE_PREFIX_PATH at the same Qt this build uses, or add its bin/ "
        "directory to PATH.")
endif()
message(STATUS "MDReader: macdeployqt = ${MACDEPLOYQT_EXECUTABLE}")

# The bundle name has to match what src/CMakeLists.txt installs, and the
# executable inside it is the target name.
set(MACOSX_BUNDLE_NAME "MDReader")
set(MACOSX_BUNDLE_EXECUTABLE "mdreader")

# Runs inside the install step, i.e. while CPack is staging the payload into
# its temporary directory. CMAKE_INSTALL_PREFIX points at that staging
# directory at this point, which is exactly where the bundle was installed.
install(CODE "
  set(_bundle \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app\")
  if(NOT EXISTS \"\${_bundle}\")
    message(FATAL_ERROR \"MDReader: app bundle not found at \${_bundle}\")
  endif()
  message(STATUS \"MDReader: running macdeployqt on \${_bundle}\")
  execute_process(
    COMMAND \"${MACDEPLOYQT_EXECUTABLE}\" -always-overwrite \"\${_bundle}\"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR \"MDReader: macdeployqt failed (\${_rc}):\n\${_out}\n\${_err}\")
  endif()
  # The Qt WebEngine helper process is a separate executable that ships next
  # to the app. It is the one that renders the preview, so a bundle missing it
  # starts fine and then fails on the first document — worth failing the
  # build here instead.
  if(NOT EXISTS \"\${_bundle}/Contents/Frameworks/QtWebEngineCore.framework\")
    message(WARNING \"MDReader: QtWebEngineCore.framework is missing from the bundle\")
  endif()
  # macdeployqt rewrites the bundle's resources, which invalidates any
  # signature that was on it. Re-sign ad-hoc so the bundle is internally
  # consistent — macOS refuses to launch a bundle whose resources were
  # modified after signing. (A real release would sign with a Developer ID
  # certificate and notarize; see the README.)
  find_program(CODESIGN_EXECUTABLE codesign)
  if(CODESIGN_EXECUTABLE)
    message(STATUS \"MDReader: ad-hoc signing \${_bundle}\")
    execute_process(
      COMMAND \"${CODESIGN_EXECUTABLE}\" --deep --force --sign - \"\${_bundle}\"
      RESULT_VARIABLE _rc
      OUTPUT_QUIET
      ERROR_QUIET)
    if(NOT _rc EQUAL 0)
      message(WARNING \"MDReader: ad-hoc signing failed (\${_rc}) — the app bundle may refuse to launch\")
    endif()
  endif()
")

# --------------------------------------------------------------------- CPack

set(CPACK_GENERATOR "DragNDrop;TGZ")
set(CPACK_PACKAGE_FILE_NAME "MDReader-${PROJECT_VERSION}")
set(CPACK_DMG_FILE_NAME "MDReader-${PROJECT_VERSION}-macOS.dmg")
set(CPACK_DMG_VOLUME_NAME "MDReader ${PROJECT_VERSION}")
set(CPACK_DMG_FORMAT "UDZO")
# Drag the .app onto the Applications alias CPack puts on the volume.
# The name is relative to the volume root — a leading slash would make
# CPack look for an absolute path and silently skip the alias.
set(CPACK_DMG_APPLICATIONS_FOLDER_NAME "Applications")
# Show the .app and the alias only; no .DS_Store noise from the build
# machine.
set(CPACK_DMG_HIDDEN_FILES OFF)
