#ifndef ZBUS_CLIENT_H
#define ZBUS_CLIENT_H

#include <QWebSocket>

class ZBusEvent;
class ZWebSocketPrivate;

class ZWebSocket : public QWebSocket
{
  Q_OBJECT
  Q_DISABLE_COPY(ZWebSocket)

public:
  ZWebSocket(const QString &origin = QString(), QWebSocketProtocol::Version version = QWebSocketProtocol::VersionLatest, QObject *parent = nullptr);
  ~ZWebSocket();

  qint64 sendZBusEvent(const ZBusEvent &event);

signals:
  void processedEventQueue();
  void zBusEventReceived(const ZBusEvent &event);

private slots:
  void processEventQueue();

private:
  ZWebSocketPrivate *p;
};

#endif
