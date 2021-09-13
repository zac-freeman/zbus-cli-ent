#ifndef ZBUS_CLI_H
#define ZBUS_CLI_H

#include <QObject>
#include <QAbstractSocket>

class ZBusEvent;
class ZBusCliPrivate;

class ZBusCli : public QObject
{
  Q_OBJECT
  Q_DISABLE_COPY(ZBusCli)

public:
  ZBusCli(QObject *parent = nullptr);
  ~ZBusCli();

public slots:
  void onConnected() const;
  void onDisconnected() const;
  void onZBusEventReceived(const ZBusEvent &event);
  void onError(QAbstractSocket::SocketError error) const;

private:
  ZBusCliPrivate *p;
};

#endif
