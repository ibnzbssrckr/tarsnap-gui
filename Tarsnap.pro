#-------------------------------------------------
#
# Project created by QtCreator 2014-12-10T01:25:38
#
#-------------------------------------------------

QT       += core gui network sql
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++11

TEMPLATE = app
TARGET = tarsnap-gui

VERSION = 0.6
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
#QMAKE_TARGET_COMPANY = Tarsnap Backup Inc.
#QMAKE_TARGET_PRODUCT = Tarsnap
#QMAKE_TARGET_DESCRIPTION = GUI frontend for Tarsnap
#QMAKE_TARGET_COPYRIGHT = copyright Tarsnap Backup Inc.

SOURCES += \
    src/coreapplication.cpp \
    src/main.cpp \
    src/utils.cpp \
    src/widgets/archivelistitem.cpp \
    src/widgets/backuplistitem.cpp \
    src/widgets/backuplistwidget.cpp \
    src/widgets/archivelistwidget.cpp \
    src/widgets/filepicker.cpp \
    src/widgets/filepickerdialog.cpp \
    src/widgets/joblistitem.cpp \
    src/widgets/joblistwidget.cpp \
    src/widgets/jobwidget.cpp \
    src/widgets/mainwindow.cpp \
    src/widgets/restoredialog.cpp \
    src/widgets/setupdialog.cpp \
    src/widgets/textlabel.cpp \
    src/widgets/busywidget.cpp \
    src/customfilesystemmodel.cpp \
    src/taskmanager.cpp \
    src/tarsnapclient.cpp \
    src/persistentmodel/persistentstore.cpp \
    src/persistentmodel/persistentobject.cpp \
    src/persistentmodel/archive.cpp \
    src/persistentmodel/job.cpp \
    src/backuptask.cpp

HEADERS  += \
    src/coreapplication.h \
    src/debug.h \
    src/utils.h \
    src/widgets/archivelistitem.h \
    src/widgets/backuplistitem.h \
    src/widgets/backuplistwidget.h \
    src/widgets/archivelistwidget.h \
    src/widgets/filepicker.h \
    src/widgets/filepickerdialog.h \
    src/widgets/joblistitem.h \
    src/widgets/joblistwidget.h \
    src/widgets/jobwidget.h \
    src/widgets/mainwindow.h \
    src/widgets/restoredialog.h \
    src/widgets/setupdialog.h \
    src/widgets/textlabel.h \
    src/widgets/busywidget.h \
    src/customfilesystemmodel.h \
    src/taskmanager.h \
    src/tarsnapclient.h \
    src/persistentmodel/persistentstore.h \
    src/persistentmodel/persistentobject.h \
    src/persistentmodel/archive.h \
    src/persistentmodel/job.h \
    src/backuptask.h

INCLUDEPATH += src/widgets/ \
            += src/

FORMS    += \
    forms/backupitemwidget.ui \
    forms/setupdialog.ui \
    forms/mainwindow.ui \
    forms/archiveitemwidget.ui \
    forms/restoredialog.ui \
    forms/filepicker.ui \
    forms/filepickerdialog.ui \
    forms/jobitemwidget.ui \
    forms/jobwidget.ui \
    forms/aboutwidget.ui

RESOURCES += resources/resources.qrc

DISTFILES += \
    COPYING \
    README \
    INSTALL \
    CHANGELOG

osx {
    OBJECTIVE_SOURCES += src/platform_osx.mm
    LIBS += -framework Foundation
    ICON = resources/icons/tarsnap.icns
    TARGET = Tarsnap
}
