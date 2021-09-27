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
  }

  void fromInvalidJson()
  {
    ZBusEvent event(invalidJson);
    QCOMPARE(event.sender, QString(""));
    QCOMPARE(event.type, QString(""));
    QCOMPARE(event.data.toString(), QString(""));
  }

  void fromEventAndData()
  {
    ZBusEvent event("test-sender.test-type", "test-data");
    QCOMPARE(event.sender, QString("test-sender"));
    QCOMPARE(event.type, QString("test-type"));
    QCOMPARE(event.data.toString(), QString("test-data"));
  }

  void toJson()
  {
    ZBusEvent event;
    event.data = "test-data";
    event.sender = "test-sender";
    event.type = "test-type";
    QCOMPARE(event.toJson(), validJson);
  }

private:
  const QString validJson{"{\"data\":\"test-data\",\"event\":\"test-sender.test-type\"}"};
  const QString invalidJson{"{\"data\":\"test-data\",\"event\":\""};
};

QTEST_GUILESS_MAIN(ZBusEventTest);
#include "zbusevent.test.moc"
