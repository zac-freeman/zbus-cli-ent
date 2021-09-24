#ifndef ZBUS_EVENT_H
#define ZBUS_EVENT_H

#include <QJsonValue>
#include <QString>

/* Representation of the JSON format for events received and broadcasted by zBus. The shape of a
 * zBus event is:
 *  ```
 *  {
 *      "event": "<sender>.<type>",
 *      "data": <data>
 *  }
 *  ```
 */
class ZBusEvent
{
public:
  ZBusEvent(const QString &json = QString());
  ZBusEvent(const QString &event, const QString &data);

  QString toJson() const;

  QString sender;
  QString type;
  QJsonValue data;
};

#endif
