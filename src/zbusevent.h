#ifndef ZBUS_EVENT_H
#define ZBUS_EVENT_H

#include <QJsonValue>
#include <QJsonObject>
#include <QString>

/* Mock event types. For each value, there is a corresponding event that mocks some event from a
 * hardware device.
 */
enum class Mock
{
    // nuffin
    None,

    // pinpad events
    PinpadCardDeclined,
    PinpadCardInfo,
    PinpadCardInserted,
    PinpadCardReadError,
    PinpadCardRemoved,
    PinpadCustomerInfoRequestSucceeded,
    PinpadDisplayItemFailure,
    PinpadDisplayItemSuccess,
    PinpadFinishPaymentRequest,
    PinpadPaymentAccepted,

    // printer events
    PrinterConnected,
    PrinterDisconnected,
    PrinterDrawerClosed,
    PrinterDrawerOpened,

    // scanner events
    ScannerRead,
    ScannerReadPCI
};

/* Representation of the (extended) JSON format for events received and broadcasted by zBus. The
 * event data may contain an `authAttemptId`. The fields `requestId` and `authAttemptId` are used
 * exclusively for communication with the pinpad to verify that that a zBus event corresponds to
 * another zBus event, and that a zBus event corresponds to a specific authorization/payment,
 * respectively. The shape of a zBus event is:
 *  ```
 *  {
 *      "event": "<sender>.<type>",
 *      "data": <json array or object>,
 *      "requestId": "<requestId>"
 *  }
 *  ```
 */
class ZBusEvent
{
public:
  ZBusEvent(const QJsonObject &json = QJsonObject());
  ZBusEvent(const QString &event,
            const QJsonValue &data = QJsonValue(),
            const QString &requestId = QString());
  ZBusEvent(enum Mock name,
            const QString &requestId = QString(),
            const QString &authAttemptId = QString());

  QString toJson() const;
  QString name() const;
  QString dataString() const;

  QString sender;
  QString type;
  QJsonValue data;
  QString requestId;
};

#endif
