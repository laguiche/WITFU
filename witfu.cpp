#include "witfu.h"
#include "ui_witfu.h"
#include <QTime>
#include <QDate>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QTableWidget>
#include <QTableWidgetItem>

WITFU::WITFU(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::WITFU)
{
    ui->setupUi(this);

    discoveryAgent = new QBluetoothDeviceDiscoveryAgent();
    discoveryService=new QBluetoothServiceDiscoveryAgent();
    localDevice=new QBluetoothLocalDevice();
    localDevice->setHostMode(QBluetoothLocalDevice::HostDiscoverable);


    connect(discoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
    this, SLOT(deviceDiscovered(QBluetoothDeviceInfo)));

    connect(ui->pB_scan_device, SIGNAL(clicked()), this, SLOT(startScan()));

    connect(discoveryAgent, SIGNAL(finished()), this, SLOT(scanFinished()));

    connect(localDevice,SIGNAL(pairingFinished(QBluetoothAddress,QBluetoothLocalDevice::Pairing)),
    this,SLOT(pairingFinished(QBluetoothAddress,QBluetoothLocalDevice::Pairing)));

    QStringList header;
    header << "nom" << "ref" << "numero";

    ui->conf->setColumnCount(3);
    ui->conf->setHorizontalHeaderLabels(header);


    ean13_barcode = ui->ean13_view;

    connect(ui->pB_generate_EAN13,SIGNAL(clicked(bool)),this,SLOT(generateEAN13(bool)));
    connect(ui->pB_saveImage,SIGNAL(clicked(bool)),this,SLOT(saveImage(bool)));

    //init database to save time of running loops in sport mode
    m_statusDB=NOK;
    m_statusDB=setDataBase();

    //remplissage des noms d'urls
    if(m_statusDB==OK)
    {
        QSqlQuery query;
        QString query_str="SELECT id, name FROM URLs ORDER BY name DESC";
        if(!query.exec(query_str))
          qDebug() << "[QURL] :\tSQL ERROR: " << query.lastError().text();

        while(query.next())
        {
            ui->cB_URLtype->addItem(query.value(1).toString(),QVariant(query.value(0)));
        }
    }
    else
        ui->cB_URLtype->addItem("No Name",QVariant("0000"));

    ui->cB_URLtype->model()->sort(0);
    ui->cB_URLtype->setCurrentIndex(0);

    //autocompletion des références
    QStringList RefList;
    if(m_statusDB==OK)
    {
        QSqlQuery query;
        QString query_str="SELECT ref FROM Refs ORDER BY ref DESC";
        if(!query.exec(query_str))
          qDebug() << "[QURL] :\tSQL ERROR: " << query.lastError().text();

        while(query.next())
        {
           RefList << query.value(0).toString();
        }

        if (!RefList.isEmpty())
        {
            QCompleter *completer = new QCompleter(RefList, this);
            completer->setCaseSensitivity(Qt::CaseInsensitive);
            ui->lE_URLref->setCompleter(completer);
        }
    }


/*
    //exemple pour plus tard si on veut remplacer la base de données par un fichier csv
    QFile f("url_types.csv");
    if (f.open(QIODevice::ReadOnly))
    {
        QTextStream in(&f);
        while (!in.atEnd())
        {
            QStringList wordList;
            QString line = f.readLine();
            wordList.append(line.split(';'));
            if(wordList.size()>=2)
            {
                QString key(wordList.at(0));
                QString value(wordList.at(1));
                URLtypes[key]=value.trimmed();
            }
        }

        f.close();

        if(URLtypes.isEmpty())
            URLtypes["No Name"]="0000";
    }
    else
        URLtypes["No Name"]="0000";

    //qDebug() << URLtypes;
    ui->cB_URLtype->addItems(URLtypes.keys());
*/
    //gestion du debug
    m_debugGen=ui->debugGen->isChecked();
    m_debugScan=ui->debugScan->isChecked();
    connect(ui->debugGen,SIGNAL(toggled(bool)),this,SLOT(setDebug()));
    connect(ui->debugScan,SIGNAL(toggled(bool)),this,SLOT(setDebug()));

    connect(ui->pB_sauvConf,SIGNAL(clicked(bool)),this,SLOT(saveConf(bool)));

    discoveryAgent->start();
}

WITFU::~WITFU()
{
    delete discoveryAgent;
    delete discoveryService;
    if (socket != NULL && socket->isOpen())
    {
        socket->close();
        delete socket;
        socket = NULL;
    }
    delete ui;
}

void WITFU::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    if(m_debugScan)
        qDebug() << Q_FUNC_INFO;

    if(device.name().contains("CT90"))
    {
        if(m_debugScan)
            qDebug() <<  "[QURL] :\t" << "Scannette de code barre trouvée:" << device.name() << '(' << device.address().toString() << ')';
        //Pour la classe QBluetoothLocalDevice (issue de la doc qt):
        //On iOS and Windows, this class cannot be used because the platform
        //does not expose any data or API which may provide information on
        //the local Bluetooth device.
        #if defined(Q_OS_WIN)
            connecter(device.address());
        #elif defined(Q_OS_LINUX)
            localDevice->requestPairing(device.address(), QBluetoothLocalDevice::Paired);
        #endif

    }
}

void WITFU::startScan()
{
    discoveryAgent->start();
    ui->pB_scan_device->setEnabled(false);
}

void WITFU::scanFinished()
{
    if(m_debugScan)
        qDebug() << Q_FUNC_INFO;
    ui->pB_scan_device->setEnabled(true);
}


void WITFU::pairingFinished(const QBluetoothAddress &address, QBluetoothLocalDevice::Pairing pairing)
{
    if(m_debugScan)
        qDebug() << Q_FUNC_INFO;
    if(m_debugScan)
        qDebug() <<  "[QURL] :\t" <<address.toString() <<" : scannette appairée";
    connecter(address);
}

void WITFU::connecter(const QBluetoothAddress &address)
{
    if(m_debugScan)
        qDebug() << Q_FUNC_INFO;
    socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol);
    connect(socket, SIGNAL(connected()), this, SLOT(socketConnected()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()));

    QBluetoothUuid uuid = QBluetoothUuid(QBluetoothUuid::SerialPort);
    socket->connectToService(address, uuid);
    socket->open(QIODevice::ReadWrite);
}

void WITFU::deconnecter()
{
    if (socket->isOpen())
    {
        socket->close();
    }
}

bool WITFU::estConnecte()
{
    return socket->isOpen();
}

void WITFU::socketConnected()
{
    if(m_debugScan)
        qDebug() << Q_FUNC_INFO;
    QString message = QString::fromUtf8("Périphérique connecté ") + socket->peerName() + " [" + socket->peerAddress().toString() + "]";
    if(m_debugScan)
    qDebug() <<  "[QURL] :\t" << message;
}

void WITFU::socketDisconnected()
{
    if(m_debugScan)
        qDebug() << Q_FUNC_INFO;
    QString message = QString::fromUtf8("Périphérique déconnecté ");
    if(m_debugScan)
    qDebug() << "[QURL] :\t" << message;
}

void WITFU::socketReadyRead()
{
    if(m_debugScan)
        qDebug() << Q_FUNC_INFO;

    while (socket->bytesAvailable())
    {
        donnees += socket->readAll();
        //QThread::usleep(150000); // cf. timeout
    }

    if(m_debugScan)
    qDebug() << "[QURL] :\tDonnées reçues : " << QString(donnees) << " soit " << donnees.size() << " caracteres.";


    if(donnees.endsWith("\r"))
        decode();
    else
        if(m_debugScan)
            qDebug() << "[QURL] :\tDonnées reçues invalides.";

    donnees.clear();
}

void WITFU::generateEAN13(bool state)
{
    QString code2gen;

    bool okForGenerate=true;

    QString errorMessage="Abandon création code barre:";

    //Ajout du type d'url
    QString type(ui->cB_URLtype->currentData().toString().left(TYPE_DIGITS));

    int iType=type.toInt();
    iType=1000-iType;

    type.setNum(iType).truncate(TYPE_DIGITS+1);

    int type_size=type.size();

    if(type_size<TYPE_DIGITS)
        for(int i=0; i <(TYPE_DIGITS-type_size);i++)
            type.prepend("0");

    code2gen.append(type);

    //Ajout de la référence de l'URL
    QString ref;
    QString refWanted=ui->lE_URLref->text();

    if(!refWanted.isEmpty())
    {
        if(!sql_isRefExist(refWanted))
        {
            QMessageBox msgBox;
            msgBox.setText("La référence "+refWanted+" n'existe pas.");
            msgBox.setInformativeText("Voulez vous la créer?");
            msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Cancel);
            int ret = msgBox.exec();

            //on ajoute la ref à la base de données
            //en cas de succès on génère le code barre
            if(ret==QMessageBox::Ok)
            {
                if(sql_setRef(ui->lE_URLref->text()))
                {
                    okForGenerate=true;
                }
                else
                {
                    errorMessage.append(" pb base de onnées /");
                    okForGenerate=false;
                }
            }

            //manifestement la ref n'est pas la bonne on abandonne la génération de code barre
            if(ret==QMessageBox::Cancel)
            {
                errorMessage.append(" ref inconnue /");
                okForGenerate=false;
            }
        }

        ref.setNum(sql_getIdRef(ui->lE_URLref->text())).truncate(REF_DIGITS+1);

        int ref_size=ref.size();

        if(ref_size<REF_DIGITS)
            for(int i=0; i <(REF_DIGITS-ref_size);i++)
                ref.prepend("0");

        code2gen.append(ref);
    }
    else
    {
        errorMessage.append(" ref vide /");
        okForGenerate=false;
    }

    //Ajout du numéro de série
    QString serial(ui->lE_URLserial->text().left(SERIAL_DIGITS));

    if(!serial.isEmpty())
    {
        int serial_size=serial.size();

        if(serial_size<SERIAL_DIGITS)
            for(int i=0; i <(SERIAL_DIGITS-serial_size);i++)
                serial.prepend("0");

        code2gen.append(serial);
    }
    else
    {
        errorMessage.append(" num série vide /");
        okForGenerate=false;
    }

    if(okForGenerate)
    {
        QString sChecksum=EAN13::makeChecksum(code2gen);

        ui->pB_saveImage->setEnabled(true);

        if(m_debugGen)
        qDebug() << "[QURL] :\tSet code " << code2gen<<sChecksum;

        ui->label_code->setText(code2gen+sChecksum); //affichage du code barre en chiffre
        QString label;
        label=ui->cB_URLtype->currentText()+
                " "+ui->lE_URLref->text()+
                " "+ui->lE_URLserial->text();
        ui->label_etiquette->setText(label); //affichage du code barre en lettres
        ean13_barcode->setCode(code2gen); //fabrication du code barre
        ean13_barcode->updateScreenCode(); //affichage du code barre
        statusBar()->showMessage("Création code barre réussie : "+label+" => "+code2gen+sChecksum,2000);
    }
    else
    {
        statusBar()->showMessage(errorMessage,2000);
        if(m_debugGen)
            qDebug() << "[QURL] :\tAbandon de la génération du code barre :" << errorMessage;
    }
}

unsigned char WITFU::setDataBase()
{
    unsigned char status=OK;
    const QString DRIVER("QSQLITE");
    if(QSqlDatabase::isDriverAvailable(DRIVER))
        m_db = QSqlDatabase::addDatabase(DRIVER);
    else
        status=NOK;

    //creation de la database
    m_db.setDatabaseName("url_types.db");

    if(!m_db.open())
    {
        qDebug() << "[QURL] :\tErreur d'ouverture de la base de données: " << m_db.lastError();
        status=NOK;
    }

    return status;
}

void WITFU::saveImage(bool state)
{
    QString ficName;

    ficName=stringSimplify(ui->cB_URLtype->currentText())+
            "_"+stringSimplify(ui->lE_URLref->text())+
    "_"+stringSimplify(ui->lE_URLserial->text())+".png";
    //qDebug() << ficName;
    ean13_barcode->save(ficName);
}

QString WITFU::sql_getType(QString fragBarcode)
{
    int iType=fragBarcode.toInt();
    QString result;
    result.setNum(1000-iType);

    if(m_statusDB==OK)
    {
        QSqlQuery query1;
        QString query1_str="SELECT name FROM URLs WHERE id=%1";

        if(!query1.exec(query1_str.arg(1000-iType)))
          qDebug() << "[QURL] :\tSQL ERROR: " << query1.lastError().text();
        else {

            while (query1.next()) {
                    result = query1.value(0).toString();
                    //qDebug() << result;
                }
        }
    }

    return result;
}

QString WITFU::sql_getRef(QString fragBarcode)
{
    QString result=fragBarcode;
    int iRef=fragBarcode.toInt();

    if(m_statusDB==OK)
    {
        QSqlQuery query1;
        QString query1_str="SELECT ref FROM Refs WHERE id=%1";

        if(!query1.exec(query1_str.arg(iRef)))
          qDebug() << "[QURL] :\tSQL ERROR: " << query1.lastError().text();
        else {

            while (query1.next()) {
                    result = query1.value(0).toString();
                    //qDebug() << result;
                }
        }
    }

    return result;
}

int WITFU::sql_getIdRef(QString ref)
{
    QString result="1";

    if(m_statusDB==OK)
    {
        QSqlQuery query1;
        //QString query1_str="SELECT id FROM Refs WHERE ref=%1";
        QString query1_str="SELECT id FROM Refs WHERE ref=\""+ref+"\"";
        //if(!query1.exec(query1_str.arg(ref)))
        if(!query1.exec(query1_str))
        {
            qDebug() << "[QURL] :\tSQL ERROR with query: " << query1_str;
          qDebug() << "[QURL] :\tSQL ERROR: " << query1.lastError().text();
        }
        else {

            while (query1.next()) {
                    result = query1.value(0).toString();
                    //qDebug() << result;
                }
        }
    }

    return result.toInt();
}

void WITFU::setDebug(void)
{
    m_debugGen=ui->debugGen->isChecked();
    m_debugScan=ui->debugScan->isChecked();
}

void WITFU::decode(void)
{
    if(donnees.size()==14) //barcode EAN-13 avec retour chariot
    {
        if(m_debugScan)
        qDebug() << "[QURL] :\tIl s'agit d'un code barre EAN13";
        QString sBarcode(donnees);
        sBarcode.chop(2);//enleve le retour chariot \r et le chiffre de controle du code barre

        QString sType=sBarcode.left(TYPE_DIGITS);
        QString sSerial=sBarcode.right(SERIAL_DIGITS);
        sBarcode.chop(SERIAL_DIGITS);
        QString sRef=sBarcode.right(REF_DIGITS);

        if(m_debugScan)
        qDebug() << "[QURL] :\tCode URL: " << sType << "\tCode Ref: " << sRef <<"\tNuméro de série: " << sSerial;

        QTableWidgetItem *newItem_name = new QTableWidgetItem(sql_getType(sType));
        QTableWidgetItem *newItem_ref = new QTableWidgetItem(sql_getRef(sRef));
        QTableWidgetItem *newItem_serial = new QTableWidgetItem(sSerial);


        int lastRow=ui->conf->rowCount();
        ui->conf->insertRow(lastRow);

        if(m_debugScan)
        qDebug() << "[QURL] :\tPret pour enregistrement ligne " << lastRow;

        ui->conf->setItem(lastRow,0,newItem_name);
        ui->conf->setItem(lastRow,1,newItem_ref);
        ui->conf->setItem(lastRow,2,newItem_serial);
    }
    else
    {
        if(m_debugScan)
        qDebug() << "[QURL] :\tCe n'est pas un code barre EAN13";
    }
}

QString WITFU::stringSimplify(QString complexString)
{
    QString result="NoName";
    if(!complexString.isEmpty())
    {
        result=complexString.replace("/","_").replace(" ","_").replace("\\","_").replace("(","_").replace(")","_");
    }

    return result;
}

void WITFU::saveConf(bool state)
{
    QString ficName("savedConf_");
    QString temps2=QTime::currentTime().toString("hh_mm");
    QString temps1 = QDate::currentDate().toString("yyyy_MM_dd_at_");
    ficName.append(temps1);
    ficName.append(temps2);
    ficName.append(".csv");

    if(!ui->lE_conf->text().isEmpty())
    {
        ficName.prepend("_");
        ficName.prepend(stringSimplify(ui->lE_conf->text()));
    }


    if(ui->conf->rowCount()>=1)
    {
        QFile file_csv(ficName);
        if(file_csv.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))
        {
            QTextStream stream(&file_csv);

            QString strWholeFile;

            int lastRow=ui->conf->rowCount();

            for(int i=0; i<lastRow;i++)
            {
                strWholeFile += ui->conf->item(i,0)->text()+";"+
                    ui->conf->item(i,1)->text()+";"+
                    ui->conf->item(i,2)->text()+";\n";
            }


            if(m_debugScan)
            qDebug() << "[QURL] :\tPret pour enregistrement conf " << strWholeFile;

            stream << strWholeFile;
            file_csv.close();
        }
        else
        {
            file_csv.close();
        }
    }
}

bool WITFU::sql_isRefExist(QString ref)
{
    bool isRefExist=false;

    if(m_statusDB==OK)
    {
        QSqlQuery query1;
        QString query1_str="SELECT id FROM Refs WHERE ref='%1'";
        if(!query1.exec(query1_str.arg(ref)))
        {
            qDebug() << "[QURL] :\tSQL ERROR with query: " << query1_str.arg(ref);
          qDebug() << "[QURL] :\tSQL ERROR: " << query1.lastError().text();
        }
        else {

            while (query1.next()) {
                    isRefExist=true;
                }
        }
    }

    return isRefExist;
}

bool WITFU::sql_setRef(QString ref)
{
    bool isSuccess=true;

    if(m_statusDB==OK)
    {
        QSqlQuery query1;
        //QString query1_str="SELECT id FROM Refs WHERE ref=\""+simpleRef+"\"";
        QString query1_str="INSERT INTO Refs(ref) VALUES('%1')";
        if(!query1.exec(query1_str.arg(ref)))
        {
            qDebug() << "[QURL] :\tSQL ERROR with query: " << query1_str.arg(ref);
          qDebug() << "[QURL] :\tSQL ERROR: " << query1.lastError().text();
          isSuccess=false;
        }
    }
    else
        isSuccess=false;

    return isSuccess;
}
