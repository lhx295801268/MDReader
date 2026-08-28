#!/bin/sh
# Launcher wrapper for MDReader.
#
# The binary at /usr/bin/mdreader is linked against Qt 6.5.3 libraries
# that we bundle under /usr/lib/mdreader/qt6/. The runtime needs to know
# where the bundled QtWebEngine helper and its data files live, otherwise
# QtWebEngineProcess fails to initialize and the renderer process aborts.
# Setting these env vars from the .desktop launcher avoids requiring the
# user to export them by hand.
#
# The binary itself resolves Qt libs via its RPATH ($ORIGIN/../lib/...),
# so LD_LIBRARY_PATH isn't needed — only the WebEngine-specific paths.

export QTWEBENGINEPROCESS_PATH=/usr/lib/mdreader/qt6/libexec/QtWebEngineProcess
export QTWEBENGINE_RESOURCES_PATH=/usr/share/mdreader/qt6-resources
# Locales live next to resources in our bundle (qtwebengine_locales/).
# Qt searches <RESOURCES_PATH>/qtwebengine_locales first, but only when
# QTWEBENGINE_LOCALES_PATH is unset and the bundle layout matches. We
# point it explicitly so it always finds en-US.pak.
export QTWEBENGINE_LOCALES_PATH=/usr/share/mdreader/qt6-resources/qtwebengine_locales

exec /usr/bin/mdreader "$@"