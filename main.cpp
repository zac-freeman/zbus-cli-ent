// TODO: add unit tests, add test and explanation for setting origin
#include "zbuscli.h"
#include "zbusevent.h"
#include "zwebsocket.h"

#include <signal.h>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QObject>

void handleSignal(int signum);

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("zbus-cli-ent");
  QCoreApplication::setApplicationVersion("0.1");
  QLoggingCategory::defaultCategory()->setEnabled(QtDebugMsg, true);

  QCommandLineParser parser;
  parser.setApplicationDescription("zbus client controlled with CLI");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOption({{"s", "send"},
                    QCoreApplication::translate("main", "send json-formatted zBus <event>"),
                    QCoreApplication::translate("main", "event")});

  parser.process(app);

  ZWebSocket zBusClient("http://localhost"); //zBus tries to ensure that clients are local

  signal(SIGINT, handleSignal);
  signal(SIGTERM, handleSignal);

  // TODO: dont parse commas as separator
  // TODO: send multiple events
  // TODO: interactive mode w/ event sending
  if (parser.isSet("send"))
  {
      QObject::connect(&zBusClient, &ZWebSocket::processedEventQueue,
                       &app, &QCoreApplication::quit);
      zBusClient.open(QUrl("ws://192.168.0.157:8180"));
      zBusClient.sendZBusEvent(ZBusEvent(parser.value("send")));
      return app.exec();
  }

  ZBusCli zBusCli;

  QObject::connect(&zBusClient, &ZWebSocket::connected,
                   &zBusCli, &ZBusCli::onConnected);
  QObject::connect(&zBusClient, &ZWebSocket::disconnected,
                   &zBusCli, &ZBusCli::onDisconnected);
  QObject::connect(&zBusClient, &ZWebSocket::zBusEventReceived,
                   &zBusCli, &ZBusCli::onZBusEventReceived);
  QObject::connect(&zBusClient, SIGNAL(error(QAbstractSocket::SocketError)),
                   &zBusCli, SLOT(onError(QAbstractSocket::SocketError)));

  zBusClient.open(QUrl("ws://192.168.0.157:8180"));

  return app.exec();
}

// TODO: proper signal handling
void handleSignal(int signum)
{
  if (signum == SIGINT || signum == SIGTERM)
  {
    exit(0);
  }
}
