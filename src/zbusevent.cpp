#include "zbusevent.h"

#include "mockdata.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <QVariant>

static const QMap<Mock, ZBusEvent> mockEvent
{
    // nuffin
    { Mock::None, {} },

    // pinpad events
    { Mock::PinpadCardDeclined, { "pinpad.paymentError", MOCK_CARD_INFO } },
    { Mock::PinpadCardInfo, { "pinpad.cardInfo", MOCK_CARD_INFO } },
    { Mock::PinpadCardInserted, { "pinpad.cardInserted" } },
    { Mock::PinpadCardReadError, { "pinpad.cardReadError", MOCK_CARD_INFO } },
    { Mock::PinpadCardRemoved, { "pinpad.cardRemoved" } },
    {
        Mock::PinpadCustomerInfoRequestSucceeded,
        { "pinpad.customerInfoRequestSucceeded", MOCK_CUSTOMER_INFO }
    },
    { Mock::PinpadDisplayItemFailure, { "pinpad.displayItemFailure" } },
    { Mock::PinpadDisplayItemSuccess, { "pinpad.displayItemSuccess" } },
    { Mock::PinpadFinishPaymentRequest, { "pinpad.finishPaymentRequest" } },
    { Mock::PinpadPaymentAccepted, { "pinpad.paymentAccepted", MOCK_CARD_INFO } },

    // printer events
    { Mock::PrinterConnected, { "printer.stateUpdate", MOCK_PRINTER_CONNECTED_STATE } },
    { Mock::PrinterDisconnected, { "printer.stateUpdate", MOCK_PRINTER_DISCONNECTED_STATE } },
    { Mock::PrinterDrawerClosed, { "printer.stateUpdate", MOCK_PRINTER_CONNECTED_STATE } },
    { Mock::PrinterDrawerOpened, { "printer.stateUpdate", MOCK_DRAWER_OPEN_STATE } },

    // scanner events
    { Mock::ScannerRead, { "scanner.read" } },
    { Mock::ScannerReadPCI, { "scanner.read", MOCK_PCI } }
};

/* \brief Constructs a ZBusEvent from a json object. If a field can not be extracted from
 *        the given object for any reason (e.g. invalid json, missing field), it will be left blank.
 *
 * \param <json> JSON-formatted string.
 */
ZBusEvent::ZBusEvent(const QJsonObject &json)
{
  QStringList senderAndType = json.value("event").toString().split(".");
  this->sender = senderAndType.value(0);
  this->type = senderAndType.value(1);
  this->data = json.value("data");
  this->requestId = json.value("requestId").toString();
}

/* \brief Constructs a ZBusEvent from the given event string and data string.
 *
 * \param <event> String containing the event sender and type in the format "sender.type".
 *                Corresponds to the "event" field in the JSON representation of a zBus event.
 * \param <data> JSON value containing the event data.
 * \param <requestId> String ID of the pinpad request this event corresponds to.
 */
ZBusEvent::ZBusEvent(const QString &event,
                     const QJsonValue &data,
                     const QString &requestId)
{
  QStringList senderAndType = event.split(".");
  this->sender = senderAndType.value(0);
  this->type = senderAndType.value(1);
  this->data = data;
  this->requestId = requestId;
}

ZBusEvent::ZBusEvent(enum Mock name,
                     const QString &requestId,
                     const QString &authAttemptId)
{
    this->sender = mockEvent[name].sender;
    this->type = mockEvent[name].type;
    this->data = mockEvent[name].data;
    this->requestId = requestId;

    if (this->data.isObject())
    {
        QJsonObject dataObject = this->data.toObject();
        dataObject.insert("authAttemptId", authAttemptId);
        this->data = dataObject;
    }
}

/* \brief Creates a JSON-formatted string from the ZBusEvent.
 *
 * \returns JSON-formatted string generated from the ZBusEvent.
 */
QString ZBusEvent::toJson() const
{
  QJsonObject json{{"event", sender + (type.isEmpty() ? "" : ".") + type},
                   {"data", data},
                   {"requestId", requestId}};

  return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QString ZBusEvent::name() const
{
    return sender + "." + type;
}

QString ZBusEvent::dataString() const
{
  if (data.isObject())
  {
    return QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact);
  }

  if (data.isArray())
  {
    return QJsonDocument(data.toArray()).toJson(QJsonDocument::Compact);
  }

   return data.toString();
}
