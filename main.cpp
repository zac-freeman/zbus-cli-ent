// TODO: add unit tests
// TODO: add README
// TODO: add comments
#include "zbuscli.h"
#include "zbusevent.h"
#include "zwebsocket.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QList>
#include <QObject>
#include <QUrl>
#include <signal.h>

void handleSignal(int signum);

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName("zbus-cli-ent");
  QCoreApplication::setApplicationVersion("0.1");

  QCommandLineParser parser;
  parser.setApplicationDescription("text-based zbus client");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOption({{"w", "websocket"},
                    QCoreApplication::translate("main", "connect to zBus at websocket <url>"),
                    QCoreApplication::translate("main", "url")});
  parser.addOption({{"s", "send"},
                    QCoreApplication::translate("main", "send json-formatted zBus <event>"),
                    QCoreApplication::translate("main", "event")});

  parser.process(app);

  if (!parser.isSet("websocket"))
  {
      qWarning() << "URL to zBus websocket is required. See --help for more info.";
      return 1;
  }

  QUrl zBusUrl = QUrl(parser.value("websocket"));
  if (!zBusUrl.isValid())
  {
      qWarning() << "The provided zBus websocket URL is invalid.";
      return 1;
  }

  // TODO: send multiple events
  if (parser.isSet("send"))
  {
      // quit application upon receiving signal to quit (e.g. Ctrl+C)
      signal(SIGINT, handleSignal);
      signal(SIGTERM, handleSignal);

      ZWebSocket zBusClient;

      // quit application after processing provided events
      QObject::connect(&zBusClient, &ZWebSocket::processedEventQueue,
                       &app, &QCoreApplication::quit);

      // send zBus events to client, then connect to zBus server
      // this ensures a processedEventQueue signal is always emitted
      zBusClient.sendZBusEvents(parser.values("send"));
      zBusClient.open(zBusUrl);

      return app.exec();
  } else {
      ZBusCli zBusCli;
      zBusCli.exec(zBusUrl);
      return app.exec();
  }
}

void handleSignal(int signum)
{
  if ((signum == SIGINT) || (signum == SIGTERM))
  {
      exit(signum);
  }
}
