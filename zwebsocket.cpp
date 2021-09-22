#include "zwebsocket.h"

#include "zbusevent.h"

#include <QDebug>
#include <QList>
#include <QQueue>
#include <QString>

class ZWebSocketPrivate {
public:
  QQueue<ZBusEvent> eventQueue;
};

ZWebSocket::ZWebSocket(const QString &origin, QWebSocketProtocol::Version version, QObject *parent)
    : QWebSocket(origin, version, parent)
{
  p = new ZWebSocketPrivate();

  connect(this, &ZWebSocket::connected, this, &ZWebSocket::processEventQueue);
  connect(this, &ZWebSocket::textMessageReceived, this, &ZWebSocket::zBusEventReceived);
}

ZWebSocket::~ZWebSocket()
{
  delete p;
}

void ZWebSocket::processEventQueue()
{
  while (!p->eventQueue.isEmpty())
  {
      sendZBusEvent(p->eventQueue.dequeue());
  }

  emit processedEventQueue();
}

qint64 ZWebSocket::sendZBusEvent(const ZBusEvent &event)
{
  if (isValid())
  {
      return sendTextMessage(event.toJson());
  }
  else
  {
      p->eventQueue.enqueue(event);
      return 0;
  }
}

qint64 ZWebSocket::sendZBusEvents(const QStringList &events)
{
  QList<ZBusEvent> zBusEvents;
  QString event;
  foreach(event, events)
  {
      zBusEvents.append(event);
  }

  return sendZBusEvents(zBusEvents);
}

qint64 ZWebSocket::sendZBusEvents(const QList<ZBusEvent> &events)
{
  qint64 bytesSent = 0;
  ZBusEvent event;
  foreach(event, events)
  {
      bytesSent += sendZBusEvent(event);
  }

  return bytesSent;
}
