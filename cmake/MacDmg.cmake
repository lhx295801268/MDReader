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

# codesign is looked up here, at configure time, because the install script
# below is *generated* now: a variable referenced in its COMMAND line is
# expanded when the script is written, so a find_program() inside the
# script itself would leave an empty command name behind.
find_program(CODESIGN_EXECUTABLE codesign
    DOC "macOS code signing tool (used for the ad-hoc signature)")

# Escape hatch for the self-containment check below: a build against a Qt
# that does not deploy cleanly (Homebrew, /usr/local) produces a .dmg that
# only runs where that Qt is installed.
option(MDREADER_ALLOW_EXTERNAL_QT_DEPS
    "Allow packaging a .dmg that still depends on the build-time Qt" OFF)

# The bundle name has to match what src/CMakeLists.txt installs, and the
# executable inside it is the target's OUTPUT_NAME (CMake derives both the
# .app directory name and the executable name from it).
set(MACOSX_BUNDLE_NAME "MDReader")
get_target_property(_mdreader_output_name mdreader OUTPUT_NAME)
if(NOT _mdreader_output_name)
    set(_mdreader_output_name "mdreader")
endif()
set(MACOSX_BUNDLE_EXECUTABLE "${_mdreader_output_name}")

# Runs inside the install step, i.e. while CPack is staging the payload into
# its temporary directory. CMAKE_INSTALL_PREFIX points at that staging
# directory at this point, which is exactly where the bundle was installed.
install(CODE "
  # These two are baked into the script at configure time. An install
  # script runs in CMake's script mode, where the cache is not loaded and
  # a *quoted* string in if() is dereferenced as a variable name — so
  # if(\"\${CODESIGN_EXECUTABLE}\") would silently be false and the
  # signing step would never run.
  set(CODESIGN_EXECUTABLE \"${CODESIGN_EXECUTABLE}\")
  set(MDREADER_ALLOW_EXTERNAL_QT_DEPS \"${MDREADER_ALLOW_EXTERNAL_QT_DEPS}\")
  set(_bundle \"\${CMAKE_INSTALL_PREFIX}/${MACOSX_BUNDLE_NAME}.app\")
  if(NOT EXISTS \"\${_bundle}\")
    message(FATAL_ERROR \"MDReader: app bundle not found at \${_bundle}\")
  endif()
  message(STATUS \"MDReader: running macdeployqt on \${_bundle}\")
  # The bundle path must come *before* the options: macdeployqt treats the
  # first argument as the app bundle, so a leading option makes it print its
  # usage text and exit 1 (which then fails the whole package).
  execute_process(
    COMMAND \"${MACDEPLOYQT_EXECUTABLE}\" \"\${_bundle}\" -always-overwrite
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
  # ---------------------------------------------------------------------
  # Self-containment check
  #
  # Every Mach-O dependency of the app and of the WebEngine helper has to
  # resolve *inside* the bundle. Qt builds that are not the official
  # distribution (Homebrew, /usr/local) leave absolute paths such as
  # /opt/homebrew/opt/qtbase/lib/QtGui.framework inside the helper
  # process: the .dmg then only runs on machines that happen to have that
  # same Qt installed, and the preview silently fails to render anywhere
  # else.
  #
  # Try to repair that first (cmake/MacBundleRepair.cmake), then fail the
  # build if anything still points outside — shipping a .dmg that cannot
  # render is worse than not shipping one.
  # The path is expanded when this script is generated (install scripts
  # have no CMAKE_SOURCE_DIR of their own).
  include(\"${CMAKE_SOURCE_DIR}/cmake/MacBundleRepair.cmake\")
  mdreader_external_deps(\"\${_bundle}\" _outside)
  if(_outside)
    message(STATUS \"MDReader: repairing the bundle so that it no longer depends on the build-time Qt\")
    mdreader_repair_helper(\"\${_bundle}\")
    mdreader_external_deps(\"\${_bundle}\" _outside)
  endif()
  if(_outside)
    string(REPLACE \";\" \"\\n  \" _outside_text \"\${_outside}\")
    if(MDREADER_ALLOW_EXTERNAL_QT_DEPS)
      message(WARNING \"MDReader: the bundle is not self-contained — these dependencies resolve outside it:\\n  \${_outside_text}\")
    else()
      message(FATAL_ERROR
        \"MDReader: the bundle is not self-contained — these dependencies resolve outside it:\\n  \${_outside_text}\\n\"
        \"The Qt this build used does not deploy cleanly (the official Qt releases do). \"
        \"Use the official Qt SDK (see the README) or configure with -DMDREADER_ALLOW_EXTERNAL_QT_DEPS=ON to override.\")
    endif()
  endif()
  # macdeployqt rewrites the bundle's resources, which invalidates any
  # signature that was on it. Re-sign ad-hoc so the bundle is internally
  # consistent — macOS refuses to launch a bundle whose resources were
  # modified after signing. (A real release would sign with a Developer ID
  # certificate and notarize; see the README.)
  #
  # The entitlements are not optional: Qt WebEngine is Chromium, and
  # macOS' sandbox refuses to let the renderer JIT and to load the app's
  # own frameworks without them. Without them the app still starts but
  # the preview stays blank — see packaging/entitlements.plist.
  #
  # The path has to be resolved at *configure* time: a find_program() inside
  # this install script would run too late, and the COMMAND line is expanded
  # when the script is generated — an unresolved variable leaves an empty
  # command name, which CMake fails to spawn with \"permission denied\".
  # The condition is expanded when this script is generated: an install
  # script has no CODESIGN_EXECUTABLE of its own, so a literal if()
  # would always be false and the signing step would be skipped silently.
  if(CODESIGN_EXECUTABLE)
    message(STATUS \"MDReader: ad-hoc signing \${_bundle}\")
    execute_process(
      COMMAND \"${CODESIGN_EXECUTABLE}\" --deep --force --sign - --options runtime --entitlements \"${CMAKE_SOURCE_DIR}/packaging/entitlements.plist\" \"\${_bundle}\"
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
# The DragNDrop generator names its image after CPACK_PACKAGE_FILE_NAME
# (CPACK_DMG_FILE_NAME only applies to the "DMG" generator), so the
# "-macOS" suffix has to live here for the artifact to come out as
# MDReader-<version>-macOS.dmg — the name the README and the CI upload
# step expect.
set(CPACK_PACKAGE_FILE_NAME "MDReader-${PROJECT_VERSION}-macOS")
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
