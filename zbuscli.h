#ifndef ZBUS_CLI_H
#define ZBUS_CLI_H

#include <QAbstractSocket>
#include <QObject>

class ZBusEvent;
class ZBusCliPrivate;

class ZBusCli : public QObject
{
  Q_OBJECT
  Q_DISABLE_COPY(ZBusCli)

public:
  ZBusCli(QObject *parent = nullptr);
  ~ZBusCli();

  void exec(const QUrl &zBusUrl);

signals:
  void eventSubmitted(const QString &event, const QString &data);

private slots:
  void onDisconnected();
  qint64 onEventSubmitted(const QString &event, const QString &data);
  void onZBusEventReceived(const ZBusEvent &event);

private:
  void ncurses();

  ZBusCliPrivate *p;
};

#endif
