QT += widgets charts
QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    accuracy/accuracydatasaver.cpp \
    accuracy/accuracystate.cpp \
    accuracy/accuracyvisualizer.cpp \
    accuracy/accuracywindow.cpp \
    accuracy/accuracycalculator.cpp \
    appstate.cpp \
    autoconfigdialog.cpp \
    automeasurement.cpp \
    averagefilter.cpp \
    calculatemesurement.cpp \
    databuffer.cpp \
    datameasurement.cpp \
    datavisualizer.cpp \
    expectationfilter.cpp \
    exportmanager.cpp \
    filemanager.cpp \
    filter.cpp \
    main.cpp \
    mainwindow.cpp \
    nonefilter.cpp \
    pyproc.cpp \
    settingsmanager.cpp \
    stepconfigdialog.cpp

HEADERS += \
    accuracy/accuracydatasaver.h \
    accuracy/accuracystate.h \
    accuracy/accuracyvisualizer.h \
    accuracy/accuracywindow.h \
    accuracy/accuracycalculator.h \
    appstate.h \
    autoconfigdialog.h \
    automeasurement.h \
    averagefilter.h \
    calculatemesurement.h \
    databuffer.h \
    datameasurement.h \
    datavisualizer.h \
    expectationfilter.h \
    exportmanager.h \
    filemanager.h \
    filter.h \
    mainwindow.h \
    nonefilter.h \
    pyproc.h \
    settingsmanager.h \
    stepconfigdialog.h \
    typemeasurement.h

FORMS += \
    accuracy/accuracywindow.ui \
    autoconfigdialog.ui \
    mainwindow.ui \
    stepconfigdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    .gitattributes \
    .gitignore
