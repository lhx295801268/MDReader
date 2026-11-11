# Helpers for making the macOS .app bundle self-contained.
#
# Included by cmake/MacDmg.cmake from its install(CODE ...) block, i.e. at
# install time, when the bundle is already staged in its final location.
#
# Background: Qt distributions that are not the official one (Homebrew,
# /usr/local, MacPorts) build their frameworks with *absolute* paths:
#
#     /opt/homebrew/opt/qtbase/lib/QtGui.framework/Versions/A/QtGui
#
# macdeployqt rewrites the frameworks it copies into the bundle, but it
# does not touch the nested WebEngine helper process, whose own
# dependencies keep pointing at the build machine's Qt installation. A
# bundle built that way only runs where that exact Qt is installed — and
# even on the build machine the helper dies with
#
#     dyld: Library not loaded: @executable_path/../Frameworks/
#           QtWebChannel.framework/Versions/A/QtWebChannel
#
# because @executable_path refers to the *helper's* directory once the
# helper is the process being started, not to the app's.
#
# Two functions here:
#
#   mdreader_external_deps(<bundle> <out_var>)
#       Collects the dependencies of the app and of the WebEngine helper
#       that resolve outside the bundle.
#
#   mdreader_repair_helper(<bundle>)
#       Rewrites the helper's absolute dependencies to @rpath (the helper
#       already has an rpath pointing at Contents/Frameworks) and gives
#       the helper a Frameworks symlink next to its executable, so that
#       @executable_path/../Frameworks resolves to the app's frameworks.

# Collects the dependencies that resolve outside the bundle.
#
# <bundle>   the .app bundle to inspect
# <out_var>  name of the variable (in the caller's scope) that receives
#            the list of offending paths
function(mdreader_external_deps bundle out_var)
    set(_deps "")
    set(_frameworks "${bundle}/Contents/Frameworks")
    set(_helper "${_frameworks}/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess")

    # The app executable plus the WebEngine helper: the two processes that
    # have to find their frameworks inside the bundle.
    file(GLOB _exes "${bundle}/Contents/MacOS/*")
    foreach(_bin ${_exes} "${_helper}")
        if(NOT EXISTS "${_bin}")
            continue()
        endif()
        execute_process(
            COMMAND otool -L "${_bin}"
            OUTPUT_VARIABLE _otool_out
            ERROR_QUIET)
        string(REPLACE "\n" ";" _otool_lines "${_otool_out}")
        foreach(_line ${_otool_lines})
            # Dependency lines carry the version suffix; the first line of
            # otool -L output is the file name itself.
            if(NOT _line MATCHES "compatibility version")
                continue()
            endif()
            string(STRIP "${_line}" _stripped)
            string(REGEX MATCH "^([^ ]+)" _dep_match "${_stripped}")
            if(NOT _dep_match)
                continue()
            endif()
            set(_dep "${CMAKE_MATCH_1}")
            # @rpath / @executable_path / @loader_path stay inside the bundle.
            if(NOT _dep MATCHES "^/")
                continue()
            endif()
            # macOS' own libraries are always available.
            if(_dep MATCHES "^/System/" OR _dep MATCHES "^/usr/lib/")
                continue()
            endif()
            list(APPEND _deps "${_dep}")
        endforeach()
    endforeach()

    if(_deps)
        list(REMOVE_DUPLICATES _deps)
    endif()
    set(${out_var} "${_deps}" PARENT_SCOPE)
endfunction()

# Makes the WebEngine helper resolve its dependencies inside the bundle.
function(mdreader_repair_helper bundle)
    set(_frameworks "${bundle}/Contents/Frameworks")
    set(_helper_app "${_frameworks}/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app")
    set(_helper_exe "${_helper_app}/Contents/MacOS/QtWebEngineProcess")
    if(NOT EXISTS "${_helper_exe}")
        return()
    endif()

    # 1. The helper's own dependencies: /some/where/QtGui.framework/... →
    #    @rpath/QtGui.framework/... The helper already carries an rpath
    #    that points at Contents/Frameworks, so @rpath resolves there.
    execute_process(
        COMMAND otool -L "${_helper_exe}"
        OUTPUT_VARIABLE _otool_out
        ERROR_QUIET)
    string(REPLACE "\n" ";" _otool_lines "${_otool_out}")
    foreach(_line ${_otool_lines})
        if(NOT _line MATCHES "compatibility version")
            continue()
        endif()
        string(STRIP "${_line}" _stripped)
        string(REGEX MATCH "^([^ ]+)" _dep_match "${_stripped}")
        if(NOT _dep_match)
            continue()
        endif()
        set(_dep "${CMAKE_MATCH_1}")
        if(NOT _dep MATCHES "^/")
            continue()
        endif()
        if(_dep MATCHES "^/System/" OR _dep MATCHES "^/usr/lib/")
            continue()
        endif()
        # Keep the part that starts at the framework directory.
        string(REGEX MATCH "([^/]+\\.framework/.*)" _fr_match "${_dep}")
        if(NOT _fr_match)
            continue()
        endif()
        set(_relative "${CMAKE_MATCH_1}")
        string(REGEX MATCH "^([^/]+\\.framework)" _fw_match "${_relative}")
        set(_framework "${CMAKE_MATCH_1}")
        # Only rewrite to a framework that actually shipped in the bundle.
        if(NOT EXISTS "${_frameworks}/${_framework}")
            continue()
        endif()
        execute_process(
            COMMAND install_name_tool -change "${_dep}" "@rpath/${_relative}" "${_helper_exe}"
            RESULT_VARIABLE _rc
            OUTPUT_QUIET
            ERROR_QUIET)
        if(NOT _rc EQUAL 0)
            message(WARNING "MDReader: could not rewrite ${_dep} in the WebEngine helper")
        else()
            message(STATUS "MDReader: rewrote helper dependency ${_dep} → @rpath/${_relative}")
        endif()
    endforeach()

    # 2. The helper's @executable_path/../Frameworks references resolve
    #    against the helper's own directory. A symlink next to the helper's
    #    executable points them at the app's frameworks.
    #
    #    The target has to be *relative*: the bundle is staged in a
    #    temporary directory by CPack and copied to /Applications
    #    afterwards, so an absolute link would keep pointing at the build
    #    tree (and the copy would load two copies of every framework).
    #
    #    Relative to the helper's Contents directory, the app's frameworks
    #    are six levels up:
    #      Contents/Frameworks/QtWebEngineCore.framework/Versions/A/
    #        Helpers/QtWebEngineProcess.app/Contents  ->  Contents/Frameworks
    if(NOT EXISTS "${_helper_app}/Contents/Frameworks")
        file(CREATE_LINK "../../../../../../"
             "${_helper_app}/Contents/Frameworks" SYMBOLIC)
        message(STATUS "MDReader: linked the WebEngine helper's Frameworks directory")
    endif()
endfunction()
