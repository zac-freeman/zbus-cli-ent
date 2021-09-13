#include "zwebsocket.h"

#include "zbusevent.h"

#include <QList>
#include <QString>

class ZWebSocketPrivate {};

ZWebSocket::ZWebSocket(const QString &origin, QWebSocketProtocol::Version version, QObject *parent)
    : QWebSocket(origin, version, parent)
{
  p = new ZWebSocketPrivate();
}

ZWebSocket::~ZWebSocket()
{
  delete p;
}

qint64 ZWebSocket::sendZBusEvent(const ZBusEvent &event)
{
  return sendTextMessage(event.toJson());
}
