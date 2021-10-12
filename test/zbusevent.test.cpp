#include "../src/zbusevent.h"

#include <QObject>
#include <QtTest/QtTest>

class ZBusEventTest : public QObject
{
  Q_OBJECT

private slots:
  void fromValidJson()
  {
    ZBusEvent event(validJson);
    QCOMPARE(event.sender, QString("test-sender"));
    QCOMPARE(event.type, QString("test-type"));
    QCOMPARE(event.data.toString(), QString("test-data"));
    QCOMPARE(event.requestId, QString("test-request-id"));
    QCOMPARE(event.authAttemptId, QString("test-auth-id"));
  }

  void fromInvalidJson()
  {
    ZBusEvent event(invalidJson);
    QCOMPARE(event.sender, QString(""));
    QCOMPARE(event.type, QString(""));
    QCOMPARE(event.data.toString(), QString(""));
    QCOMPARE(event.requestId, QString(""));
    QCOMPARE(event.authAttemptId, QString(""));
  }

  void fromEventAndData()
  {
    ZBusEvent event("test-sender.test-type",
                    "test-data",
                    "test-request-id",
                    "test-auth-id");
    QCOMPARE(event.sender, QString("test-sender"));
    QCOMPARE(event.type, QString("test-type"));
    QCOMPARE(event.data.toString(), QString("test-data"));
    QCOMPARE(event.requestId, QString("test-request-id"));
    QCOMPARE(event.authAttemptId, QString("test-auth-id"));
  }

  void toJson()
  {
    ZBusEvent event;
    event.sender = "test-sender";
    event.type = "test-type";
    event.data = "test-data";
    event.requestId = "test-request-id";
    event.authAttemptId = "test-auth-id";
    QCOMPARE(event.toJson(), validJson);
  }

private:
  const QString validJson{"{"
      "\"authAttemptId\":\"test-auth-id\","
      "\"data\":\"test-data\","
      "\"event\":\"test-sender.test-type\","
      "\"requestId\":\"test-request-id\""
  "}"};
  const QString invalidJson{"{\"data\":\"test-data\",\"event\":\""};
};

QTEST_GUILESS_MAIN(ZBusEventTest);
#include "zbusevent.test.moc"
