// TODO: add unit tests, add test and explanation for setting origin
// TODO: add README
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
  // TODO: interactive mode flag

  parser.process(app);

  signal(SIGINT, handleSignal);
  signal(SIGTERM, handleSignal);

  // TODO: dont parse commas as separator
  // TODO: send multiple events
  // TODO: interactive mode w/ event sending
  // TODO: retry logic
  if (parser.isSet("send"))
  {
      ZWebSocket zBusClient("http://localhost"); //zBus tries to ensure that clients are local
      QObject::connect(&zBusClient, &ZWebSocket::processedEventQueue,
                       &app, &QCoreApplication::quit);
      zBusClient.open(QUrl("ws://192.168.0.157:8180"));
      zBusClient.sendZBusEvent(ZBusEvent(parser.value("send")));
      return app.exec();
  } else {
      ZBusCli zBusCli;
      QObject::connect(&app, &QCoreApplication::aboutToQuit,
                       &zBusCli, &ZBusCli::quit);
      zBusCli.exec();
      return app.exec();
  }
}

// TODO: proper signal handling
void handleSignal(int signum)
{
  if (signum == SIGINT || signum == SIGTERM)
  {
    exit(0);
  }
}
