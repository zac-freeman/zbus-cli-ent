#include "zbuscli.h"

#include "zbusevent.h"
#include "zwebsocket.h"

// Qt libraries MUST be imported before ncurses libraries.
// Somewhere in the depths of ncurses, there is a macro that redefines `timeout` globally.
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonValue>
#include <QList>
#include <QPair>
#include <QTimer>
#include <QQueue>
#include <QVector>

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
static const QMap<Origin, QString> origin_sign
{
    { Origin::Received, "-> " },
    { Origin::Sent, "<- " }
};

// Maps each mode to the corresponding help text to be displayed.
static const QMap<Mode, QString> help_text
{
    { Mode::Command, "Ctrl+C) exit program, s) begin send mode, p) begin peruse mode, "
                     "m) toggle pinpad simulator" },
    { Mode::Send, "Ctrl+C) exit program, Esc) begin command mode, Tab) switch field, "
                  "Enter) send event" },
    { Mode::Peruse, "Ctrl+C) exit program, Esc) begin command mode" }
};

/* A container for the data associated with each entry in the mock menu. An instance of
 * `MockMenuEntry` should set EXACTLY ONE OF its `menu` and `mock` members. The unset value should
 * be set to its "None" value.
 *
 * This type handles the fact that a menu entry can point to EITHER another menu OR a mocked event.
 * Encapsulating that behavior in this type reduces the complexity of the structure representing the
 * mock menu.
 */
struct MockMenuEntry
{
    QString text;
    enum Menu menu = Menu::None;
    enum Mock mock = Mock::None;

    MockMenuEntry() {}

    MockMenuEntry(const QString &text, enum Menu menu) : text(text), menu(menu), mock(Mock::None) {}

    MockMenuEntry(const QString &text, enum Mock mock) : text(text), menu(Menu::None), mock(mock) {}
};

static const QMap<QString, QVector<Mock>> pinpad_simulator_responses
{
    { "pos.connected", { Mock::PinpadDisplayItemSuccess } },
    { "pinpad.preparePaymentRequest", { Mock::PinpadCardInserted, Mock::PinpadCardInfo } },
    { "pinpad.authorizePaymentRequest", { Mock::PinpadPaymentAccepted, Mock::PinpadCardRemoved } },
    { "pinpad.finishPaymentRequest", { Mock::PinpadFinishPaymentRequest } }
};

/* Maps each `Menu` value to a list of `MockMenuEntry`s. This map represents a tree (excluding the
 * cycles introduced by the "back" option) where branches point to lists of mock menu entries, and
 * leaves point to a mock event.
 *
 * Each menu (besides the root/main menu) should contain a "back" option.
 */
static const QMap<Menu, QVector<MockMenuEntry>> mock_menu_entries
{
    { Menu::None, {} },
    {
        Menu::Main,
        {
            { "pinpad", Menu::Pinpad },
            { "printer", Menu::Printer },
            { "scanner", Menu::Scanner }
        }
    },
    {
        Menu::Pinpad,
        {
            { "card", Menu::PinpadCard },
            { "payment", Menu::PinpadPayment },
            { "other", Menu::PinpadOther },
            { "back", Menu::Main }
        }
    },
    {
        Menu::Printer,
        {
            { "connected", Mock::PrinterConnected },
            { "disconnected", Mock::PrinterDisconnected },
            { "drawer opened", Mock::PrinterDrawerOpened },
            { "drawer closed", Mock::PrinterDrawerClosed },
            { "back", Menu::Main }
        }
    },
    {
        Menu::Scanner,
        {
            { "read", Mock::ScannerRead },
            { "read PCI", Mock::ScannerReadPCI },
            { "back", Menu::Main }
        }
    },
    {
        Menu::PinpadCard,
        {
            { "card inserted", Mock::PinpadCardInserted },
            { "card read", Mock::PinpadCardInfo },
            { "card read failed", Mock::PinpadCardReadError },
            { "card declined", Mock::PinpadCardDeclined },
            { "card removed", Mock::PinpadCardRemoved },
            { "back", Menu::Pinpad }
        }
    },
    {
        Menu::PinpadPayment,
        {
            { "finish transaction", Mock::PinpadFinishPaymentRequest },
            { "payment accepted", Mock::PinpadPaymentAccepted },
            { "back", Menu::Pinpad }
        }
    },
    {
        Menu::PinpadOther,
        {
            { "customer info request succeeded", Mock::PinpadCustomerInfoRequestSucceeded },
            { "item displayed", Mock::PinpadDisplayItemSuccess },
            { "item display failed", Mock::PinpadDisplayItemFailure },
            { "back", Menu::Pinpad }
        }
    }
};

// The context with which the input handler should handle input.
// TODO: default-initialize values here
struct Context
{
    // general context
    bool connected;     // zbus connection status
    int size;           // last recorded size of event_history
    Mode mode;          // mode with which to process input

    // command mode context
    Menu menu;          // menu to display

    // peruse mode context
    int top;            // index in event_history of event at the top of history window
    int selection;      // index in event_history of selected event
};

// Stores the dimensions and position of an ncurses WINDOW object alongside said WINDOW object.
struct META_WINDOW
{
    WINDOW *window = nullptr;
    int rows;
    int columns;
    int y;
    int x;

    void regenerate(int rows, int columns, int y, int x)
    {
        this->rows = rows;
        this->columns = columns;
        this->y = y;
        this->x = x;

        regenerate();
    }

    void regenerate()
    {
        wresize(window, this->rows, this->columns);
        mvwin(window, this->y, this->x);
    }
};

/* The private implementation for `ZBusCli`. Contains logic for handling inbound/outbound zBus
 * events, user input, and the UI.
 */
class ZBusCliPrivate
{
public:
    QList<QPair<Origin, ZBusEvent>> event_history;  // list of all events to and from zBus
    QString current_request_id;                     // last requestId received from zBus event
    QString current_auth_attempt_id;                // last authAttemptId received from zBus event
    bool pinpad_simulator_enabled;                  // simulates affirmative responses from pinpad
    ZWebSocket client;                              // sender and receiver of zBus events

    FIELD *entry_fields[3] = {};
    FORM *entry_form = nullptr;

    META_WINDOW screen;
    META_WINDOW help;
    META_WINDOW status;
    META_WINDOW mock_menu;
    META_WINDOW entry;
    META_WINDOW sub_entry;
    META_WINDOW history;

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
        screen.window = stdscr;
        getmaxyx(screen.window, screen.rows, screen.columns);

        // create window to display keybinds
        help.rows = 2;
        help.columns = screen.columns;
        help.y = 0;
        help.x = screen.columns - help.columns;
        help.window = newwin(help.rows, help.columns, help.y, help.x);
        wmove(help.window, 0, 0);
        wprintw(help.window, help_text[Mode::Command].toUtf8());
        wrefresh(help.window);

        // create window to display connection status with zBus
        status.rows = 3;
        status.columns = screen.columns;
        status.y = help.y + help.rows;
        status.x = screen.columns - status.columns;
        status.window = newwin(status.rows, status.columns, status.y, status.x);

        // create field for input of event sender and event type
        QString event_label = "event ";
        int event_rows = 2;
        int event_columns = screen.columns - event_label.size();
        int event_y = 0;
        int event_x = screen.columns - event_columns;
        entry_fields[0] = new_field(event_rows - 1, event_columns, event_y, event_x, 0, 0);
        set_field_back(entry_fields[0], A_UNDERLINE);
        field_opts_off(entry_fields[0], O_AUTOSKIP);
        field_opts_off(entry_fields[0], O_STATIC);
        field_opts_off(entry_fields[0], O_BLANK);
        field_opts_off(entry_fields[0], O_WRAP);

        // create field for input of requestId
        QString request_id_label = "requestId ";
        int request_id_rows = 2;
        int request_id_columns = screen.columns - request_id_label.size();
        int request_id_y = event_y + event_rows;
        int request_id_x = screen.columns - request_id_columns;
        entry_fields[1] = new_field(request_id_rows - 1, request_id_columns, request_id_y, request_id_x, 0, 0);
        set_field_back(entry_fields[1], A_UNDERLINE);
        field_opts_off(entry_fields[1], O_AUTOSKIP);
        field_opts_off(entry_fields[1], O_STATIC);
        field_opts_off(entry_fields[1], O_BLANK);
        field_opts_off(entry_fields[1], O_WRAP);

        // create field for input of event data
        QString data_label = "data ";
        int data_rows = 6;
        int data_columns = screen.columns - data_label.size();
        int data_y = request_id_y + request_id_rows;
        int data_x = screen.columns - data_columns;
        entry_fields[2] = new_field(data_rows - 1, data_columns, data_y, data_x, 0, 0);
        set_field_back(entry_fields[2], A_UNDERLINE);
        field_opts_off(entry_fields[2], O_AUTOSKIP);
        field_opts_off(entry_fields[2], O_STATIC);
        field_opts_off(entry_fields[2], O_BLANK);
        field_opts_off(entry_fields[2], O_WRAP);

        // create window to contain event entry form
        entry.rows = event_rows + request_id_rows + data_rows;
        entry.columns = screen.columns;
        entry.y = status.y + status.rows;
        entry.x = screen.columns - entry.columns;
        entry.window = newwin(entry.rows, entry.columns, entry.y, entry.x);
        sub_entry.window = derwin(entry.window, entry.rows, entry.columns, 0, 0);

        // create event entry form to contain event entry fields
        entry_form = new_form(entry_fields);
        set_form_win(entry_form, entry.window);
        set_form_sub(entry_form, sub_entry.window);
        post_form(entry_form);

        // add labels for event entry fields
        wmove(entry.window, event_y, 0);
        wprintw(entry.window, event_label.toUtf8());
        wmove(entry.window, request_id_y, 0);
        wprintw(entry.window, request_id_label.toUtf8());
        wmove(entry.window, data_y, 0);
        wprintw(entry.window, data_label.toUtf8());
        wrefresh(entry.window);

        // create window to display mock menu entries
        mock_menu.rows = 7;
        mock_menu.columns = screen.columns;
        mock_menu.y = status.y + status.rows;
        mock_menu.x = screen.columns - mock_menu.columns;
        mock_menu.window = newwin(mock_menu.rows, mock_menu.columns, mock_menu.y, mock_menu.x);
        update_mock_menu(Menu::Main);

        // create window for event history, using the remaining rows in the screen
        history.rows = screen.rows - (mock_menu.y + mock_menu.rows);
        history.columns = screen.columns;
        history.y = screen.rows - history.rows;
        history.x = screen.columns - history.columns;
        history.window = newwin(history.rows, history.columns, history.y, history.x);
        wmove(history.window, 0, 0);
        wprintw(history.window, "Events broadcast by the zBus server will appear here.");
        wrefresh(history.window);
    }

    /* \brief Frees up memory occupied by the ncurses windows, forms, and fields, then ends curses
     *        mode, returning the terminal to its regular appearance and behavior.
     */
    ~ZBusCliPrivate()
    {
        delwin(help.window);
        delwin(status.window);
        delwin(sub_entry.window);
        delwin(entry.window);
        delwin(mock_menu.window);
        delwin(history.window);

        free_form(entry_form);
        free_field(entry_fields[0]);
        free_field(entry_fields[1]);
        free_field(entry_fields[2]);

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
        // TODO: address possibility of too-large events by always printing at least one event
        //       (display class can handle this)
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
        wclear(history.window);

        // get width and height of the terminal window
        int rows = history.rows;
        int columns = history.columns;

        // write events until running out of events or screen space
        int row = 0;
        for (int i = top; i >= 0; i--)
        {
            // determine the height (due to line-wrapping) of the next event to be written;
            // if the event would extend past the end of the history.window,
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
            wmove(history.window, row, 0);

            // if the event has been selected, make it bold
            if (i == selection)
            {
                wattron(history.window, A_BOLD);
            }
            wprintw(history.window, prefix.toUtf8());
            wprintw(history.window, json.toUtf8());
            wattroff(history.window, A_BOLD);

            // set row for next event immediately after current event
            row = row + height;
        }

        // update screen
        wrefresh(history.window);
    }

    /* \brief Displays the help text corresponding to the given mode.
     *
     * \param <mode> Mode for which the help text should be displayed.
     */
    void update_help_text(Mode mode)
    {
        // TODO: handle multiline help text
        wmove(help.window, 0, 0);
        wclear(help.window);
        wprintw(help.window, help_text[mode].toUtf8());
        wrefresh(help.window);
    }

    void update_status(bool connected, QString error)
    {
        wclear(status.window);

        status.rows = connected ? 2 : 3;
        status.y = help.y + help.rows;
        status.regenerate();

        wmove(status.window, 0, 0);

        // update the status message and error message
        if (connected)
        {
            wattron(status.window, COLOR_PAIR(GREEN_TEXT) | A_BOLD);
            wprintw(status.window, "status: connected to zBus");
            wattroff(status.window, COLOR_PAIR(GREEN_TEXT) | A_BOLD);
        }
        else
        {
            wattron(status.window, COLOR_PAIR(RED_TEXT) | A_BOLD);
            wprintw(status.window, "status: disconnected from zBus");
            wattroff(status.window, COLOR_PAIR(RED_TEXT) | A_BOLD);

            // display the error message from the websocket
            wmove(status.window, 1, 0);
            wattron(status.window, COLOR_PAIR(RED_TEXT) | A_BOLD);
            wprintw(status.window, "error: ");
            wprintw(status.window, error.toUtf8());
            wattroff(status.window, COLOR_PAIR(RED_TEXT) | A_BOLD);
        }

        wrefresh(status.window);
    }

    /* \brief Adjusts the height of the event history window, depending on the given mode.
     *
     * \param <mode> Mode for which the history window should be resized.
     */
    void resize_history_window(Mode mode)
    {
        switch(mode)
        {
            case Mode::Command:
                redrawwin(mock_menu.window);
                wrefresh(mock_menu.window);
                history.rows = screen.rows - (mock_menu.y + mock_menu.rows);
                break;
            case Mode::Send:
                redrawwin(entry.window);
                history.rows = screen.rows - (entry.y + entry.rows);
                break;
            default:
                history.rows = screen.rows - (status.y + status.rows);
                break;
        }

        history.y = screen.rows - history.rows;

        history.regenerate();
        wrefresh(history.window);
    }

    /* \brief Populates each of the event entry fields with the corresponding values from the given
     *        event
     *
     * \param <event> The zBus event to be inserted into the entry form.
     */
    void insert_event(ZBusEvent event)
    {
        set_field_buffer(entry_fields[0], 0, event.name().toUtf8());
        set_field_buffer(entry_fields[1], 0, event.requestId.toUtf8());
        set_field_buffer(entry_fields[2], 0, event.dataString().toUtf8());
    }

    /* \brief Generates a visual list from the menu entries associated with the given menu value,
     *        resizes the window to fit the content, then writes it to the mock menu window.
     *
     * \param <menu> The menu to be displayed.
     */
    void update_mock_menu(Menu menu)
    {
        QVector<MockMenuEntry> entries = mock_menu_entries[menu];

        wclear(mock_menu.window);
        mock_menu.rows = entries.size() + 1;
        mock_menu.y = status.y + status.rows;
        mock_menu.regenerate();
        for (int i = 0; i < entries.size(); i++)
        {
            wmove(mock_menu.window, i, 0);
            wprintw(mock_menu.window, QByteArray::number(i + 1));
            wprintw(mock_menu.window, ") ");
            wprintw(mock_menu.window, entries[i].text.toUtf8());
        }
        wrefresh(mock_menu.window);
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
 * \param <requestId> String containing the requestId of the event.
 *
 * \returns Number of bytes sent.
 */
qint64 ZBusCli::onEventSubmitted(const ZBusEvent &event)
{
    p->event_history.append({ Origin::Sent, event });
    return p->client.sendZBusEvent(event);
}

/* \brief Stores the given event in the event_history list, and captures the event's `requestId` and
 *        `authAttemptId`.
 *
 *        This is connected to ZWebSocket's zBusEventReceived signal in order to save all received
 *        events, and keep track of the current requestId and authAttemptId expected from mock
 *        pinpad events.
 *
 * \param <event> Event received from zBus.
 */
void ZBusCli::onZBusEventReceived(const ZBusEvent &event)
{
    p->event_history.append({Origin::Received, event});

    // if the received event contains a requestId,
    // update the stored requestId for mock events
    p->current_request_id = event.requestId.isEmpty() ? p->current_request_id
                                                      : event.requestId;

    // if the received event contains an authAttemptId,
    // update the stored authAttemptId for mock events
    const QString auth_attempt_id = event.data.toObject().value("authAttemptId").toString();
    p->current_auth_attempt_id = auth_attempt_id.isEmpty() ? p->current_auth_attempt_id
                                                           : auth_attempt_id;

    // if the pinpad simulator is enabled, attempt to respond to the event
    if (p->pinpad_simulator_enabled)
    {
        QVector<Mock> responses = pinpad_simulator_responses.value(event.name());
        foreach (Mock response, responses)
        {
            // POS needs about 5 seconds before it is able to receive responses to
            // pinpad.preparePaymentRequest
            int timeout = (response == Mock::PinpadCardInserted
                        || response == Mock::PinpadCardInfo) ? 5000 : 100;
            QTimer::singleShot(
                    timeout,
                    [response, this] { onEventSubmitted({ response,
                                                        p->current_request_id,
                                                        p->current_auth_attempt_id });});
        }
    }
}

/* \brief Starts an infinite loop that waits for input, processes pending Qt events, and updates the
 *        display. Forever.
 */
void ZBusCli::startEventLoop()
{
    Context current
    {
        /* connected */ true,
        /* size */ 0,
        /* mode */ Mode::Command,
        /* menu */ Menu::Main,
        /* top */ 0,
        /* selected */ -1
    };

    // capture input, process pending Qt events, then update the display
    while (true)
    {
        int input = wgetch(p->entry.window);

        // process input with current state, and capture next state for next input
        Context next;
        switch(current.mode)
        {
            case Mode::Command:
                next = handle_command_input(input, current);
                break;

            case Mode::Send:
                next = handle_send_input(input, current);
                break;

            case Mode::Peruse:
                next = handle_peruse_input(input, current);
                break;
        }

        // before updating the display, process pending Qt events
        QCoreApplication::processEvents();

        // tracks if there have been any window changes that need to be propogated to lower windows
        bool changes_above = false;

        // if the mode has changed, update the help text
        if (current.mode != next.mode)
        {
            p->update_help_text(next.mode);
            changes_above = true;
        }

        // if anything above has changed or the connection status has changed, update the status
        if (changes_above || current.connected != p->client.isValid())
        {
            next.connected = p->client.isValid();
            p->update_status(next.connected, p->client.errorString());
            changes_above = true;
        }

        // if anything above has changed or the menu selection has changed, update the menu
        if ((changes_above && next.mode == Mode::Command) || current.menu != next.menu)
        {
            p->update_mock_menu(next.menu);
            changes_above = true;
        }

        // if anything above has changed, update the history window
        if (changes_above)
        {
            p->resize_history_window(next.mode);
        }

        // the event selection has changed, if any new events have been received, or the mode has
        // changed, update the event history
        next.size = p->event_history.size();
        if (next.selection != current.selection ||
            next.size > current.size ||
            current.mode != next.mode)
        {
            next.top = p->find_top_for_selection(current.top, next.selection);
            p->update_history_window(next.top, next.selection);
        }

        // if entry fields are visible, return cursor to last position in current field
        // otherwise, hide the cursor
        // TODO: put the cursor somewhere in other modes?
        if (next.mode == Mode::Send)
        {
            curs_set(1);
            pos_form_cursor(p->entry_form);
        }
        else
        {
            curs_set(0);
        }

        // update state for processing next input
        current = next;
    }
}

/* \brief Handles input received while the client is in Command mode.
 *
 * \param <input> Character code of keypress from keyboard.
 * \param <context> The context at the time of input.
 *
 * \returns The context that subsequent input should be processed with.
 */
Context ZBusCli::handle_command_input(int input, Context context)
{
    switch(input)
    {
        // On any number, try to select a corresponding entry from the current menu
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            {
                int index = QByteArray(1, input).toInt() - 1;
                MockMenuEntry entry = mock_menu_entries[context.menu].value(index); //TODO: replace operator[] with .at() and .value() everywhere

                // if the selection refers to another menu, display the next menu
                if (entry.menu != Menu::None)
                {
                    context.menu = entry.menu;
                    return context;
                }

                // if the selection refers to a mock event, insert the mock event
                if (entry.mock != Mock::None)
                {
                    p->insert_event({ entry.mock,
                            p->current_request_id,
                            p->current_auth_attempt_id });
                    context.mode = Mode::Send;
                    context.menu = Menu::Main;
                    return context;
                }

                // if the selection refers neither a menu or a mock event, the selection is invalid;
                // do nothing
                return context;
            }

        // On "s", switch to send mode and reset the mock menu window
        case 's':
            context.mode = Mode::Send;
            context.menu = Menu::Main;
            return context;

        // On "p", switch to peruse mode and reset the mock menu window
        case 'p':
            context.mode = Mode::Peruse;
            context.menu = Menu::Main;
            return context;

        // On "m", toggle the pinpad simulator:
        case 'm':
            p->pinpad_simulator_enabled = !p->pinpad_simulator_enabled; // TODO: move into context?
            return context;

        // On all other input, do nothing
        default:
            return context;
    }
}

/* \brief Handles input received while the client is in Send mode.
 *
 * \param <input> Character code of keypress from keyboard.
 * \param <context> The context at the time of input.
 *
 * \returns The context that subsequent input should be processed with.
 */
Context ZBusCli::handle_send_input(int input, Context context)
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
            emit eventSubmitted({ field_buffer(p->entry_fields[0], 0),
                    field_buffer(p->entry_fields[2], 0),
                    field_buffer(p->entry_fields[1], 0) });
            break;

        // on Backspace, backspace
        case 127:
        case KEY_BACKSPACE:
            form_driver(p->entry_form, REQ_DEL_PREV);
            break;

            // on Escape, determine whether or not this is the beginning of an "Escape Sequence"
        case '\033':
            input = wgetch(p->entry.window);

            // it's not an "Escape Sequence"; on Escape, switch to Command Mode and process
            // subsequent input
            if (input != '[')
            {
                context.mode = Mode::Command;
                return handle_command_input(input, context);
            }

            // it's an "Escape Sequence"; on Arrow Key, move cursor (arrow keys are received in the
            // form "Esc + [ + A")
            switch (wgetch(p->entry.window))
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

    return context;
}

/* \brief Handles input received while the client is in Peruse mode.
 *
 * \param <input> Character code of keypress from keyboard.
 * \param <context> The context at the time of input.
 *
 * \returns The context that subsequent input should be processed with.
 */
Context ZBusCli::handle_peruse_input(int input, Context context)
{
    switch(input)
    {
        // on Escape, determine whether or not this is the beginning of an "Escape Sequence"
        case '\033':
            input = wgetch(p->entry.window);

            // it's not an "Escape Sequence"; on Escape, switch to Command Mode, clear the
            // selection, and process subsequent input
            if (input != '[')
            {
                context.mode = Mode::Command;
                context.selection = -1;
                return handle_command_input(input, context);
        }


            // if the event history is empty, ignore selections
            if (p->event_history.empty())
            {
                return context;
            }

            // it's an "Escape Sequence"; on Arrow Key, select another event (arrow keys are
            // received in the form "Esc + [ + A")
            int latest_event = p->event_history.size() - 1;
            switch (wgetch(p->entry.window))
            {
                // Up
                case 'A':
                    // wrap around to latest event if no event or the last event is selected
                    context.selection = (context.selection == -1 //TODO: do I need both of these conditions?
                                      || context.selection == latest_event) ? 0
                                                                            : context.selection + 1;
                    return context;
                // Down
                case 'B':
                    // wrap around to latest event if first event is selected
                    context.selection = context.selection > 0 ? context.selection - 1 : latest_event;
                    return context;
            }

            // on any other escape sequence, do nothing
            // TODO: replace with default case, remove mixing of breaks and returns
            break;
    }

    // on any other input, do nothing
    return context;
}
