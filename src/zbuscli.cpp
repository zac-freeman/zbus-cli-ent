#include "zbuscli.h"

#include "zbusevent.h"
#include "zwebsocket.h"

// Qt libraries MUST be imported before ncurses libraries.
// Somewhere in the depths of ncurses, there is a macro that redefines `timeout` globally.
#include <QCoreApplication>
#include <QList>
#include <QPair>
#include <QTimer>
#include <QQueue>

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

// Provides a visual indicator for the origin of an event.
static const QMap<Origin, QString> origin_sign{
    { Origin::Received, "-> " },
    { Origin::Sent, "<- " }
};

// Maps each mode to the corresponding help text to be displayed.
static const QMap<Mode, QString> help_text{
    { Mode::Command, "Ctrl+C) exit program, s) begin send mode, p) begin peruse mode" },
    { Mode::Send, "Ctrl+C) exit program, Esc) begin command mode, Tab) switch field, "
                   "Enter) send event" },
    { Mode::Peruse, "Ctrl+C) exit program, Esc) begin command mode" }
};

// Used for multiple return from handle_peruse_input
struct PeruseResult { Mode mode; int selection; };

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

    /* \brief Returns the index of the event in the event_history, nearest to the current top, that
     *        accomodates displaying the selected event on screen.
     *
     * \param <current_top> The index of the event currently at the top of the history window.
     * \param <next_selection> The index of the event to be selected and displayed in the history
     *                         window.
     */
    int find_top_for_selection(int current_top, int next_selection)
    {
        // if there is no event selected, return the most recent event as the next top
        if (next_selection == -1)
        {
            return event_history.size() - 1;
        }

        // if the next selection is at or above the current top,
        // return the next selection as the next top
        if (next_selection >= current_top)
        {
            return next_selection;
        }

        // get width and height of history window
        int rows = history.rows;
        int columns = history.columns;

        // determine the distance, in rows, from the current top through the next selection
        int distance = 0;
        QQueue<int> event_heights;
        for (int i = current_top; i >= next_selection; i--)
        {
            QPair<Origin, ZBusEvent> event = event_history.at(i);
            QString prefix = origin_sign[event.first];
            QString json = event.second.toJson();
            int height = ((prefix.size() + json.size() - 1) / columns) + 1;
            event_heights.enqueue(height);
            distance += height;
        }

        // if there is enough space in the terminal window to display the current top and the next
        // selection, return the current top as the next top
        if (distance <= rows)
        {
            return current_top;
        }

        // determine where the next top must be to accomodate the next selection
        for (int i = current_top; i >= next_selection; i--)
        {
            if (distance <= rows)
            {
                return i;
            }

            distance -= event_heights.dequeue();
        }

        // I think, if this is reached, the next selection is too large to fit on the terminal window
        // TODO: address possibility of too-large events
        return next_selection;
    }

    /* \brief Updates the history window with the event at the given top index at the top, and the
     *        given selection bolded.
     *
     * \param <top> The index of the event to be displayed at the top of the history window.
     * \param <selection> The index of the event to be bolded.
     */
    void update_history_window(int top, int selection)
    {
        // clear event history
        wclear(history_window);

        // get width and height of the terminal window
        int rows = history.rows;
        int columns = history.columns;

        // write events until running out of events or screen space
        int row = 0;
        for (int i = top; i >= 0; i--)
        {
            // determine the height (due to line-wrapping) of the next event to be written;
            // if the event would extend past the end of the history_window,
            // do not display that event or any subsequent events
            QPair<Origin, ZBusEvent> event = event_history.at(i);
            QString prefix = origin_sign[event.first];
            QString json = event.second.toJson();
            int height = ((prefix.size() + json.size() - 1) / columns) + 1;
            if (row + height > rows)
            {
                break;
            }

            // move to next row, and write event to line
            wmove(history_window, row, 0);

            // if the event has been selected, make it bold
            if (i == selection)
            {
                wattron(history_window, A_BOLD);
            }
            wprintw(history_window, prefix.toUtf8().data());
            wprintw(history_window, json.toUtf8().data());
            wattroff(history_window, A_BOLD);

            // set row for next event immediately after current event
            row = row + height;
        }

        // update screen
        wrefresh(history_window);
    }

    /* \brief Displays the help text corresponding to the given mode.
     *
     * \param <mode> Mode for which the help text should be displayed.
     */
    void update_help_text(Mode mode)
    {
        // TODO: handle multiline help text
        wmove(help_window, 0, 0);
        wclear(help_window);
        wprintw(help_window, help_text[mode].toUtf8().data());
        wrefresh(help_window);
    }

    /* \brief Adjusts the height of the event history window, depending on the given mode. If the given
     *        mode is not Send, the history window height is increased to cover the event entry fields.
     *        If the mode is Send, the history window height is decreased to reveal the event entry
     *        fields.
     *
     * \param <mode> Mode for which the history window should be resized.
     */
    void resize_history_window(Mode mode)
    {
        history.rows = (mode == Mode::Send) ? screen.rows - (entry.y + entry.rows)
                                            : screen.rows - (status.y + status.rows);
        history.y = screen.rows - history.rows;

        wresize(history_window, history.rows, history.columns);
        mvwin(history_window, history.y, history.x);
        wrefresh(history_window);

        // display the entry window if it is no longer overlapped by the history window
        if (mode == Mode::Send)
        {
            redrawwin(entry_window);
        }
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

/* \brief Starts an infinite loop that waits for input, processes pending Qt events, and updates the
 *        display. Forever.
 */
void ZBusCli::startEventLoop()
{
  int current_size = 0; // size of event_history during previous cycle of event loop
  int current_selection = -1; // the index of the event in event_history to be selected; -1 == none
  int current_top = 0; // the index of the event in event_history at the top of the history window
  Mode current_mode = Mode::Command; // the mode for processing fresh input

  // capture input, process pending Qt events, then update the display
  while (true)
  {
    int input = wgetch(p->entry_window);

    // process input with current mode, and capture next mode for next input
    int next_selection = current_selection;
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
            PeruseResult result = handle_peruse_input(input, current_selection);
            next_mode = result.mode;
            next_selection = result.selection;
            break;
    }

    // before updating the display, process pending Qt events
    QCoreApplication::processEvents();

    // if the mode has changed, update the help text and resize the history to fill the available
    // space
    if (current_mode != next_mode)
    {
        p->update_help_text(next_mode);
        p->resize_history_window(next_mode);
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

    // if there is a connection error, display it
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

    // if any new events have been received, or the event selection has changed,
    // update the event history
    int next_size = p->event_history.size();
    if (next_selection != current_selection || next_size > current_size)
    {
        int next_top = p->find_top_for_selection(current_top, next_selection);
        p->update_history_window(next_top, next_selection);

        current_size = next_size;
        current_selection = next_selection;
        current_top = next_top;
    }

    // if entry fields are visible, return cursor to last position in current field
    // otherwise, hide the cursor
    if (next_mode == Mode::Send)
    {
        curs_set(1);
        pos_form_cursor(p->entry_form);
    } else {
        curs_set(0);
    }

    // update mode for processing next input
    current_mode = next_mode;
  }
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
PeruseResult ZBusCli::handle_peruse_input(int input, int selection)
{
    switch(input)
    {
        // on Escape, determine whether or not this is the beginning of an "Escape Sequence"
        case '\033':
            input = wgetch(p->entry_window);

            // it's not an "Escape Sequence"; on Escape, switch to Command Mode, clear the
            // selection, and process subsequent input
            if (input != '[')
            {
                return { handle_command_input(input), -1};
            }


            // if the event history is empty, ignore selections
            if (p->event_history.empty())
            {
                return { Mode::Peruse, -1 };
            }

            // it's an "Escape Sequence"; on Arrow Key, select another event (arrow keys are
            // received in the form "Esc + [ + A")
            int latest_event = p->event_history.size() - 1;
            switch (wgetch(p->entry_window))
            {
                // Up
                case 'A':
                    // wrap around to latest event if no event or the last event is selected
                    if (selection == -1 || selection == latest_event)
                    {
                        return { Mode::Peruse, 0 };
                    }

                    return { Mode::Peruse, selection + 1};
                // Down
                case 'B':
                    // wrap around to latest event if first event is selected
                    return { Mode::Peruse, selection > 0 ? selection - 1 : latest_event };
            }
            break;
    }

    return { Mode::Peruse, selection };
}
