#include "zbuscli.h"

#include "zbusevent.h"
#include "zwebsocket.h"

#include <ncurses.h>
#include <QDebug>

class ZBusCliPrivate
{
public:
  QList<ZBusEvent> eventHistory;
  ZWebSocket client{"http://localhost"}; //zBus tries to ensure that clients are local

  int rows;
  int columns;
};

// TODO: send events through CLI
ZBusCli::ZBusCli(QObject *parent) : QObject(parent)
{
  p = new ZBusCliPrivate();

  connect(&p->client, &ZWebSocket::connected,
          this, &ZBusCli::onConnected);
  connect(&p->client, &ZWebSocket::disconnected,
          this, &ZBusCli::onDisconnected);
  connect(&p->client, &ZWebSocket::zBusEventReceived,
          this, &ZBusCli::onZBusEventReceived);
  connect(&p->client, SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(onError(QAbstractSocket::SocketError)));

}

ZBusCli::~ZBusCli()
{
  delete p;
}

void ZBusCli::exec()
{
  // start curses mode with character echoing and line buffering disabled
  initscr();
  noecho();
  cbreak();

  // get width and height of terminal
  getmaxyx(stdscr, p->rows, p->columns);

  // connect client to zBus server
  p->client.open(QUrl("ws://192.168.0.157:8180"));
}

void ZBusCli::quit()
{
  endwin();
}

void ZBusCli::onConnected() const
{
  move(p->rows - 2, 0);
  printw("status: connected to zBus");
  refresh();
}

void ZBusCli::onDisconnected() const
{
  move(p->rows - 2, 0);
  printw("status: disconnected from zBus");
  refresh();
}

// TODO: handle line wrapping
// TODO: handle size exceeding rows
void ZBusCli::onZBusEventReceived(const ZBusEvent &event)
{
  p->eventHistory.append(event);

  int size = p->eventHistory.size();

  for (int i = 0; i < size; i++)
  {
      // move to line, clear line, and write event to line
      move(p->rows - (size + 3 - i), 0);
      clrtoeol();
      printw(p->eventHistory.at(i).toJson().toUtf8().data());
      refresh();
  }
}

void ZBusCli::onError(QAbstractSocket::SocketError error) const
{
  move(p->rows - 2, 0);
  printw("error: " + error);
  refresh();
}
