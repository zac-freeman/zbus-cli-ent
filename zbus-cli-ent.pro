TEMPLATE = app
CONFIG += warn_all console
QT += core websockets
QT -= gui
LIBS += -lform -lncurses

TARGET = zbus-cli-ent.x

HEADERS += src/zbuscli.h
HEADERS += src/zbusevent.h
HEADERS += src/zwebsocket.h

SOURCES += src/main.cpp
SOURCES += src/zbuscli.cpp
SOURCES += src/zbusevent.cpp
SOURCES += src/zwebsocket.cpp

target.path = .
INSTALLS += target
