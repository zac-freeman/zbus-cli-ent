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
    QCOMPARE(event.sender, QString("test-sender"));
    QCOMPARE(event.type, QString("test-type"));
    QCOMPARE(event.data.toString(), QString("test-data"));
    QCOMPARE(event.requestId, QString("test-request-id"));
  }

  void fromEmptyJson()
  {
    const QJsonObject json;
    const ZBusEvent event(json);
    QCOMPARE(event.sender, QString(""));
    QCOMPARE(event.type, QString(""));
    QCOMPARE(event.data.toString(), QString(""));
    QCOMPARE(event.requestId, QString(""));
  }

  void fromEventAndData()
  {
    const ZBusEvent event("test-sender.test-type",
                          "test-data",
                          "test-request-id");
    QCOMPARE(event.sender, QString("test-sender"));
    QCOMPARE(event.type, QString("test-type"));
    QCOMPARE(event.data.toString(), QString("test-data"));
    QCOMPARE(event.requestId, QString("test-request-id"));
  }

  void toJson()
  {
    ZBusEvent event;
    event.sender = "test-sender";
    event.type = "test-type";
    event.data = "test-data";
    event.requestId = "test-request-id";
    QCOMPARE(event.toJson(), validJson);
  }

private:
  const QString validJson{"{"
      "\"data\":\"test-data\","
      "\"event\":\"test-sender.test-type\","
      "\"requestId\":\"test-request-id\""
  "}"};
};

//TODO: ensure ZBusEvent constructor correctly infers type of QJsonValue given (and doesn't just
//      assume everything is a string)

QTEST_GUILESS_MAIN(ZBusEventTest);
#include "zbusevent.test.moc"
