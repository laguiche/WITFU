#ifndef WITFU_H
#define WITFU_H

#include <QMainWindow>
#include <qbluetoothaddress.h>
#include <qbluetoothdevicediscoveryagent.h>
#include <qbluetoothservicediscoveryagent.h>
#include <QBluetoothSocket>
#include <qbluetoothlocaldevice.h>
#include <QThread>
#include <QPainter>
#include <QStringList>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <QTime>
#include <QTimer>
#include <QDate>
#include <QCompleter>
#include <QMessageBox>

#include "ean13.h"

#define OK 1
#define NOK 0

#define TYPE_DIGITS 3
#define REF_DIGITS 4
#define SERIAL_DIGITS 5

namespace Ui {
class WITFU;
}

class WITFU : public QMainWindow
{
    Q_OBJECT

public:
    explicit WITFU(QWidget *parent = 0);
    ~WITFU();

private:
    Ui::WITFU *ui;
    QBluetoothDeviceDiscoveryAgent *discoveryAgent;
    QBluetoothLocalDevice *localDevice;
    QBluetoothServiceDiscoveryAgent *discoveryService;
    QBluetoothSocket * socket;
    QByteArray donnees;
    EAN13   *ean13_barcode ;
    QHash<QString, QString> URLtypes;
    QSqlDatabase m_db;
    unsigned char m_statusDB;

    bool m_debugGen;
    bool m_debugScan;


    void connecter(const QBluetoothAddress &address);
    void deconnecter();
    bool estConnecte();

    unsigned char setDataBase();
    QString sql_getRef(QString fragBarcode);
    QString sql_getType(QString fragBarcode);
    int sql_getIdRef(QString ref);
    void decode();
    QString stringSimplify(QString complexString);
    bool sql_isRefExist(QString ref);
    bool sql_setRef(QString ref);
private slots:
    void deviceDiscovered(const QBluetoothDeviceInfo &device);
    void startScan(void);
    void scanFinished(void);
    void pairingFinished(const QBluetoothAddress &address, QBluetoothLocalDevice::Pairing pairing);
    void socketConnected();
    void socketDisconnected();
    void socketReadyRead();
    void generateEAN13(bool state);
    void saveImage(bool state);
    void setDebug();
    void saveConf(bool state);
};

#endif // WITFU_H
