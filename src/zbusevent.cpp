#include "zbusevent.h"

#include "mockdata.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <QVariant>

struct DomainAndType
{
    QString domain;
    QString type;
};

/* \brief Extracts the domain and type from a given event into a struct.
 *
 *        The event name for a zBus event takes the form of "<domain>.<type>". However, in some
 *        cases, a domain can contain subdomains, giving event names like
 *        "pinpad.manager.updateFirmware".
 */
static DomainAndType extractDomainAndType(const QString &event)
{
    QStringList domainAndType = event.trimmed().split(".");
    QString domain = domainAndType.mid(0, domainAndType.size() - 1).join('.');
    QString type = domainAndType.value(domainAndType.size() - 1);
    return {domain, type};
}

/* Mocked hardware events, corresponding to specific hardware events/behaviors named in the `Mock`
 * enum.
 */
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
    { Mock::PinpadPartialApproval, { "pinpad.partialApprovalAuthorized", MOCK_PARTIAL_APPROVAL } },
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
 * \param <json> JSON object expected to contain `event`, `data`, and `requestId`.
 */
ZBusEvent::ZBusEvent(const QJsonObject &json)
{
    DomainAndType domainAndType = extractDomainAndType(json.value("event").toString());
    this->domain = domainAndType.domain;
    this->type = domainAndType.type;
    this->data = json.value("data");
    this->requestId = json.value("requestId").toString();
}

/* \brief Constructs a ZBusEvent from the given event string and data string.
 *
 * \param <event> String containing the event domain and type in the format "domain.type".
 *                Corresponds to the "event" field in the JSON representation of a zBus event.
 * \param <data> JSON value containing the event data.
 * \param <requestId> String ID of the pinpad request this event corresponds to.
 */
ZBusEvent::ZBusEvent(const QString &event,
                     const QJsonValue &data,
                     const QString &requestId)
{
    DomainAndType domainAndType = extractDomainAndType(event);
    this->domain = domainAndType.domain;
    this->type = domainAndType.type;
    this->requestId = requestId.trimmed();

    // if the data is a string, attempt to convert it into an object or array;
    // if the data is not an object or array, it is assumed that it is a string
    if (data.isString())
    {
        QString dataString = data.toString().trimmed();
        QJsonDocument dataDoc = QJsonDocument::fromJson(dataString.toUtf8());
        this->data = dataDoc.isNull() ? dataString : QJsonValue::fromVariant(dataDoc.toVariant());
    }
    else
    {
        this->data = data;
    }
}

/* \brief Constructs a mock ZBusEvent for a given Mock value, from the mock event map. Optionally
 *        adds the provided `requestId` and `authAttemptId` to the mocked event to associate said
 *        event with a real transaction.
 *
 * \param <Mock> Event type to determine the type, domain, and data.
 * \param <requestId> String ID of the pinpad request this event corresponds to.
 * \param <authAttemptId> String ID of the pinpad payment authorization this event corresponds to.
 */
ZBusEvent::ZBusEvent(enum Mock name,
                     const QString &requestId,
                     const QString &authAttemptId)
{
    this->domain = mockEvent[name].domain;
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
    QJsonObject json{{"event", name()},
                     {"data", data},
                     {"requestId", requestId}};

    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

/* \brief Assembles the event name from the domain and type.
 *
 * \returns The event name of the ZBusEvent.
 */
QString ZBusEvent::name() const
{
    return domain + ((domain.isEmpty() || type.isEmpty()) ? "" : ".") + type;
}

/* \brief Creates a JSON-formatted string from the event data.
 *
 *        Since the event data can be any valid JSON, this function must check the type of the event
 *        data before attempting to convert it to a string. Simply calling `data.toString()` would
 *        result in an empty string if `data` is an object or array.
 *
 * \returns The event name of the ZBusEvent.
 */
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
