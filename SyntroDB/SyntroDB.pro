# This file is part of Syntro
#
# Copyright (c) 2013 Pansenti, LLC. All rights reserved.
#

TEMPLATE = app
TARGET = SyntroDB

win32* {
    DESTDIR = Release
}
else {
    DESTDIR = Output
}

QT += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += release

# No debug in release builds
unix:QMAKE_CXXFLAGS_RELEASE -= -g

unix {
	macx {
		target.path = /usr/local/bin
	}
	else {
		target.path = /usr/bin
	}

	INSTALLS += target

        INCLUDEPATH += ../SyntroLib \
                       ../SyntroGUI \
                       ../SyntroControlLib
 
        QMAKE_LIBDIR += ../SyntroLib \
                        ../SyntroGUI \
                        ../SyntroControlLib

        LIBS += -lSyntroLib -lSyntroGUI -lSyntroControlLib
}

DEFINES += QT_NETWORK_LIB

INCLUDEPATH += GeneratedFiles \
	../Common \
	.

win32-g++:LIBS += -L"$(SYNTRODIR)/bin"

win32-msvc*:LIBS += -L"$(SYNTRODIR)/lib"

win32 {
        DEFINES += _CRT_SECURE_NO_WARNINGS
        INCLUDEPATH += $(SYNTRODIR)/include
        LIBS += -lSyntroLib -lSyntroGUI -lSyntroControlLib
}

DEPENDPATH +=

MOC_DIR += GeneratedFiles/release

OBJECTS_DIR += release

UI_DIR += GeneratedFiles

RCC_DIR += GeneratedFiles

include(SyntroDB.pri)
