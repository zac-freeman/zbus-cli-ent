// TODO: add unit tests
// TODO: add README
// TODO: remove zbusevent dep
#include "zbuscli.h"
#include "zbusevent.h"
#include "zwebsocket.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QObject>
#include <QUrl>

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

  // TODO: dont parse commas as separator
  // TODO: send multiple events
  // TODO: validate provided json?
  // TODO: check signals in send mode
  if (parser.isSet("send"))
  {
      ZWebSocket zBusClient;
      QObject::connect(&zBusClient, &ZWebSocket::processedEventQueue,
                       &app, &QCoreApplication::quit);
      zBusClient.open(zBusUrl);
      zBusClient.sendZBusEvent(ZBusEvent(parser.value("send")));
      return app.exec();
  } else {
      ZBusCli zBusCli;
      zBusCli.exec(zBusUrl);
      return app.exec();
  }
}
