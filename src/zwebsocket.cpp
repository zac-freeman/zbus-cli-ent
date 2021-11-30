#include "zwebsocket.h"

#include "zbusevent.h"

#include <QDebug>
#include <QList>
#include <QQueue>
#include <QString>

class ZWebSocketPrivate {
public:
    QQueue<ZBusEvent> eventQueue;
};

/* \brief Constructs ZWebSocket, and prepares to send any messages that were queued up before the
 *        connection to zBus was established.
 *
 * \param <origin> Value to be used in the origin header of the request. Typically, the origin
 *                 header is used by browsers to enable Cross-Origin Resource Sharing (CORS).
 * \param <version> Version of the WebSocket protocol used.
 * \param <parent> Parent of this instantiation of ZWebSocket.
 */
ZWebSocket::ZWebSocket(const QString &origin, QWebSocketProtocol::Version version, QObject *parent)
    : QWebSocket(origin, version, parent)
{
    p = new ZWebSocketPrivate();

    connect(this, &ZWebSocket::connected, this, &ZWebSocket::processEventQueue);
    connect(this, &ZWebSocket::textMessageReceived, this, &ZWebSocket::zBusEventReceived);
}

/* \brief Cleans up objects created on the heap.
*/
ZWebSocket::~ZWebSocket()
{
    delete p;
}

/* \brief Sends events that were queued up while ZWebSocket was not connected to zBus and emits a
 *        signal when finished.
 */
void ZWebSocket::processEventQueue()
{
    while (!p->eventQueue.isEmpty())
    {
        sendZBusEvent(p->eventQueue.dequeue());
    }

    emit processedEventQueue();
}

/* \brief If ZWebSocket is connected to zBus, sends the given event to zBus. Otherwise, the event is
 *        queued up to be sent when the connection is established.
 *
 * \param <event> Event to be sent to zBus.
 *
 * \returns Number of bytes transmitted.
 */
qint64 ZWebSocket::sendZBusEvent(const ZBusEvent &event)
{
    if (isValid())
    {
        return sendTextMessage(event.toJson());
    }
    else
    {
        p->eventQueue.enqueue(event);
        return 0;
    }
}

/* \brief Constructs, then sends, or queues, multiple events.
 *
 *        A QString can be implicitly converted to a ZBusEvent, but a list of QStrings can not be
 *        implicitly converted to a list of ZBusEvents, so the list of QStrings is explicitly
 *        converted here.
 *
 * \param <events> List of JSON-formatted strings to be converted then sent to zBus.
 *
 * \returns Number of bytes transmitted.
 */
qint64 ZWebSocket::sendZBusEvents(const QStringList &events)
{
    QList<ZBusEvent> zBusEvents;
    QString event;
    foreach(event, events)
    {
        zBusEvents.append(event);
    }

    return sendZBusEvents(zBusEvents);
}

/* \brief Sends, or queues, multiple events.
 *
 * \param <events> List of events to be sent to zBus.
 *
 * \returns Number of bytes transmitted.
 */
qint64 ZWebSocket::sendZBusEvents(const QList<ZBusEvent> &events)
{
    qint64 bytesSent = 0;
    ZBusEvent event;
    foreach(event, events)
    {
        bytesSent += sendZBusEvent(event);
    }

    return bytesSent;
}
