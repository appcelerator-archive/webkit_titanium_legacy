TARGET = DumpRenderTree
CONFIG  -= app_bundle

include(../../../WebKit.pri)
INCLUDEPATH += /usr/include/freetype2
INCLUDEPATH += ../../../JavaScriptCore
DESTDIR = ../../../bin

CONFIG += link_pkgconfig
PKGCONFIG += fontconfig

QT = core gui
macx: QT += xml network

HEADERS = WorkQueue.h WorkQueueItem.h DumpRenderTree.h jsobjects.h testplugin.h
SOURCES = WorkQueue.cpp DumpRenderTree.cpp main.cpp jsobjects.cpp testplugin.cpp

unix:!mac {
    QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
}

lessThan(QT_MINOR_VERSION, 4) {
    DEFINES += QT_BEGIN_NAMESPACE="" QT_END_NAMESPACE=""
}
