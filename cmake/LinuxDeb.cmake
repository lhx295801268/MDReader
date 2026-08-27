# 安装 .desktop / icon / mime
install(FILES ${CMAKE_SOURCE_DIR}/packaging/mdreader.desktop
        DESTINATION share/applications)
install(FILES ${CMAKE_SOURCE_DIR}/packaging/mdreader.png
        DESTINATION share/icons/hicolor/128x128/apps)
install(FILES ${CMAKE_SOURCE_DIR}/packaging/mdreader-mime.xml
        DESTINATION share/mime/packages)

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