/******************************************************************************\
 * QSSTV - AVIF Image Codec
 *
 * Description:
 *   AVIF image encode/decode using libavif.
 *   Mirrors the jp2IO interface for drop-in replacement.
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
\******************************************************************************/

#ifndef AVIFIO_H
#define AVIFIO_H

#include <QObject>
#include <QString>
#include <QImage>

class avifCodec : public QObject
{
    Q_OBJECT
public:
    avifCodec();
    bool check(QString fileName);
    QImage decode(QString fileName);
    QByteArray encode(QImage qimage, QImage &newImage, int &fileSize,
                      int compressionRatio = 0);
    void setParams(QImage *im, QString filename, bool tFromCache)
    {
        threadImage = im;
        threadFilename = filename;
        fromCache = tFromCache;
    }

public slots:
    void slotStart();

signals:
    void done(bool, bool);

private:
    QImage *threadImage;
    QString threadFilename;
    bool fromCache;
};

#endif /* AVIFIO_H */
