QT += core gui widgets network
CONFIG += c++17

TARGET = ELSKY_test_server
TEMPLATE = app

SOURCES += \
    main.cpp \
    server_window.cpp \
    wake_on_lan.cpp \
    ../common/protocol.cpp

HEADERS += \
    server_window.h \
    wake_on_lan.h \
    ../common/protocol.h

INCLUDEPATH += ../common

# 自动部署：编译完成后自动运行 windeployqt，将所需DLL复制到exe所在目录
win32 {
    DEPLOY_COMMAND = windeployqt
    CONFIG(debug, debug|release) {
        DEPLOY_DIR = $$OUT_PWD/debug
    } else {
        DEPLOY_DIR = $$OUT_PWD/release
    }
    DEPLOY_TARGET = $$shell_path($${DEPLOY_DIR}/$${TARGET}.exe)
    QMAKE_POST_LINK += $$quote($$DEPLOY_COMMAND --no-translations --no-opengl-sw $$DEPLOY_TARGET)
}
