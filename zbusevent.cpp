#include "zbusevent.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

ZBusEvent::ZBusEvent(const QString &event)
{
  QJsonObject json = QJsonDocument::fromJson(event.toUtf8()).object();

  QStringList senderAndType = json.value("event").toString().split(".");
  this->sender = senderAndType.value(0);
  this->type = senderAndType.value(1);
  this->data = json.value("data");
}

QString ZBusEvent::toJson() const
{
  QJsonObject json{{"event", sender + (type.isEmpty() ? "" : ".") + type},
                   {"data", data}};

  return QJsonDocument(json).toJson(QJsonDocument::Compact);
}
