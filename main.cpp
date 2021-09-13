// TODO: add unit tests, add test and explanation for setting origin
#include "zbuscli.h"
#include "zwebsocket.h"

#include <signal.h>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
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

  ZBusCli zBusCli;

  ZWebSocket zBusClient("http://localhost"); //zBus tries to ensure that clients are local

  QObject::connect(&zBusClient, &ZWebSocket::connected,
                   &zBusCli, &ZBusCli::onConnected);
  QObject::connect(&zBusClient, &ZWebSocket::disconnected,
                   &zBusCli, &ZBusCli::onDisconnected);
  QObject::connect(&zBusClient, &ZWebSocket::textMessageReceived,
                   &zBusCli, &ZBusCli::onTextMessageReceived);
  QObject::connect(&zBusClient, SIGNAL(error(QAbstractSocket::SocketError)),
                   &zBusCli, SLOT(onError(QAbstractSocket::SocketError)));

  zBusClient.open(QUrl("ws://192.168.0.157:8180"));

  signal(SIGINT, handleSignal);
  signal(SIGTERM, handleSignal);

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
