# 安装 .desktop / icon / mime / launcher wrapper
install(FILES ${CMAKE_SOURCE_DIR}/packaging/mdreader.desktop
        DESTINATION share/applications)
install(FILES ${CMAKE_SOURCE_DIR}/packaging/mdreader.png
        DESTINATION share/icons/hicolor/128x128/apps)
install(FILES ${CMAKE_SOURCE_DIR}/packaging/mdreader-mime.xml
        DESTINATION share/mime/packages)
# Wrapper script: sets QTWEBENGINE_* env vars so the bundled
# QtWebEngineProcess and its .pak resources are found. The .desktop
# Exec= line points at this wrapper instead of /usr/bin/mdreader
# directly so GUI launches from the application menu work without
# requiring the user to export env vars themselves.
install(PROGRAMS ${CMAKE_SOURCE_DIR}/packaging/mdreader.sh
        DESTINATION bin
        RENAME mdreader.sh)

# postinst 触发更新
set(_postinst "${CMAKE_BINARY_DIR}/postinst")
file(WRITE ${_postinst}
     "#!/bin/sh\n"
     "if [ \"$1\" = \"configure\" ]; then\n"
     "  update-mime-database /usr/share/mime || true\n"
     "  update-desktop-database /usr/share/applications || true\n"
     "fi\n"
     "exit 0\n")
install(PROGRAMS ${_postinst} DESTINATION /var/lib/dpkg/info
        RENAME postinst)