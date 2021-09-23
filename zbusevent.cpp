#include "zbusevent.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QVariant>

/* \brief Constructs a ZBusEvent from a json-formatted string. If a field can not be extracted from
 *        the given string for any reason (e.g. invalid json, missing field), it will be left blank.
 *
 * \param <json> JSON-formatted string.
 */
ZBusEvent::ZBusEvent(const QString &json)
{
  QJsonObject jsonObject = QJsonDocument::fromJson(json.toUtf8()).object();

  QStringList senderAndType = jsonObject.value("event").toString().split(".");
  this->sender = senderAndType.value(0);
  this->type = senderAndType.value(1);
  this->data = jsonObject.value("data");
}

/* \brief Constructs a ZBusEvent from the given event string and data string.
 *
 * \param <event> String containing the event sender and type in the format "sender.type".
 *                Corresponds to the "event" field in the JSON representation of a zBus event.
 * \param <data> JSON-formatted string containing the event data.
 */
ZBusEvent::ZBusEvent(const QString &event, const QString &data)
{
  QStringList senderAndType = event.split(".");
  this->sender = senderAndType.value(0);
  this->type = senderAndType.value(1);

  // store data as appropriate type of JSON value
  // if it is not an object or array, treat it like a string
  bool ok = false;
  QJsonDocument dataDoc = QJsonDocument::fromJson(data.toUtf8());
  if (!dataDoc.isNull())
  {
      this->data = QJsonValue::fromVariant(dataDoc.toVariant());
  }
  else if (data.toDouble(&ok) || ok)
  {
      this->data = QJsonValue(data.toDouble());
  }
  else if (data.toInt(&ok) || ok)
  {
      this->data = QJsonValue(data.toInt());
  }
  else if (data == "true" || data == "false")
  {
      this->data = QJsonValue(data == "true");
  }
  else
  {
      this->data = data;
  }
}

/* \brief Creates a JSON-formatted string from the ZBusEvent.
 *
 * \returns JSON-formatted string generated from the ZBusEvent.
 */
QString ZBusEvent::toJson() const
{
  QJsonObject json{{"event", sender + (type.isEmpty() ? "" : ".") + type},
                   {"data", data}};

  return QJsonDocument(json).toJson(QJsonDocument::Compact);
}
