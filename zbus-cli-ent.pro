TEMPLATE = app
CONFIG += warn_all console
QT += core websockets
QT -= gui

TARGET = zbus-cli-ent.x

HEADERS += zbuscli.h
HEADERS += zbusevent.h
HEADERS += zwebsocket.h

SOURCES += main.cpp
SOURCES += zbuscli.cpp
SOURCES += zbusevent.cpp
SOURCES += zwebsocket.cpp

target.path = .
INSTALLS += target
