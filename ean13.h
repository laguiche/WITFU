#ifndef EAN13_H
#define EAN13_H

#include <QWidget>
#include <QStringList>
#include <QImage>
#include <QtPrintSupport/QPrinter>

/********************************************************************************/
class EAN13 : public QWidget
{

    Q_OBJECT

public:
    EAN13(QWidget *parent = 0);
    void makePattern(const QString &code) ;
    void draw(const QRectF &rect, QPainter &painter);
    void draw(QPainter *painter) ;
    void setCode(QString &code);
    void updateScreenCode() ;

    static QString makeChecksum(QString twelveDigits);
private:
    QStringList LCode;
    QStringList GCode;
    QStringList RCode;
    QStringList Parity;
    QString     quietZone;
    QString     leadTail;
    QString     separator;
    QString     checksum ;
    QString     barcode;
    QString     parity ;
    QString     pattern ;
    QString     convertToDigitPattern(QString number, QStringList &pattern);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) ;

public slots:
    void save(QString fic);

};

/********************************************************************************/

#endif // EAN13_H
