# Bundle a self-contained Qt 6.5.3 runtime alongside the mdreader binary
# into the staging area under ${STAGE_DIR}/usr/lib/mdreader/qt6/.
#
# What it copies:
#   - lib/        : every Qt6 + libicu .so that ldd reports mdreader needs
#   - libexec/    : QtWebEngineProcess (loaded by libQt6WebEngineCore.so
#                   when a page is rendered — Ubuntu 22.04 ships the .so
#                   but NOT the helper, which is what crashes the app)
#   - resources/  : icudtl.dat + qtwebengine_*.pak (Chromium data files)
#
# Then it runs patchelf on the staged binary to set RPATH to
# $ORIGIN/../lib/mdreader/qt6/lib so the loader finds the bundled libs
# before the host's broken /usr/lib/x86_64-linux-gnu/libQt6WebEngineCore.
#
# Called from src/CMakeLists.txt at install time when we are about to
# hand the stage to CPack.

# IN_LIST needs CMP0057 NEW on older CMake; install-time scripts don't
# inherit the policy from the outer project tree reliably, so set it
# explicitly.
if(POLICY CMP0057)
    cmake_policy(SET CMP0057 NEW)
endif()
# CMP0011: keep our policy changes scoped so they don't leak into the
# caller (the outer src/cmake_install.cmake warns about this otherwise).
cmake_policy(PUSH)
cmake_policy(SET CMP0011 NEW)

# Caller must set:
#   QT653_SRC_ROOT  — path to the local Qt SDK root (e.g. /home/lhx/Qt/6.5.3/gcc_64)
#   STAGE_DIR       — install prefix root (e.g. /tmp/staging/usr when DESTDIR=/tmp/staging)
#   QT653_BIN_SRC   — path to the BUILD binary (before install stripped its RPATH)
# We ldd the build binary (which still has the Qt SDK in RPATH) to find
# which .so files we must copy, then patchelf the STAGED binary so the
# runtime loader finds them at the new location.
set(QT653_SRC_ROOT  "${QT653_SRC_ROOT}")
set(STAGE_DIR       "${STAGE_DIR}")
set(QT653_BIN_SRC   "${QT653_BIN_SRC}")
# STAGE_DIR points at the install prefix root (e.g. /tmp/staging/usr when
# DESTDIR=/tmp/staging and CMAKE_INSTALL_PREFIX=/usr). The binary was
# installed at STAGE_DIR/bin/mdreader via install(TARGETS ... RUNTIME
# DESTINATION bin), so the path is just STAGE_DIR/bin — not /usr/bin.
set(QT653_BIN       "${STAGE_DIR}/bin/mdreader")
set(QT653_LIB_DST   "${STAGE_DIR}/lib/mdreader/qt6/lib")
set(QT653_LIBEXEC_DST "${STAGE_DIR}/lib/mdreader/qt6/libexec")
set(QT653_RES_DST   "${STAGE_DIR}/share/mdreader/qt6-resources")
# Plugins: Qt looks for platform/input/etc plugins via QT_PLUGIN_PATH or
# relative to the binary at $ORIGIN/../lib/qt6/plugins. Bundling the
# whole plugins/ subtree is small (~10MB) and avoids subtle runtime
# failures when the host doesn't have matching Qt plugins installed.
set(QT653_PLUGIN_DST "${STAGE_DIR}/lib/mdreader/qt6/plugins")

if(NOT QT653_SRC_ROOT OR NOT EXISTS "${QT653_SRC_ROOT}/lib/cmake/Qt6/Qt6Config.cmake")
    message(FATAL_ERROR
        "BundleQt653: QT653_SRC_ROOT (${QT653_SRC_ROOT}) is not a Qt 6.5+ SDK "
        "(expected lib/cmake/Qt6/Qt6Config.cmake). Pass -DQT653_SRC_ROOT=/path/to/Qt/6.5.x/gcc_64")
endif()

if(NOT EXISTS "${QT653_BIN}")
    message(FATAL_ERROR "BundleQt653: expected mdreader binary at ${QT653_BIN}")
endif()
if(NOT EXISTS "${QT653_BIN_SRC}")
    message(FATAL_ERROR "BundleQt653: expected source binary at ${QT653_BIN_SRC} (used for ldd to find SDK libs)")
endif()

find_program(PATCHELF patchelf REQUIRED)

# 1. Walk ldd output to find every .so mdreader pulls in from the Qt SDK.
#    We key on the SDK's lib/ dir, not the literal SONAME, so libicu and
#    any other transitive Qt-only deps get pulled in too.
file(MAKE_DIRECTORY "${QT653_LIB_DST}")
file(MAKE_DIRECTORY "${QT653_LIBEXEC_DST}")
file(MAKE_DIRECTORY "${QT653_RES_DST}")

set(_copied "")
# ldd the BUILD binary (still has Qt SDK in its build-time RPATH) — the
# staged copy has its RPATH stripped by install() so ldd would resolve
# everything to system libs and we'd never find anything to bundle.
execute_process(
    COMMAND ldd "${QT653_BIN_SRC}"
    OUTPUT_VARIABLE _ldd_out
    ERROR_QUIET
)
string(REPLACE "\n" ";" _ldd_lines "${_ldd_out}")
foreach(_line ${_ldd_lines})
    # Lines look like:  libQt6Core.so.6 => /home/lhx/Qt/6.5.3/gcc_64/lib/libQt6Core.so.6 (0x...)
    string(REGEX MATCH "=> ([^ ]+) " _m "${_line}")
    if(NOT _m)
        continue()
    endif()
    set(_resolved "${CMAKE_MATCH_1}")
    string(STRIP "${_resolved}" _resolved)
    if(NOT _resolved)
        continue()
    endif()
    # Skip non-files (vdso, the loader itself) and anything outside our SDK.
    string(REGEX MATCH "^${QT653_SRC_ROOT}/lib/" _is_qt "${_resolved}")
    if(NOT _is_qt)
        continue()
    endif()
    get_filename_component(_name "${_resolved}" NAME)
    if("${_name}" IN_LIST _copied)
        continue()
    endif()
    # Copy the real file (skip the symlink — copy the target with all its
    # versioned siblings so SONAME lookups by unversioned name still hit).
    get_filename_component(_real "${_resolved}" REALPATH)
    file(COPY "${_real}" DESTINATION "${QT653_LIB_DST}")
    # Also copy the versioned family (.so, .so.6, .so.6.5.3) that lives
    # next to it so the dynamic linker can resolve unversioned lookups
    # via the SDK's own SONAME chain.
    get_filename_component(_base "${_real}" NAME_WE)         # libQt6Core
    string(REGEX REPLACE "\\.so.*$" "" _base "${_base}")     # libQt6Core
    file(GLOB _siblings "${QT653_SRC_ROOT}/lib/${_base}.so*")
    foreach(_s ${_siblings})
        get_filename_component(_sn "${_s}" NAME)
        if(NOT "${_sn}" IN_LIST _copied)
            file(COPY "${_s}" DESTINATION "${QT653_LIB_DST}")
            list(APPEND _copied "${_sn}")
        endif()
    endforeach()
    list(APPEND _copied "${_name}")
endforeach()
list(LENGTH _copied _n_so)
message(STATUS "BundleQt653: bundled ${_n_so} Qt .so files into ${QT653_LIB_DST}")

# 2. QtWebEngineProcess — the helper binary libQt6WebEngineCore.so spawns
#    to render pages. Its RPATH is $ORIGIN/../lib which resolves to
#    ${QT653_LIBEXEC_DST}/../lib == ${QT653_LIB_DST} once installed.
if(EXISTS "${QT653_SRC_ROOT}/libexec/QtWebEngineProcess")
    file(COPY "${QT653_SRC_ROOT}/libexec/QtWebEngineProcess"
         DESTINATION "${QT653_LIBEXEC_DST}")
    execute_process(COMMAND chmod +x "${QT653_LIBEXEC_DST}/QtWebEngineProcess")
    message(STATUS "BundleQt653: copied QtWebEngineProcess")
else()
    message(WARNING "BundleQt653: QtWebEngineProcess not found in SDK — WebEngine will crash")
endif()

# 3. Resources: .pak files + icudtl.dat. QtWebEngineProcess looks for
#    these at $ORIGIN/../resources by default (relative to libexec). We
#    lay them out in /usr/share/mdreader/qt6-resources and patch the
#    helper's RPATH-style resource path below.
if(EXISTS "${QT653_SRC_ROOT}/resources")
    file(GLOB _paks "${QT653_SRC_ROOT}/resources/*")
    foreach(_p ${_paks})
        get_filename_component(_pn "${_p}" NAME)
        file(COPY "${_p}" DESTINATION "${QT653_RES_DST}")
    endforeach()
    list(LENGTH _paks _n_res)
    message(STATUS "BundleQt653: copied ${_n_res} resource files to ${QT653_RES_DST}")
endif()

# 3b. Locale .pak files (qtwebengine_locales/<lang>.pak). Without these
#     Chromium logs "could not find en-US.pak" and falls back to bogus
#     locale data, which can crash the renderer. Bundle the whole
#     qtwebengine_locales/ dir next to the resources.
if(EXISTS "${QT653_SRC_ROOT}/translations/qtwebengine_locales")
    file(COPY "${QT653_SRC_ROOT}/translations/qtwebengine_locales"
         DESTINATION "${QT653_RES_DST}")
    message(STATUS "BundleQt653: copied qtwebengine_locales/ to ${QT653_RES_DST}")
endif()

# 4. Patch the mdreader binary so the loader looks at our bundled libs
#    first. RPATH=$ORIGIN/../lib/mdreader/qt6/lib means: at runtime the
#    binary is /usr/bin/mdreader and looks at
#    /usr/lib/mdreader/qt6/lib/libQt6*.so — which overrides the system
#    /usr/lib/x86_64-linux-gnu/libQt6WebEngineCore.so.6 (the broken one).
set(_new_rpath "$ORIGIN/../lib/mdreader/qt6/lib")
execute_process(
    COMMAND ${PATCHELF} --set-rpath "${_new_rpath}" "${QT653_BIN}"
    RESULT_VARIABLE _patch_result
)
if(NOT _patch_result EQUAL 0)
    message(FATAL_ERROR "BundleQt653: patchelf failed (${_patch_result})")
endif()
message(STATUS "BundleQt653: RPATH set to ${_new_rpath} on ${QT653_BIN}")

# 5. Tell QtWebEngineProcess where to find resources. It normally uses
#    QTWEBENGINEPROCESS_PATH-style env vars; we set them at app launch
#    time. No need to patch the helper itself.

# 6. Plugins: bundle the entire plugins/ subtree so xcb / wayland /
#    offscreen platforms and their integrations are available without
#    depending on whatever the host distro ships.
if(EXISTS "${QT653_SRC_ROOT}/plugins")
    file(COPY "${QT653_SRC_ROOT}/plugins" DESTINATION "${STAGE_DIR}/lib/mdreader/qt6")
    message(STATUS "BundleQt653: copied plugins/ to ${QT653_PLUGIN_DST}")
endif()

# 7. Second ldd pass on every bundled plugin .so. Step 1 only walked ldd
#    of the mdreader binary, so transitive Qt deps that are NEEDED only
#    by a plugin (e.g. libQt6XcbQpa.so.6 is needed by libqxcb.so, but
#    not by mdreader itself) were missed. Without this pass the loader
#    would resolve libQt6XcbQpa.so.6 against the host distro's
#    /usr/lib/x86_64-linux-gnu/libQt6XcbQpa.so.6 (Ubuntu 22.04 ships
#    Qt 6.2.4 — ABI mismatch makes QXcbIntegration fail with the
#    misleading "xcb-cursor0 is needed" error).
#
#    Important: ldd must be run against the SDK plugin (NOT the staged
#    one). The staged plugin has its own RPATH ($ORIGIN/../../lib →
#    stage's bundled lib/), so its ldd resolves transitive deps to
#    themselves and we never see the SDK paths we need to copy from.
#    The SDK plugin, by contrast, has RPATH → SDK lib/, so its ldd
#    output shows the SDK paths we need to detect.
#    CMake 3.22's file(GLOB) doesn't support **, so enumerate by walking
#    the directory tree ourselves.
file(GLOB _sdk_plugin_sos "${QT653_SRC_ROOT}/plugins/*.so")
file(GLOB _sdk_plugin_subdirs LIST_DIRECTORIES true "${QT653_SRC_ROOT}/plugins/*")
foreach(_d ${_sdk_plugin_subdirs})
    if(IS_DIRECTORY "${_d}")
        file(GLOB _s "${_d}/*.so")
        list(APPEND _sdk_plugin_sos ${_s})
    endif()
endforeach()
list(LENGTH _sdk_plugin_sos _n_plugins)
message(STATUS "BundleQt653: pass 2 scanning ${_n_plugins} SDK plugin .so files")
set(_copied2 "")
foreach(_p ${_sdk_plugin_sos})
    execute_process(
        COMMAND ldd "${_p}"
        OUTPUT_VARIABLE _ldd2
        ERROR_QUIET
    )
    string(REPLACE "\n" ";" _lines2 "${_ldd2}")
    foreach(_l ${_lines2})
        string(REGEX MATCH "=> ([^ ]+) " _m2 "${_l}")
        if(NOT _m2)
            continue()
        endif()
        set(_r2 "${CMAKE_MATCH_1}")
        string(STRIP "${_r2}" _r2)
        if(NOT _r2)
            continue()
        endif()
        # Plugin .so files live under SDK plugins/<x>/, so ldd prints
        # relative RPATH paths like ".../plugins/platforms/../../lib/...".
        # Normalize to an absolute path before checking it's under SDK lib/.
        get_filename_component(_r2_abs "${_r2}" ABSOLUTE)
        string(REGEX MATCH "^${QT653_SRC_ROOT}/lib/" _is_qt2 "${_r2_abs}")
        if(NOT _is_qt2)
            continue()
        endif()
        # Use the realpath (resolve the ../../lib → SDK/lib) for the
        # file copy source, so we don't rely on file(COPY) following
        # symlinks correctly in every case.
        get_filename_component(_r2_real "${_r2_abs}" REALPATH)
        get_filename_component(_n2 "${_r2_real}" NAME)
        if("${_n2}" IN_LIST _copied2)
            continue()
        endif()
        file(COPY "${_r2_real}" DESTINATION "${QT653_LIB_DST}")
        # Copy the unversioned + versioned siblings so SONAME lookups
        # against an unversioned name (libfoo.so) resolve here too.
        get_filename_component(_b2 "${_r2_real}" NAME_WE)
        string(REGEX REPLACE "\\.so.*$" "" _b2 "${_b2}")
        file(GLOB _s2 "${QT653_SRC_ROOT}/lib/${_b2}.so*")
        foreach(_s ${_s2})
            get_filename_component(_sn "${_s}" NAME)
            if(NOT "${_sn}" IN_LIST _copied2)
                file(COPY "${_s}" DESTINATION "${QT653_LIB_DST}")
                list(APPEND _copied2 "${_sn}")
            endif()
        endforeach()
        list(APPEND _copied2 "${_n2}")
    endforeach()
endforeach()
list(LENGTH _copied2 _n_so2)
if(_n_so2 GREATER 0)
    message(STATUS "BundleQt653: pass 2 bundled ${_n_so2} additional Qt .so from plugin deps")
endif()

cmake_policy(POP)