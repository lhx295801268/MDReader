# Generates a macOS .icns from a PNG.
#
# Run as a script:
#   cmake -DSRC_PNG=<in.png> -DOUT_ICNS=<out.icns> -P GenerateIcns.cmake
#
# Both tools used here (sips, iconutil) ship with macOS, so this step is a
# no-op on other platforms — the caller only wires it up under if(APPLE).
#
# The iconset has to carry the exact file names iconutil expects, including
# the @2x variants: iconutil reads the pixel size of each PNG and refuses a
# set whose @2x file is not twice the size of its base file.

if(NOT DEFINED SRC_PNG OR NOT DEFINED OUT_ICNS)
    message(FATAL_ERROR "GenerateIcns.cmake requires -DSRC_PNG= and -DOUT_ICNS=")
endif()
if(NOT EXISTS "${SRC_PNG}")
    message(FATAL_ERROR "GenerateIcns.cmake: source image not found: ${SRC_PNG}")
endif()

find_program(SIPS_EXECUTABLE sips)
find_program(ICONUTIL_EXECUTABLE iconutil)
if(NOT SIPS_EXECUTABLE OR NOT ICONUTIL_EXECUTABLE)
    message(FATAL_ERROR "GenerateIcns.cmake: sips/iconutil not found (macOS only)")
endif()

set(_iconset "${OUT_ICNS}.iconset")
file(REMOVE_RECURSE "${_iconset}")
file(MAKE_DIRECTORY "${_iconset}")

# base size -> @2x size
set(_pairs "16:32" "32:64" "64:128" "128:256" "256:512" "512:1024")
foreach(_pair ${_pairs})
    string(REPLACE ":" ";" _sizes ${_pair})
    list(GET _sizes 0 _base)
    list(GET _sizes 1 _double)
    execute_process(
        COMMAND "${SIPS_EXECUTABLE}" -z ${_base} ${_base} "${SRC_PNG}"
                --out "${_iconset}/icon_${_base}x${_base}.png"
        RESULT_VARIABLE _rc
        OUTPUT_QUIET
        ERROR_QUIET)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "GenerateIcns.cmake: sips failed for ${_base}px")
    endif()
    execute_process(
        COMMAND "${SIPS_EXECUTABLE}" -z ${_double} ${_double} "${SRC_PNG}"
                --out "${_iconset}/icon_${_base}x${_base}@2x.png"
        RESULT_VARIABLE _rc
        OUTPUT_QUIET
        ERROR_QUIET)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "GenerateIcns.cmake: sips failed for ${_double}px")
    endif()
endforeach()

execute_process(
    COMMAND "${ICONUTIL_EXECUTABLE}" -c icns "${_iconset}" -o "${OUT_ICNS}"
    RESULT_VARIABLE _rc
    OUTPUT_QUIET
    ERROR_QUIET)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "GenerateIcns.cmake: iconutil failed")
endif()

file(REMOVE_RECURSE "${_iconset}")
message(STATUS "GenerateIcns.cmake: wrote ${OUT_ICNS}")
