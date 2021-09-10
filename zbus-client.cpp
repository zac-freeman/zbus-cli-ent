#include "zbus-client.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>
#include <QWebSocket>

struct ZBusEvent
{
  QString sender;
  QString event;
  QJsonObject data;

  QString toString()
  {
    return QString();
  }
  
  QString toJson()
  {
    return QString();
  }
};

class ZBusClientPrivate
{
public:
  ZBusClientPrivate(const QString &serverUrl)
  {
    this->serverUrl = serverUrl;
    server.open(serverUrl);
  }

  QList<ZBusEvent> eventList;
  QWebSocket server;
  QString serverUrl;
};

ZBusClient::ZBusClient(const QString &serverUrl, QObject *parent) : QObject(parent)
{
  p = new ZBusClientPrivate(serverUrl);

  connect(&p->server, &QWebSocket::connected, this, &ZBusClient::onConnected);
  connect(&p->server, &QWebSocket::disconnected, this, &ZBusClient::onDisconnected);
}

ZBusClient::~ZBusClient()
{
  delete p;
}

void ZBusClient::onConnected()
{
  qDebug() << "connected!";
  connect(&p->server, &QWebSocket::textMessageReceived, this, &ZBusClient::onTextMessageReceived);
}

void ZBusClient::onDisconnected()
{
  qDebug() << "disconnected!";
}

void ZBusClient::onTextMessageReceived(const QString &event)
{
  qDebug() << "received: " << event;
}

bool ZBusClient::send(const QString &event)
{
  return false;
}

bool ZBusClient::send(const ZBusEvent &event)
{
  return false;
}
