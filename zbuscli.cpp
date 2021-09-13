#include "zbuscli.h"

#include "zbusevent.h"

#include <QDebug>

class ZBusCliPrivate
{
public:
  QList<ZBusEvent> zBusEventList;
};

ZBusCli::ZBusCli(QObject *parent) : QObject(parent)
{
  p = new ZBusCliPrivate();
}

ZBusCli::~ZBusCli()
{
  delete p;
}

void ZBusCli::onConnected() const
{
  qDebug() << "connected to zBus";
}

void ZBusCli::onDisconnected() const
{
  qDebug() << "disonnected from zBus";
}

// TODO: delete
void ZBusCli::onTextMessageReceived(const QString &message)
{
  qDebug() << "received: " << message;
}

void ZBusCli::onZBusEventReceived(const ZBusEvent &event)
{
  p->zBusEventList.append(event);
  qDebug() << "received: " << event.toJson();
}

void ZBusCli::onError(QAbstractSocket::SocketError error) const
{
  qDebug() << "error: " << error;
}
