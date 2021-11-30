#ifndef ZBUS_CLI_H
#define ZBUS_CLI_H

#include <QObject>

class State;
class ZBusCliPrivate;
class ZBusEvent;

/* The Menu determines what options are displayed to the user, and what the client does with
 * (menu-related) input received from the user. A valid input will result in either a new menu being
 * displayed, or the event entry fields being populated with a mock event.
 *
 * Each Menu value has a corresponding mock menu entry.
 */
enum class Menu { None, Main, Pinpad, Printer, Scanner, PinpadCard, PinpadPayment, PinpadOther };

/* The mode determines what the client does with input received from the user.
 *
 * Command - The default mode. Switches to one of the other modes, based on the input received.
 * Send - Takes input for the purpose of navigating and editing the event type and data fields, and
 *        sending the constructed events.
 * Peruse - Takes input for the purpose of navigating the event history.
 */
enum class Mode { Command, Send, Peruse };

/* Bridge between the ZWebSocket sending and receiving events, and the ncurses event loop displaying
 * the events and accepting input from the user.
 */
class ZBusCli : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ZBusCli)

public:
    ZBusCli(QObject *parent = nullptr);
    ~ZBusCli();

    void exec(const QUrl &zBusUrl);
    void startEventLoop();
    State handle_command_input(int input, enum Menu current_menu, int selection);
    State handle_peruse_input(int input, enum Menu current_menu, int selection);
    State handle_send_input(int input, enum Menu current_menu, int selection);

signals:
    void eventSubmitted(const QString &event, const QString &data, const QString &requestId);

private slots:
    void onDisconnected();
    qint64 onEventSubmitted(const QString &event, const QString &data, const QString &requestId);
    void onZBusEventReceived(const ZBusEvent &event);

private:
    ZBusCliPrivate *p;
};

#endif
