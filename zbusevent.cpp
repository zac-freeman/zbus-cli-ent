#include "zbusevent.h"

#include <QJsonDocument>

ZBusEvent::ZBusEvent(const QString &event)
{
  QJsonObject json = QJsonDocument::fromJson(event.toUtf8()).object();
  this->sender = json.value("sender").toString();
  this->event = json.value("event").toString();
  this->data = json.value("data").toObject();
}

QString ZBusEvent::toJson() const
{
  return "{\"sender\":\"" + sender + "\", "
         "\"event\":\"" + event + "\", "
         "\"data\":" + QJsonDocument(data).toJson() + "}";
}
