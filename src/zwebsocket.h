#ifndef ZBUS_CLIENT_H
#define ZBUS_CLIENT_H

#include <QWebSocket>

class ZBusEvent;
class ZWebSocketPrivate;

/* A QWebSocket configured for connecting to, and communicating with, the zBus server.
 */
class ZWebSocket : public QWebSocket
{
  Q_OBJECT
  Q_DISABLE_COPY(ZWebSocket)

public:
  /* zBus checks that incoming requests have an origin header that contains "http://localhost", so
   * ZWebSocket is set to send requests with an origin header that contains "http://localhost", by
   * default
   */
  ZWebSocket(const QString &origin = "http://localhost", QWebSocketProtocol::Version version = QWebSocketProtocol::VersionLatest, QObject *parent = nullptr);
  ~ZWebSocket();

  qint64 sendZBusEvent(const ZBusEvent &event);
  qint64 sendZBusEvents(const QStringList &events);
  qint64 sendZBusEvents(const QList<ZBusEvent> &events);

signals:
  void processedEventQueue();
  void zBusEventReceived(const ZBusEvent &event);

private slots:
  void processEventQueue();

private:
  ZWebSocketPrivate *p;
};

#endif
