#include "zbuscli.h"

#include "zbusevent.h"
#include "zwebsocket.h"

// Qt libraries MUST be imported before ncurses libraries.
// Somewhere in the depths of ncurses, there is a macro that redefines `timeout` globally.
#include <QCoreApplication>
#include <QList>
#include <QPair>
#include <QTimer>

// ncurses libraries
#include <form.h>
#include <ncurses.h>

// How long to wait after a disconnection from zBus to retry connecting, in milliseconds.
static const int RETRY_DELAY_MS = 1000;

// How long to wait for input before updating the display, in deciseconds.
static const int INPUT_WAIT_DS = 10;

// ncurses colors
static const int GREEN_TEXT = 1;
static const int RED_TEXT = 2;

/* Origin of events stored in zBus event history. All the events are either received from the zBus
 * server, or sent to the zBus server.
 */
enum class Origin { Received, Sent };

class ZBusCliPrivate
{
public:

  /* \brief Initializes ncurses and constructs the UI to use all available space in the terminal.
   */
  ZBusCliPrivate()
  {
      initscr();                // starts curses mode and instantiates stdscr
      start_color();            // enable using colors
      use_default_colors();     // maps -1 to current/default color
      keypad(stdscr, true);     // function keys are captured as input like characters
      noecho();                 // input is not echo'd to the screen by default
      cbreak();                 // input is immediately captured, rather than after a line break
      nonl();                   // allows curses to detect the return key
      halfdelay(INPUT_WAIT_DS); // sets delay between infinite loop iterations

      // initialize ncurses colors
      init_pair(GREEN_TEXT, COLOR_GREEN, -1);
      init_pair(RED_TEXT, COLOR_RED, -1);

      // get width and height of screen
      int screenRows, screenColumns;
      getmaxyx(stdscr, screenRows, screenColumns);

      // create window to display keybinds
      QString helpText = "Ctrl+C) exit program  Tab) switch field  Enter) send event";
      int helpRows = 1;
      int helpColumns = screenColumns;
      int helpY = 0;
      int helpX = screenColumns - helpColumns;
      helpWindow = newwin(helpRows, helpColumns, helpY, helpX);
      wmove(helpWindow, 0, 0);
      wprintw(helpWindow, helpText.toUtf8().data());
      wrefresh(helpWindow);

      // create window to display connection status with zBus
      int statusRows = 2;
      int statusColumns = screenColumns;
      int statusY = helpY + helpRows + 1;
      int statusX = screenColumns - statusColumns;
      statusWindow = newwin(statusRows, statusColumns, statusY, statusX);

      // create field for input of event sender and event type
      QString eventLabel = "event";
      int eventRows = 1;
      int eventColumns = screenColumns - (eventLabel.size() + 1);
      int eventY = 0;
      int eventX = screenColumns - eventColumns;
      entryFields[0] = new_field(eventRows, eventColumns, eventY, eventX, 0, 0);
      set_field_back(entryFields[0], A_UNDERLINE);
      field_opts_off(entryFields[0], O_AUTOSKIP);
      field_opts_off(entryFields[0], O_STATIC);
      field_opts_off(entryFields[0], O_BLANK);

      // create field for input of event data
      QString dataLabel = "data";
      int dataRows = 5;
      int dataColumns = screenColumns - (dataLabel.size() + 1);
      int dataY = eventY + eventRows + 1;
      int dataX = screenColumns - dataColumns;
      entryFields[1] = new_field(dataRows, dataColumns, dataY, dataX, 0, 0);
      set_field_back(entryFields[1], A_UNDERLINE);
      field_opts_off(entryFields[1], O_AUTOSKIP);
      field_opts_off(entryFields[1], O_STATIC);
      field_opts_off(entryFields[1], O_BLANK);

      // create window to contain event entry form
      int entryRows = 7;
      int entryColumns = screenColumns;
      int entryY = statusY + statusRows + 1;
      int entryX = screenColumns - entryColumns;
      entryWindow = newwin(entryRows, entryColumns, entryY, entryX);
      entrySubwindow = derwin(entryWindow, entryRows, entryColumns, 0, 0);

      // create event entry form to contain event entry fields
      entryForm = new_form(entryFields);
      set_form_win(entryForm, entryWindow);
      set_form_sub(entryForm, entrySubwindow);
      post_form(entryForm);

      // add labels for event entry fields
      wmove(entryWindow, 0, 0);
      wprintw(entryWindow, "event");
      wmove(entryWindow, 2, 0);
      wprintw(entryWindow, "data");
      wrefresh(entryWindow);

      // create window for event history, using the remaining rows in the screen
      int historyRows = screenRows - (entryY + entryRows + 1);
      int historyColumns = screenColumns;
      int historyY = screenRows - historyRows;
      int historyX = screenColumns - historyColumns;
      historyWindow = newwin(historyRows, historyColumns, historyY, historyX);

      // move cursor to start of event entry field
      pos_form_cursor(entryForm);
  }

  /* \brief Frees up memory occupied by the ncurses windows, forms, and fields, then ends curses
   *        mode, returning the terminal to its regular appearance and behavior.
   */
  ~ZBusCliPrivate()
  {
      delwin(helpWindow);
      delwin(entryWindow);
      delwin(entrySubwindow);
      delwin(statusWindow);
      delwin(historyWindow);

      free_form(entryForm);
      free_field(entryFields[0]);
      free_field(entryFields[1]);

      endwin();
  }

  // TODO: display inputted events in history, as well?
  // TODO: and differentiate received events from sent events
  QList<QPair<Origin, ZBusEvent>> eventHistory;
  ZWebSocket client;

  FIELD *entryFields[2] = {};
  FORM *entryForm = nullptr;

  WINDOW *helpWindow = nullptr;
  WINDOW *statusWindow = nullptr;
  WINDOW *entryWindow = nullptr;
  WINDOW *entrySubwindow = nullptr;
  WINDOW *historyWindow = nullptr;
};

/* \brief Constructs an instance of ZBusCli, setting up the retry logic and the connections between
 *        the zBus client and the ncurses event loop.
 *
 * \param <parent> The parent of the object instantiated.
 */
ZBusCli::ZBusCli(QObject *parent) : QObject(parent)
{
  p = new ZBusCliPrivate();

  connect(&p->client, &ZWebSocket::disconnected,
          this, &ZBusCli::onDisconnected);
  connect(this, &ZBusCli::eventSubmitted,
          this, &ZBusCli::onEventSubmitted);
  connect(&p->client, &ZWebSocket::zBusEventReceived,
          this, &ZBusCli::onZBusEventReceived);
}

/* \brief Cleans up the PIMPL object.
 */
ZBusCli::~ZBusCli()
{
  delete p;
}

/* \brief Connects the zBus client to the zBus server at the given URL, and starts the ncurses event
 *        loop.
 *
 * \param <zBusUrl> URL to zBus.
 */
void ZBusCli::exec(const QUrl &zBusUrl)
{
  // connect client to zBus server
  p->client.open(zBusUrl);

  // wait for input and update display
  startEventLoop();
}

/* \brief Attempts to connect to zBus after a delay. This is connected to ZWebSocket's disconnected
 *        signal to enable retries.
 */
void ZBusCli::onDisconnected()
{
  QUrl zBusUrl = p->client.requestUrl();
  QTimer::singleShot(RETRY_DELAY_MS, [this, zBusUrl] {p->client.open(zBusUrl);});
}

/* \brief Constructs a ZBusEvent from the given event string and data string, sends it to zBus, and
 *        stores a copy in the eventHistory list.
 *
 *        This is connected to the eventSubmitted signal that is emitted from the ncurses event loop
 *        to enable sending events from the text-based UI.
 *
 * \param <event> String containing the event sender and event type.
 * \param <data> String containing the event data.
 *
 * \returns Number of bytes sent.
 */
qint64 ZBusCli::onEventSubmitted(const QString &event, const QString &data)
{
  ZBusEvent zBusEvent = ZBusEvent(event.trimmed(), data.trimmed());
  p->eventHistory.append({Origin::Sent, zBusEvent});
  return p->client.sendZBusEvent(zBusEvent);
}

/* \brief Stores the given event in the eventHistory list.
 *
 *        This is connected to ZWebSocket's zBusEventReceived signal in order to save all received
 *        events.
 *
 * \param <event> Event received from zBus.
 */
void ZBusCli::onZBusEventReceived(const ZBusEvent &event)
{
  p->eventHistory.append({Origin::Received, event});
}

/* \brief Starts an infinite loop that waits for input, processes pending Qt events, and updates the
 *        display. Forever.
 */
void ZBusCli::startEventLoop()
{
  // size of eventHistory during previous cycle of event loop
  int previousSize = 0;

  // capture input, process pending Qt events, then update the display
  while (true)
  {
    int input = wgetch(p->entryWindow);
    switch(input)
    {
        // if no input was received during the wait, move on
        case ERR:
            break;

        // on Tab, move the cursor to the next field
        case '\t':
        case KEY_STAB:
            form_driver(p->entryForm, REQ_NEXT_FIELD);
            form_driver(p->entryForm, REQ_END_LINE);
            break;

        // On Enter, send the contents of the fields to zBus
        case '\r':
        case '\n':
        case KEY_ENTER:
            form_driver(p->entryForm, REQ_VALIDATION);
            emit eventSubmitted(field_buffer(p->entryFields[0], 0), field_buffer(p->entryFields[1], 0));
            break;

        // on Backspace, backspace
        case 127:
        case KEY_BACKSPACE:
            form_driver(p->entryForm, REQ_DEL_PREV);
            break;

        // on arrow key, move cursor (arrow keys are received as Esc+[+A)
        case '\033':
            wgetch(p->entryWindow);
            switch(wgetch(p->entryWindow))
            {
                // arrow up
                case 'A': 
                    form_driver(p->entryForm, REQ_UP_CHAR);
                    break;
                // arrow down
                case 'B': 
                    form_driver(p->entryForm, REQ_DOWN_CHAR);
                    break;
                // arrow right
                case 'C': 
                    form_driver(p->entryForm, REQ_RIGHT_CHAR);
                    break;
                // arrow left
                case 'D': 
                    form_driver(p->entryForm, REQ_LEFT_CHAR);
                    break;
            }
            break;

        // provide all other input as characters to the current field
        default:
            form_driver(p->entryForm, input);
    }

    // before updating the display, process pending Qt events
    QCoreApplication::processEvents();

    // update the connection status message
    if (p->client.isValid())
    {
        wmove(p->statusWindow, 0, 0);
        wclear(p->statusWindow);
        wattron(p->statusWindow, A_BOLD);
        wattron(p->statusWindow, COLOR_PAIR(GREEN_TEXT));
        wprintw(p->statusWindow, "status: connected to zBus");
        wattroff(p->statusWindow, A_BOLD);
        wattroff(p->statusWindow, COLOR_PAIR(GREEN_TEXT));
        wrefresh(p->statusWindow);
    } else {
        wmove(p->statusWindow, 0, 0);
        wclrtoeol(p->statusWindow);
        wattron(p->statusWindow, A_BOLD);
        wattron(p->statusWindow, COLOR_PAIR(RED_TEXT) | A_BOLD);
        wprintw(p->statusWindow, "status: disconnected from zBus");
        wattroff(p->statusWindow, A_BOLD);
        wattroff(p->statusWindow, COLOR_PAIR(RED_TEXT) | A_BOLD);
        wrefresh(p->statusWindow);
    }

    // display the connection error message, if there is one
    if (p->client.error() != QAbstractSocket::UnknownSocketError)
    {
        wmove(p->statusWindow, 1, 0);
        wclrtoeol(p->statusWindow);
        wattron(p->statusWindow, A_BOLD);
        wattron(p->statusWindow, COLOR_PAIR(RED_TEXT));
        wprintw(p->statusWindow, "error: ");
        wprintw(p->statusWindow, p->client.errorString().toUtf8().data());
        wattroff(p->statusWindow, A_BOLD);
        wattroff(p->statusWindow, COLOR_PAIR(RED_TEXT));
        wrefresh(p->statusWindow);
    }

    // update the event history, if any new events have been received
    if (p->eventHistory.size() > previousSize)
    {
        previousSize = p->eventHistory.size();

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
            Origin origin = p->eventHistory.at(i).first;
            QString json = p->eventHistory.at(i).second.toJson();
            int height = ((json.size() - 1) / columns) + 1;
            if (row + height > rows) {
                break;
            }

            // move to next row, and write event to line
            // if the event was sent from this client, make it bold
            wmove(p->historyWindow, row, 0);
            if (origin == Origin::Sent)
            {
                wattron(p->historyWindow, A_BOLD);
            }
            wprintw(p->historyWindow, json.toUtf8().data());
            wattroff(p->historyWindow, A_BOLD);

            // set row for next event immediately after current event
            row = row + height;
        }

        // update screen
        wrefresh(p->historyWindow);
    }

    // return cursor to last position in current field
    pos_form_cursor(p->entryForm);
  }
}
