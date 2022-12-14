#ifndef ZBUS_CLI_H
#define ZBUS_CLI_H

#include <QObject>

class Context;
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
    void handle_input(Context current);
    Context handle_command_input(int input, Context context);
    Context handle_peruse_input(int input, Context context);
    Context handle_send_input(int input, Context context);

signals:
    void event_submitted(const ZBusEvent &event);
    void quit();

private slots:
    void retry_connection();
    qint64 handle_outbound_event(const ZBusEvent &event);
    void handle_inbound_event(const ZBusEvent &event);

private:
    ZBusCliPrivate *p;
};

#endif
