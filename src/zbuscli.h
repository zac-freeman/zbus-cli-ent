#ifndef ZBUS_CLI_H
#define ZBUS_CLI_H

#include <QObject>

class ZBusEvent;
class ZBusCliPrivate;

/* Bridge between the ZWebSocket sending and receiving events, and the ncurses event loop displaying
 * the events and accepting input from the user.
 */
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
  void startEventLoop();

  ZBusCliPrivate *p;
};

#endif
