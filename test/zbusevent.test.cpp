#include "../src/zbusevent.h"

#include <QObject>
#include <QtTest/QtTest>

class ZBusEventTest : public QObject
{
    Q_OBJECT

private slots:
    void fromJson()
    {
        const QJsonObject json = QJsonDocument::fromJson(validJson.toUtf8()).object();
        const ZBusEvent event(json);
        QCOMPARE(event.domain, { "test-domain" });
        QCOMPARE(event.type, { "test-type" });
        QCOMPARE(event.data.toString(), { "test-data" });
        QCOMPARE(event.requestId, { "test-request-id" });
    }

    // The pinpad manager events names take the form "pinpad.manager.<type>". This unit test ensures
    // that ZBusEvent captures the entirety of events broken up over more than one ".".
    void fromPinpadManagerParody()
    {
        const QJsonObject json =
            QJsonDocument::fromJson(pinpadManagerEventParody.toUtf8()).object();
        const ZBusEvent event(json);
        QCOMPARE(event.domain, { "pinpad.manager.instance.application.object" });
        QCOMPARE(event.type, { "updateFirmware" });
        QCOMPARE(event.data.toString(), { "test-data" });
        QCOMPARE(event.requestId, { "test-request-id" });
    }

    void fromEmptyJson()
    {
        const QJsonObject json;
        const ZBusEvent event(json);
        QCOMPARE(event.domain, { "" });
        QCOMPARE(event.type, { "" });
        QCOMPARE(event.data.toString(), { "" });
        QCOMPARE(event.requestId, { "" });
    }

    void fromEventAndData()
    {
        const ZBusEvent event("test-domain.test-type",
                "test-data",
                "test-request-id");
        QCOMPARE(event.domain, { "test-domain" });
        QCOMPARE(event.type, { "test-type" });
        QCOMPARE(event.data.toString(), { "test-data" });
        QCOMPARE(event.requestId, { "test-request-id" });
    }

    void toJson()
    {
        ZBusEvent event;
        event.domain = "test-domain";
        event.type = "test-type";
        event.data = "test-data";
        event.requestId = "test-request-id";
        QCOMPARE(event.toJson(), validJson);
    }

    void name()
    {
        ZBusEvent event;
        event.domain = "test-domain";
        event.type = "test-type";
        QCOMPARE(event.name(), { "test-domain.test-type" });

        event.domain = "the-lonely-loner";
        event.type = "";
        QCOMPARE(event.name(), { "the-lonely-loner" });

        event.domain = "";
        event.type = "seems-to-free-his-mind-at-night";
        QCOMPARE(event.name(), { "seems-to-free-his-mind-at-night" });

        event.domain = "";
        event.type = "";
        QCOMPARE(event.name(), { "" });
    }

    void dataString()
    {
        ZBusEvent event;
        event.data = "test-data";
        QCOMPARE(event.dataString(), { "test-data" });

        event.data =
            QJsonObject
            {
                { "key" , "value" },
                { "nest",
                    QJsonObject
                    {
                        { "test", "c'est la vie" }
                    }
                }
            };
        QCOMPARE(event.dataString(), { "{\"key\":\"value\",\"nest\":{\"test\":\"c'est la vie\"}}" });

        event.data = QJsonArray{ QJsonObject{ {"a", 1} }, 1, "this is an abomination" };
        QCOMPARE(event.dataString(), { "[{\"a\":1},1,\"this is an abomination\"]" });
    }

    private:
    const QString validJson{"{"
        "\"data\":\"test-data\","
        "\"event\":\"test-domain.test-type\","
        "\"requestId\":\"test-request-id\""
        "}"};

    const QString pinpadManagerEventParody{"{"
        "\"data\":\"test-data\","
        "\"event\":\"pinpad.manager.instance.application.object.updateFirmware\","
        "\"requestId\":\"test-request-id\""
        "}"};
};

//TODO: ensure ZBusEvent constructor correctly infers type of QJsonValue given (and doesn't just
//      assume everything is a string)

QTEST_GUILESS_MAIN(ZBusEventTest);
#include "zbusevent.test.moc"
