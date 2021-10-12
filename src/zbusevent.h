#ifndef ZBUS_EVENT_H
#define ZBUS_EVENT_H

#include <QJsonValue>
#include <QString>

/* Representation of the (extended) JSON format for events received and broadcasted by zBus.  The
 * fields `requestId` and `authAttemptId` are used exclusively for communication with the pinpad to
 * verify that that a zBus event corresponds to another zBus event, and that a zBus event
 * corresponds to a specific authorization/payment, respectively. The shape of a zBus event is:
 *  ```
 *  {
 *      "event": "<sender>.<type>",
 *      "data": <json array or object>,
 *      "requestId": "<requestId>",
 *      "authAttemptId": "<authAttemptId"
 *  }
 *  ```
 */
class ZBusEvent
{
public:
  ZBusEvent(const QString &json = QString());
  ZBusEvent(const QString &event,
            const QString &data,
            const QString &requestId = QString(),
            const QString &authAttemptId = QString());

  QString toJson() const;

  QString sender;
  QString type;
  QJsonValue data;
  QString requestId;
  QString authAttemptId;
};

#endif
