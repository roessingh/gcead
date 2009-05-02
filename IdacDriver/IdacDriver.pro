# -------------------------------------------------
# Project created by QtCreator 2009-04-17T11:33:42
# -------------------------------------------------
QT -= gui
TEMPLATE = lib
CONFIG += warn_on \
    staticlib \
    create_prl \
    debug_and_release
INCLUDEPATH += . \
    .. \
    ../Core
HEADERS += IdacDriver.h \
    IdacEnums.h \
    IdacChannelSettings.h \
    IdacSettings.h \
    Sample.h \
    Sleeper.h \
    IdacDriverUsb.h \
    IdacDriverSamplingThread.h
SOURCES += IdacDriver.cpp \
    IdacDriverUsb.cpp
win32:INCLUDEPATH += ../extern/win32
unix:!macx:HEADERS += libusb_augment.h
unix:!macx:SOURCES += libusb_augment.c
CONFIG(debug, debug|release):DESTDIR = ../debug
else:DESTDIR = ../release
