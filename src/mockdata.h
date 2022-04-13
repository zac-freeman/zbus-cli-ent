#ifndef MOCKDATA_H
#define MOCKDATA_H

#include <QJsonObject>
#include <QString>

// Mock event data used to generate mocked events.

static const QJsonObject MOCK_CARD_INFO
{
    {
        "cardInfo",
        QJsonObject{
            { "accountNumber", "374245XXXXX1337" },
            { "aid", "A000000025010801" },
            { "amount", "319.2" },
            { "appName", "AMERICAN EXPRESS" },
            { "approvalMethod", "AUTOMATIC" },
            { "approvalNumber", "123456" },
            { "arqc", "" },
            { "cardProvider", "AMEX" },
            { "entryMethod", "CHIP" },
            { "expirationDate", "0321" },
            { "pinVerified", "PIN Blocked" },
            { "ps2000", " 500               =    = 5533         N" },
            { "sequenceNumber", "00" },
            { "tc", "3BA276CB9E0F174E" }
        }
    }
};

static const QString MOCK_CUSTOMER_INFO{ "{receiptPreference: 'PAPER'}" };

static const QJsonObject MOCK_PARTIAL_APPROVAL
{
    { "authorizedAmount", "610" },
    { "requestedAmount", "960" }
};

static const QJsonObject MOCK_DRAWER_OPEN_STATE
{
    { "tillIsConnected", true },
    { "tillIsOpen", true },
    { "outOfPaper", false },
    { "feedError", false },
    { "ribbonCoverOpen", false },
    { "documentStationSelected", false },
    { "frontDocumentSensor", false },
    { "topDocumentSensor", false },
    { "isPrintingReceipt", false },
    { "isReadingCheck", false }
};

static const QJsonObject MOCK_PRINTER_CONNECTED_STATE
{
    { "tillIsConnected", true },
    { "tillIsOpen", false },
    { "outOfPaper", false },
    { "feedError", false },
    { "ribbonCoverOpen", false },
    { "documentStationSelected", false },
    { "frontDocumentSensor", false },
    { "topDocumentSensor", false },
    { "isPrintingReceipt", false },
    { "isReadingCheck", false }
};

static const QJsonObject MOCK_PRINTER_DISCONNECTED_STATE
{
    { "tillIsConnected", false },
    { "tillIsOpen", false },
    { "outOfPaper", false },
    { "feedError", false },
    { "ribbonCoverOpen", false },
    { "documentStationSelected", false },
    { "frontDocumentSensor", false },
    { "topDocumentSensor", false },
    { "isPrintingReceipt", false },
    { "isReadingCheck", false }
};

static const QString MOCK_PCI{ "900100<STORE_NUMBER><KPCOUNTER_ID>" };

#endif
