#include "zbuscli.h"

#include "zbusevent.h"
#include "zwebsocket.h"

#include <ncurses.h>

class ZBusCliPrivate
{
public:
  QList<ZBusEvent> eventHistory;
  ZWebSocket client{"http://localhost"}; //zBus tries to ensure that clients are local
  WINDOW *helpWindow;
  WINDOW *entryWindow;
  WINDOW *statusWindow;
  WINDOW *historyWindow;
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
  endwin();
  delete p;
}

void ZBusCli::exec()
{
  // start curses mode with character echoing and line buffering disabled
  initscr();
  noecho();
  cbreak();

  // get width and height of screen
  int screenRows, screenColumns;
  getmaxyx(stdscr, screenRows, screenColumns);

  // create window to display keybinds
  int helpRows = 1;
  int helpColumns = screenColumns;
  int helpY = 0;
  int helpX = screenColumns - helpColumns;
  p->helpWindow = newwin(helpRows, helpColumns, helpY, helpX);
  wmove(p->helpWindow, 0, 0);
  wprintw(p->helpWindow, "Esc) exit"); // TODO: make this true
  wrefresh(p->helpWindow);

  // create window to display connection status with zBus
  int statusRows = 2;
  int statusColumns = screenColumns;
  int statusY = helpY + helpRows + 1;
  int statusX = screenColumns - statusColumns;
  p->statusWindow = newwin(statusRows, statusColumns, statusY, statusX);

  // TODO: event entry form

  // create window for event history, using the remaining rows in the screen
  int historyRows = screenRows - (statusY + statusRows + 1);
  int historyColumns = screenColumns;
  int historyY = screenRows - historyRows;
  int historyX = screenColumns - historyColumns;
  p->historyWindow = newwin(historyRows, historyColumns, historyY, historyX);

  // connect client to zBus server
  p->client.open(QUrl("ws://192.168.0.157:8180"));
}

// TODO: add color
void ZBusCli::onConnected() const
{
  wmove(p->statusWindow, 0, 0);
  wclear(p->statusWindow);
  wprintw(p->statusWindow, "status: connected to zBus");
  wrefresh(p->statusWindow);
}

// TODO: add color
void ZBusCli::onDisconnected() const
{
  wmove(p->statusWindow, 0, 0);
  clrtoeol();
  wprintw(p->statusWindow, "status: disconnected from zBus");
  wrefresh(p->statusWindow);
}

void ZBusCli::onZBusEventReceived(const ZBusEvent &event)
{
  p->eventHistory.append(event);

  // clear event history
  wclear(p->historyWindow);

  // get width and height of window
  int rows, columns;
  getmaxyx(p->historyWindow, rows, columns);

  // write events until running out of events or screen space
  int row = 0;
  int size = p->eventHistory.size();
  for (int i = size - 1; i >= 0; i--)
  {
      // determine the height (due to line-wrapping) of the next event to be written;
      // if the event would extend past the end of the historyWindow,
      // do not display that event or any subsequent events
      QString json = p->eventHistory.at(i).toJson();
      int height = ((json.size() - 1) / columns) + 1;
      if (row + height > rows) {
          break;
      }

      // move to next row, and write event to line
      wmove(p->historyWindow, row, 0);
      wprintw(p->historyWindow, json.toUtf8().data());

      // set row for next event immediately after current event
      row = row + height;
  }

  // update screen
  wrefresh(p->historyWindow);
}

// TODO: add color
void ZBusCli::onError(QAbstractSocket::SocketError error) const
{
  Q_UNUSED(error);
  wmove(p->statusWindow, 1, 0);
  clrtoeol();
  wprintw(p->statusWindow, "error: ");
  wprintw(p->statusWindow, p->client.errorString().toUtf8().data());
  wrefresh(p->statusWindow);
}
