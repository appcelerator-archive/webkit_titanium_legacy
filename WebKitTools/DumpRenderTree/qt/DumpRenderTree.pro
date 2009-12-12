TARGET = DumpRenderTree
CONFIG  -= app_bundle
CONFIG += uitools

mac:!static:contains(QT_CONFIG, qt_framework):!CONFIG(webkit_no_framework) {
    CONFIG -= debug
    CONFIG += release
}

BASEDIR = $$PWD/../

include(../../../WebKit.pri)
INCLUDEPATH += /usr/include/freetype2
INCLUDEPATH += ../../..
INCLUDEPATH += ../../../JavaScriptCore
INCLUDEPATH += ../../../JavaScriptCore/ForwardingHeaders
INCLUDEPATH += $$BASEDIR
DESTDIR = ../../../bin

CONFIG += link_pkgconfig
PKGCONFIG += fontconfig

QT = core gui network
macx: QT += xml

HEADERS = $$BASEDIR/WorkQueue.h \
    WorkQueueItem.h \
    DumpRenderTree.h \
    EventSenderQt.h \
    TextInputControllerQt.h \
    LayoutTestControllerQt.h \
    jsobjects.h \
    testplugin.h
SOURCES = $$BASEDIR/WorkQueue.cpp \
    DumpRenderTree.cpp \
    EventSenderQt.cpp \
    TextInputControllerQt.cpp \
    WorkQueueItemQt.cpp \
    LayoutTestControllerQt.cpp \
    main.cpp \
    jsobjects.cpp \
    testplugin.cpp

unix:!mac {
    QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
}

DEFINES+=USE_SYSTEM_MALLOC
