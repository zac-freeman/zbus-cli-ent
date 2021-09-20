#include "zbuscli.h"

#include "zbusevent.h"
#include "zwebsocket.h"

#include <form.h>
#include <ncurses.h>
#include <thread>

class ZBusCliPrivate
{
public:
  ZBusCliPrivate(ZBusCli *parent) : receiver(&ZBusCli::run, parent) {}

  int previousSize = 0;
  QList<ZBusEvent> eventHistory;
  ZWebSocket client{"http://localhost"}; //zBus tries to ensure that clients are local
  std::thread receiver;

  WINDOW *helpWindow;
  WINDOW *entryWindow;
  WINDOW *entrySubwindow;
  WINDOW *statusWindow;
  WINDOW *historyWindow;

  FIELD *entryFields[2];
  FORM *entryForm;
};

// TODO: send events through CLI
ZBusCli::ZBusCli(QObject *parent) : QObject(parent)
{
  p = new ZBusCliPrivate(this);

  connect(&p->client, &ZWebSocket::zBusEventReceived,
          this, &ZBusCli::onZBusEventReceived);
}

ZBusCli::~ZBusCli()
{
  delwin(p->helpWindow);
  delwin(p->entryWindow);
  delwin(p->entrySubwindow);
  delwin(p->statusWindow);
  delwin(p->historyWindow);

  free_form(p->entryForm);
  free_field(p->entryFields[0]);
  free_field(p->entryFields[1]);

  endwin();
  delete p;
}

void ZBusCli::exec()
{
  // connect client to zBus server
  p->client.open(QUrl("ws://10.0.0.40:8180"));

  // wait for input in another thread
  p->receiver.detach();
}

void ZBusCli::run()
{
  // start curses mode with character echoing and line buffering disabled
  initscr();
  keypad(stdscr, true);
  noecho();
  cbreak();
  halfdelay(10);

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
  wprintw(p->helpWindow, "Esc) exit program  Tab) move cursor to next field"); // TODO: make this true
  wrefresh(p->helpWindow);

  // create window to display connection status with zBus
  int statusRows = 2;
  int statusColumns = screenColumns;
  int statusY = helpY + helpRows + 1;
  int statusX = screenColumns - statusColumns;
  p->statusWindow = newwin(statusRows, statusColumns, statusY, statusX);

  // create event entry form
  p->entryFields[0] = new_field(1, screenColumns - 6, 0, 6, 0, 0);
  set_field_back(p->entryFields[0], A_UNDERLINE);
  field_opts_off(p->entryFields[0], O_AUTOSKIP);
  field_opts_off(p->entryFields[0], O_STATIC);
  field_opts_off(p->entryFields[0], O_BLANK);

  p->entryFields[1] = new_field(5, screenColumns - 5, 2, 5, 0, 0);
  set_field_back(p->entryFields[1], A_UNDERLINE);
  field_opts_off(p->entryFields[1], O_AUTOSKIP);
  field_opts_off(p->entryFields[1], O_STATIC);
  field_opts_off(p->entryFields[1], O_BLANK);

  p->entryForm = new_form(p->entryFields);

  int entryRows = 7;
  int entryColumns = screenColumns;
  int entryY = statusY + statusRows + 1;
  int entryX = screenColumns - entryColumns;
  p->entryWindow = newwin(entryRows, entryColumns, entryY, entryX);
  p->entrySubwindow = derwin(p->entryWindow, entryRows, entryColumns, 0, 0);
  set_form_win(p->entryForm, p->entryWindow);
  set_form_sub(p->entryForm, p->entrySubwindow);
  post_form(p->entryForm);
  wmove(p->entryWindow, 0, 0);
  wprintw(p->entryWindow, "event");
  wmove(p->entryWindow, 2, 0);
  wprintw(p->entryWindow, "data");
  wrefresh(p->entryWindow);
  form_driver(p->entryForm, REQ_END_LINE);

  // create window for event history, using the remaining rows in the screen
  int historyRows = screenRows - (entryY + entryRows + 1);
  int historyColumns = screenColumns;
  int historyY = screenRows - historyRows;
  int historyX = screenColumns - historyColumns;
  p->historyWindow = newwin(historyRows, historyColumns, historyY, historyX);

  // capture input and update display
  while (true)
  {
    char input = wgetch(p->entryWindow);
    switch(input)
    {
        case ERR:
            break;
        case 127:
            form_driver(p->entryForm, REQ_DEL_PREV);
            break;
        case '\t':
            form_driver(p->entryForm, REQ_NEXT_FIELD);
            form_driver(p->entryForm, REQ_END_LINE);
            break;
        default:
            form_driver(p->entryForm, input);
    }

    if (p->client.isValid())
    {
        wmove(p->statusWindow, 0, 0);
        wclear(p->statusWindow);
        wprintw(p->statusWindow, "status: connected to zBus");
        wrefresh(p->statusWindow);
    } else {
        wmove(p->statusWindow, 0, 0);
        clrtoeol();
        wprintw(p->statusWindow, "status: disconnected from zBus");
        wrefresh(p->statusWindow);
    }

    if (p->client.error() != QAbstractSocket::UnknownSocketError)
    {
        wmove(p->statusWindow, 1, 0);
        clrtoeol();
        wprintw(p->statusWindow, "error: ");
        wprintw(p->statusWindow, p->client.errorString().toUtf8().data());
        wrefresh(p->statusWindow);
    }

    if (p->eventHistory.size() > p->previousSize)
    {
        p->previousSize = p->eventHistory.size();

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

    // TODO: return cursor to previous position in entry form
  }
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
