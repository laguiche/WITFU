#include <QPainter>
#include <QDebug>

#include "ean13.h"

EAN13::EAN13(QWidget *parent)
 : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowSystemMenuHint)
{
        // Set up constant data

LCode << "0001101" << "0011001" << "0010011" <<"0111101" << "0100011"
        << "0110001" << "0101111" << "0111011" << "0110111" << "0001011";

GCode << "0100111" << "0110011" << "0011011" << "0100001" << "0011101"
        << "0111001" << "0000101" << "0010001" << "0001001" << "0010111";

RCode << "1110010" << "1100110" << "1101100" << "1000010" << "1011100"
        << "1001110" << "1010000" << "1000100" << "1001000" << "1110100";

Parity << "000000" << "001011" << "001101" << "001110" << "010011"
         << "011001" << "011100" << "010101" << "010110" << "011010";

quietZone = "000000000";
leadTail = "101";
separator = "01010";
}
/*!
 * \brief EAN13::makePattern
 * Les codes EAN 13 sont composés de 13 chiffres.
 *
 * La séquence des barres est alors :
 *
 * une zone de garde normale
 * le 2e chiffre sous la forme d’un élément A
 * le 3e chiffre sous la forme d’un élément A ou d'un élément B
 * le 4e chiffre sous la forme d’un élément A ou d'un élément B
 * le 5e chiffre sous la forme d’un élément A ou d'un élément B
 * le 6e chiffre sous la forme d’un élément A ou d'un élément B
 * le 7e chiffre sous la forme d’un élément A ou d'un élément B
 * une zone de garde centrale
 * le 8e chiffre sous la forme d’un élément C
 * le 9e chiffre sous la forme d’un élément C
 * le 10e chiffre sous la forme d’un élément C
 * le 11e chiffre sous la forme d’un élément C
 * le 12e chiffre sous la forme d’un élément C
 * le 13e chiffre sous la forme d’un élément C (chiffre contrôle)
 * une zone de garde normale
 * \param code
 */
void EAN13::makePattern(const QString &code)
{
    //!on se limite à 12 caractères (le dernier étant la clé de contrôle)
    if(!code.isEmpty())
        barcode = code.left(12);
    if(barcode.isEmpty())
        barcode="123456789012";

    //!Le dernier chiffre d'un code EAN 13 est toujours une clé de contrôle.
    checksum=makeChecksum(barcode);
    barcode += checksum;

    //qDebug() << "[EAN13] : " <<"code to generate: "<<barcode;

    //!construction du code EAN13 en binaire
    pattern = quietZone + leadTail ;

    /*!
     * La particularité des codes EAN 13 est que leur premier chiffre n'est pas codé sous la forme d'un élément EAN,
     * mais par la séquence d'enchaînement des types d'éléments des 6 chiffres qui le suivent.
     * Les lecteurs de codes barres (qui savent reconnaître si un élément est de type A ou B) déduisent donc
     * la valeur de ce 1er chiffre à partir du motif constitué par les types d'éléments du 2e chiffre au 7e chiffre.
    */
    parity = Parity[barcode.left(1).toInt()] ;
    //qDebug() << "[EAN13] :\tParité du premier chiffre: "<<parity;
    for(int i = 1; i <= 6; i++)
    {
        if(parity.mid(i - 1, 1).toInt())
            pattern.append(convertToDigitPattern(barcode.mid(i, 1), GCode));
        else
            pattern.append(convertToDigitPattern(barcode.mid(i, 1), LCode));
    }

    pattern += separator + convertToDigitPattern(barcode.mid(7,6), RCode) + leadTail + quietZone ;

    //qDebug() << "[EAN13] :\tPattern à générer: \n[EAN13] :\t"<< pattern;
}

void EAN13::draw(const QRectF &rect, QPainter &painter)
{
    int i ;

    painter.save();
    painter.setViewport(int(rect.x()), int(rect.y()), int(rect.width()), int(rect.height()));
    painter.setWindow(0, 0, int(rect.width()), int(rect.height()));
    painter.fillRect(painter.window(), Qt::white);	        // set background

    qreal width = rect.width();
    qreal height = rect.height();
    qreal barWidth = width / 113;

    painter.setRenderHint(QPainter::Antialiasing, true);
    QBrush brush;
    brush.setStyle(Qt::SolidPattern);
    brush.setColor(Qt::black);
    painter.setBrush(brush);

    painter.save();
    painter.setPen(Qt::NoPen);

    for(i = 0; i < pattern.length(); i++)
    {
        if(pattern.mid(i, 1) == "1")
        {
            painter.drawRect(QRectF(0, 0, barWidth, height));
        }
        painter.translate(barWidth, 0);
    }
    painter.restore();
}


QString EAN13::convertToDigitPattern(QString number, QStringList &pattern)
{
    /**
    Les éléments EAN se caractérisent par une succession de quatre barres
    (deux barres claires qui alternent avec deux barres sombres)
    dont la somme des largeurs vaut toujours 7 modules.
    Il y a donc au total 7 barres élémentaires dans un élément.
    Chacune de ces barres est elle-même constituée de la juxtaposition
    de 1 à 4 barres élémentaires de même couleur.

    Chaque élément peut être représenté en binaire par une suite de 7 bits.
    */
    QString digitPattern = "" ;

    for(int i = 0; i < number.length(); i++)
    {
        int index = number.mid(i, 1).toInt();
        digitPattern.append(pattern[index]);
    }

    return(digitPattern) ;
}

void EAN13::resizeEvent(QResizeEvent * /* event */){}

void EAN13::paintEvent(QPaintEvent *event)
{
    event = event;		// evade compiler warning

    if(!barcode.isEmpty())
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        draw(&painter);
    }
}


void EAN13::draw(QPainter *painter)
{
    this->makePattern("");
    this->draw(QRectF(0, 0, width(), height()), *painter) ;
}

void EAN13::setCode(QString &code)
{
    barcode= code.left(13) ;
}

void EAN13::updateScreenCode()
{
    update() ;
}

void EAN13::save(QString fic)
{
    QImage image(800,600,QImage::Format_RGB32);
    QPainter painter ;
    painter.begin(&image);

    makePattern("");
    draw(QRectF(0, 0, 800, 600), painter) ;

    painter.end();
    //fic="toto.png";
    if (image.save(fic))
        qDebug() << "[EAN13] :\tbarcode saved to "<< fic;
    else
        qDebug() << "[EAN13] :\tbarcode not saved";
}

/*!
 * \brief EAN13::makeChecksum
 *  Le dernier chiffre d'un code EAN 13 est toujours une clé de contrôle (check digit).
 * Le principe est le même qu'une formule de Luhn, excepté que les rangs pairs sont multipliés par trois et non par deux.
 * Elle est calculée à partir des douze premiers chiffres3selon l'algorithme suivant :
 * - Calculer trois fois la somme des chiffres de rang pair (en partant du second) de gauche à droite,
 * - calculer les somme des chiffres de rang impair (en partant du premier) de gauche à droite,
 * - totaliser ces deux sommes partielles,
 * - prendre le chiffre des unités de ce total, R,
 * - clé = reste de la division par 10 de 10- R.
 * \param twelveDigits
 * \return checksum as string
 */
QString EAN13::makeChecksum(QString twelveDigits)
{
    int i, sum, digit ;
    QString sChecksum;
    int iChecksum;

    sum = digit = 0 ;
    sChecksum="0";
    iChecksum=0;

    for(i = 0 ; i < twelveDigits.length() ; i++)
    {
        digit = twelveDigits.mid(i, 1).toInt() ;
        if(i % 2 == 0)
        sum += digit ;			// odd
        else
        sum += digit * 3 ;		        // even
    }
    iChecksum=(10 -(sum % 10)) % 10;
    sChecksum.setNum(iChecksum) ;

    return sChecksum;
}

