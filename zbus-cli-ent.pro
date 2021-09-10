TEMPLATE = app
CONFIG += warn_all
QT += core websockets
QT -= gui

TARGET = zbus-cli-ent.x

HEADERS += zbus-client.h

SOURCES += main.cpp
SOURCES += zbus-client.cpp

target.path = .
INSTALLS += target
