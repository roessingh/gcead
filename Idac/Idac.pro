# -------------------------------------------------
# Project created by QtCreator 2009-04-17T11:33:42
# -------------------------------------------------
QT -= gui
CONFIG += warn_on staticlib create_prl debug_and_release
TEMPLATE = lib
INCLUDEPATH += . .. ../Core ../IdacDriver
HEADERS += \
    IdacControlThread.h \
    IdacFactory.h \
    IdacProxy.h \
    IdacDriverManager.h 
SOURCES += \
    IdacDriverManager.cpp \
    IdacControlThread.cpp \
    IdacProxy.cpp \
    IdacFactory.cpp
win32:SOURCES += IdacDriverManager_libusb0.cpp
unix:SOURCES += IdacDriverManager_libusbx.cpp

win32:DEFINES += WIN32 LIBUSB0
unix:DEFINES += LIBUSBX

win32:INCLUDEPATH += ../extern/win32
unix:INCLUDEPATH += ../extern/libusb/include

#CONFIG(debug, debug|release) {
#    DESTDIR = ../debug
#} else {
#    DESTDIR = ../release
#}
