QT += core widgets network

CONFIG += c++11
RC_FILE = ExeIcon.rc
TARGET = ShinPlayer
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    qt_port/src/main.cpp \
    qt_port/src/PlayerDialog.cpp \
    qt_port/src/MediaPlayerWrapper.cpp \
    qt_port/src/watermarkdialog.cpp

HEADERS += \
    qt_port/src/PlayerDialog.h \
    qt_port/src/MediaPlayerWrapper.h \
    qt_port/src/watermarkdialog.h

FORMS += \
    qt_port/forms/PlayerDialog.ui

# PlayM4 SDK configuration
# DEFINES += _FOR_HIKPLAYM4_DLL_  # Commented out - using standard PlayM4 functions

# Include paths
INCLUDEPATH += $$PWD/SDK
DEPENDPATH += $$PWD/SDK

# Library paths and libraries
win32 {

        # 64-bit build
        LIBS += -L$$PWD/SDK -lShinPlayCtrl
        
        # Copy required DLLs to output directory
        CONFIG(debug, debug|release) {
            DESTDIR = $$PWD/debug
        } else {
            DESTDIR = $$PWD/release
        }
}


RESOURCES += \
    resource.qrc
