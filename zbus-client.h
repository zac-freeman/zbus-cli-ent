#ifndef ZBUS_CLIENT_H
#define ZBUS_CLIENT_H

#include <QObject>

struct ZBusEvent;
class ZBusClientPrivate;

class ZBusClient : public QObject
{
  Q_OBJECT

public:
  explicit ZBusClient(const QString& server, QObject *parent = nullptr);
  ~ZBusClient();

  bool send(const QString &event);
  bool send(const ZBusEvent &event);

private slots:
  void onConnected();
  void onDisconnected();
  void onTextMessageReceived(const QString &event);

private:
  ZBusClientPrivate *p;
};

#endif
