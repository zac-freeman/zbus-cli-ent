#ifndef ZBUS_EVENT_H
#define ZBUS_EVENT_H

#include <QJsonValue>
#include <QString>

class ZBusEvent
{
public:
  ZBusEvent(const QString &event);

  QString toJson() const;

  QString sender;
  QString type;
  QJsonValue data;
};

#endif
