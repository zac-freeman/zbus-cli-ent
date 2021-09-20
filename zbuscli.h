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

  void exec();
  void run();
  qint64 send(const QString &event, const QString &data);

signals:
  void enterPressed(const QString &event, const QString &data);

private slots:
  void onZBusEventReceived(const ZBusEvent &event);

private:
  ZBusCliPrivate *p;
};

#endif
