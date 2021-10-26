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
static const int RETRY_DELAY_MS = 500;

// How long to wait for input before updating the display, in deciseconds.
static const int INPUT_WAIT_DS = 1;

// ncurses colors
static const int GREEN_TEXT = 1;
static const int RED_TEXT = 2;

/* Origin of events stored in zBus event history. All the events are either received from the zBus
 * server, or sent to the zBus server.
 */
enum class Origin { Received, Sent };

// Maps each mode to the corresponding help text to be displayed.
static const QMap<Mode, QString> help_text{
    { Mode::Command, "Ctrl+C) exit program, s) begin send mode, p) begin peruse mode" },
    { Mode::Send, "Ctrl+C) exit program, Esc) begin command mode, Tab) switch field, "
                   "Enter) send event" },
    { Mode::Peruse, "Ctrl+C) exit program, Esc) begin command mode" }
};

// Stores the dimension and position of an ncurses WINDOW object.
struct WINDOW_PROPERTIES
{
    int rows;
    int columns;
    int y;
    int x;
};

class ZBusCliPrivate
{
public:
  QList<QPair<Origin, ZBusEvent>> event_history;
  ZWebSocket client;

  FIELD *entry_fields[2] = {};
  FORM *entry_form = nullptr;

  WINDOW *help_window = nullptr;
  WINDOW *status_window = nullptr;
  WINDOW *entry_window = nullptr;
  WINDOW *entry_subwindow = nullptr;
  WINDOW *history_window = nullptr;

  WINDOW_PROPERTIES screen;
  WINDOW_PROPERTIES help;
  WINDOW_PROPERTIES status;
  WINDOW_PROPERTIES entry;
  WINDOW_PROPERTIES history;

  /* \brief Initializes ncurses and constructs the UI to use all available space in the terminal.
   */
  ZBusCliPrivate()
  {
      initscr();                // starts curses mode and instantiates stdscr
      start_color();            // enable using colors
      use_default_colors();     // maps -1 to current/default color
      keypad(stdscr, TRUE);     // function keys are captured as input like characters
      noecho();                 // input is not echo'd to the screen by default
      cbreak();                 // input is immediately captured, rather than after a line break
      nonl();                   // allows curses to detect the return key
      halfdelay(INPUT_WAIT_DS); // sets delay between infinite loop iterations

      // initialize ncurses colors
      init_pair(GREEN_TEXT, COLOR_GREEN, -1);
      init_pair(RED_TEXT, COLOR_RED, -1);

      // get width and height of screen
      getmaxyx(stdscr, screen.rows, screen.columns);

      // create window to display keybinds
      help.rows = 2;
      help.columns = screen.columns;
      help.y = 0;
      help.x = screen.columns - help.columns;
      help_window = newwin(help.rows, help.columns, help.y, help.x);
      wmove(help_window, 0, 0);
      wprintw(help_window, help_text[Mode::Command].toUtf8().data());
      wrefresh(help_window);

      // create window to display connection status with zBus
      status.rows = 3;
      status.columns = screen.columns;
      status.y = help.y + help.rows;
      status.x = screen.columns - status.columns;
      status_window = newwin(status.rows, status.columns, status.y, status.x);

      // create field for input of event sender and event type
      QString event_label = "event";
      int event_rows = 1;
      int event_columns = screen.columns - (event_label.size() + 1);
      int event_y = 0;
      int event_x = screen.columns - event_columns;
      entry_fields[0] = new_field(event_rows, event_columns, event_y, event_x, 0, 0);
      set_field_back(entry_fields[0], A_UNDERLINE);
      field_opts_off(entry_fields[0], O_AUTOSKIP);
      field_opts_off(entry_fields[0], O_STATIC);
      field_opts_off(entry_fields[0], O_BLANK);
      field_opts_off(entry_fields[0], O_WRAP);

      // create field for input of event data
      QString data_label = "data";
      int data_rows = 5;
      int data_columns = screen.columns - (data_label.size() + 1);
      int data_y = event_y + event_rows + 1;
      int data_x = screen.columns - data_columns;
      entry_fields[1] = new_field(data_rows, data_columns, data_y, data_x, 0, 0);
      set_field_back(entry_fields[1], A_UNDERLINE);
      field_opts_off(entry_fields[1], O_AUTOSKIP);
      field_opts_off(entry_fields[1], O_STATIC);
      field_opts_off(entry_fields[1], O_BLANK);
      field_opts_off(entry_fields[1], O_WRAP);

      // create window to contain event entry form
      entry.rows = (event_rows + 1) + (data_rows + 1);
      entry.columns = screen.columns;
      entry.y = status.y + status.rows;
      entry.x = screen.columns - entry.columns;
      entry_window = newwin(entry.rows, entry.columns, entry.y, entry.x);
      entry_subwindow = derwin(entry_window, entry.rows, entry.columns, 0, 0);

      // create event entry form to contain event entry fields
      entry_form = new_form(entry_fields);
      set_form_win(entry_form, entry_window);
      set_form_sub(entry_form, entry_subwindow);
      post_form(entry_form);

      // add labels for event entry fields
      wmove(entry_window, 0, 0);
      wprintw(entry_window, "event");
      wmove(entry_window, 2, 0);
      wprintw(entry_window, "data");
      wrefresh(entry_window);

      // create window for event history, using the remaining rows in the screen
      history.rows = screen.rows - (status.y + status.rows);
      history.columns = screen.columns;
      history.y = screen.rows - history.rows;
      history.x = screen.columns - history.columns;
      history_window = newwin(history.rows, history.columns, history.y, history.x);
      wmove(history_window, 0, 0);
      wprintw(history_window, "Events broadcast by the zBus server will appear here.");
      wrefresh(history_window);
  }

  /* \brief Frees up memory occupied by the ncurses windows, forms, and fields, then ends curses
   *        mode, returning the terminal to its regular appearance and behavior.
   */
  ~ZBusCliPrivate()
  {
      delwin(help_window);
      delwin(entry_window);
      delwin(entry_subwindow);
      delwin(status_window);
      delwin(history_window);

      free_form(entry_form);
      free_field(entry_fields[0]);
      free_field(entry_fields[1]);

      endwin();
  }
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
 *        stores a copy in the event_history list.
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
  p->event_history.append({Origin::Sent, zBusEvent});
  return p->client.sendZBusEvent(zBusEvent);
}

/* \brief Stores the given event in the event_history list.
 *
 *        This is connected to ZWebSocket's zBusEventReceived signal in order to save all received
 *        events.
 *
 * \param <event> Event received from zBus.
 */
void ZBusCli::onZBusEventReceived(const ZBusEvent &event)
{
  p->event_history.append({Origin::Received, event});
}

/* \brief Handles input received while the client is in Command mode.
 *
 * \param <input> Character code of keypress from keyboard.
 *
 * \returns The Mode that further input should be processed with.
 */
Mode ZBusCli::handle_command_input(int input)
{
    switch(input)
    {
        case 's':
            return Mode::Send;

        case 'p':
            return Mode::Peruse;

        default:
            return Mode::Command;
    }
}

/* \brief Handles input received while the client is in Send mode.
 *
 * \param <input> Character code of keypress from keyboard.
 *
 * \returns The Mode that further input should be processed with.
 */
Mode ZBusCli::handle_send_input(int input)
{
    switch(input)
    {
        // if no input was received during the wait, move on
        case ERR:
            break;

        // on Tab, move the cursor to the next field
        case '\t':
        case KEY_STAB:
            form_driver(p->entry_form, REQ_NEXT_FIELD);
            form_driver(p->entry_form, REQ_END_LINE);
            break;

        // On Enter, send the contents of the fields to zBus
        case '\r':
        case '\n':
        case KEY_ENTER:
            form_driver(p->entry_form, REQ_VALIDATION);
            emit eventSubmitted(field_buffer(p->entry_fields[0], 0), field_buffer(p->entry_fields[1], 0));
            break;

        // on Backspace, backspace
        case 127:
        case KEY_BACKSPACE:
            form_driver(p->entry_form, REQ_DEL_PREV);
            break;

        // on Escape, determine whether or not this is the beginning of an "Escape Sequence"
        case '\033':
            input = wgetch(p->entry_window);

            // it's not an "Escape Sequence"; on Escape, switch to Command Mode and process
            // subsequent input
            if (input != '[')
            {
                return handle_command_input(input);
            }

            // it's an "Escape Sequence"; on Arrow Key, move cursor (arrow keys are received in the
            // form "Esc + [ + A")
            switch (wgetch(p->entry_window))
            {
                // Up
                case 'A': 
                    form_driver(p->entry_form, REQ_UP_CHAR);
                    break;
                // Down
                case 'B': 
                    form_driver(p->entry_form, REQ_DOWN_CHAR);
                    break;
                // Right
                case 'C': 
                    form_driver(p->entry_form, REQ_RIGHT_CHAR);
                    break;
                // Left
                case 'D': 
                    form_driver(p->entry_form, REQ_LEFT_CHAR);
                    break;
            }
            break;

        // provide all other input as characters to the current field
        default:
            form_driver(p->entry_form, input);
    }

    return Mode::Send;
}

/* \brief Handles input received while the client is in Peruse mode.
 *
 * \param <input> Character code of keypress from keyboard.
 *
 * \returns The Mode that further input should be processed with.
 */
Mode ZBusCli::handle_peruse_input(int input)
{
    switch(input)
    {
        case '\033':
            return Mode::Command;

        default:
            return Mode::Peruse;
    }
}

/* \brief Displays the help text corresponding to the given mode.
 *
 * \param <mode> Mode for which the help text should be displayed.
 */
void ZBusCli::update_help_text(Mode mode)
{
    // TODO: handle multiline help text
    wmove(p->help_window, 0, 0);
    wclear(p->help_window);
    wprintw(p->help_window, help_text[mode].toUtf8().data());
    wrefresh(p->help_window);
}

/* \brief Adjusts the height of the event history window, depending on the given mode. If the given
 *        mode is not Send, the history window height is increased to cover the event entry fields.
 *        If the mode is Send, the history window height is decreased to reveal the event entry
 *        fields.
 *
 * \param <mode> Mode for which the history window should be resized.
 */
void ZBusCli::resize_history_window(Mode mode)
{
    p->history.rows = (mode == Mode::Send) ? p->screen.rows - (p->entry.y + p->entry.rows)
                                           : p->screen.rows - (p->status.y + p->status.rows);
    p->history.y = p->screen.rows - p->history.rows;

    wresize(p->history_window, p->history.rows, p->history.columns);
    mvwin(p->history_window, p->history.y, p->history.x);
    wrefresh(p->history_window);

    if (mode == Mode::Send)
    {
        redrawwin(p->entry_window);
    }
}

/* \brief Starts an infinite loop that waits for input, processes pending Qt events, and updates the
 *        display. Forever.
 */
void ZBusCli::startEventLoop()
{
  int previousSize = 0; // size of event_history during previous cycle of event loop
  Mode current_mode = Mode::Command; // the mode for processing fresh input

  // capture input, process pending Qt events, then update the display
  while (true)
  {
    int input = wgetch(p->entry_window);

    // process input with current mode, and capture next mode for next input
    Mode next_mode = current_mode;
    switch(current_mode)
    {
        case Mode::Command:
            next_mode = handle_command_input(input);
            break;

        case Mode::Send:
            next_mode = handle_send_input(input);
            break;

        case Mode::Peruse:
            next_mode = handle_peruse_input(input);
            break;
    }

    // before updating the display, process pending Qt events
    QCoreApplication::processEvents();

    // if the mode has changed, update the help text and resize the history to fill the available
    // space
    if (current_mode != next_mode)
    {
        update_help_text(next_mode);
        resize_history_window(next_mode);
        // TODO: manage cursor position wrt mode
    }

    // update the connection status message
    if (p->client.isValid())
    {
        wmove(p->status_window, 0, 0);
        wclear(p->status_window);
        wattron(p->status_window, COLOR_PAIR(GREEN_TEXT) | A_BOLD);
        wprintw(p->status_window, "status: connected to zBus");
        wattroff(p->status_window, COLOR_PAIR(GREEN_TEXT) | A_BOLD);
        wrefresh(p->status_window);
    } else {
        wmove(p->status_window, 0, 0);
        wclrtoeol(p->status_window);
        wattron(p->status_window, COLOR_PAIR(RED_TEXT) | A_BOLD);
        wprintw(p->status_window, "status: disconnected from zBus");
        wattroff(p->status_window, COLOR_PAIR(RED_TEXT) | A_BOLD);
        wrefresh(p->status_window);
    }

    // display the connection error message, if there is one
    if (p->client.error() != QAbstractSocket::UnknownSocketError)
    {
        wmove(p->status_window, 1, 0);
        wclrtoeol(p->status_window);
        wattron(p->status_window, COLOR_PAIR(RED_TEXT) | A_BOLD);
        wprintw(p->status_window, "error: ");
        wprintw(p->status_window, p->client.errorString().toUtf8().data());
        wattroff(p->status_window, COLOR_PAIR(RED_TEXT) | A_BOLD);
        wrefresh(p->status_window);
    }

    // update the event history, if any new events have been received
    if (p->event_history.size() > previousSize)
    {
        previousSize = p->event_history.size();

        // clear event history
        wclear(p->history_window);

        // get width and height of window
        int rows, columns;
        getmaxyx(p->history_window, rows, columns);

        // write events until running out of events or screen space
        int row = 0;
        int size = p->event_history.size();
        for (int i = size - 1; i >= 0; i--)
        {
            // determine the height (due to line-wrapping) of the next event to be written;
            // if the event would extend past the end of the history_window,
            // do not display that event or any subsequent events
            Origin origin = p->event_history.at(i).first;
            QString json = p->event_history.at(i).second.toJson();
            QString delimiter = ">>> ";
            int height = ((json.size() + delimiter.size() - 1) / columns) + 1;
            if (row + height > rows) {
                break;
            }

            // move to next row, and write event to line
            // if the event was sent from this client, make it bold
            wmove(p->history_window, row, 0);
            if (origin == Origin::Sent)
            {
                wattron(p->history_window, A_BOLD);
            }
            wprintw(p->history_window, delimiter.toUtf8().data());
            wprintw(p->history_window, json.toUtf8().data());
            wattroff(p->history_window, A_BOLD);

            // set row for next event immediately after current event
            row = row + height;
        }

        // update screen
        wrefresh(p->history_window);
    }

    // update mode for processing next input
    current_mode = next_mode;

    // return cursor to last position in current field
    pos_form_cursor(p->entry_form);
  }
}
