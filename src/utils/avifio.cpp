/******************************************************************************\
 * QSSTV - AVIF Image Codec
 *
 * Description:
 *   AVIF image encode/decode using libavif.
 *   Mirrors the jp2IO class interface.
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
\******************************************************************************/

#include "avifio.h"
#include "appglobal.h"
#include "loggingparams.h"

#include <avif/avif.h>

#include <QFile>
#include <QDir>
#include <QDebug>

avifCodec::avifCodec()
    : threadImage(NULL), fromCache(false)
{
}

bool avifCodec::check(QString fileName)
{
    char data[12];
    QFile fi(fileName);
    if (!fi.open(QIODevice::ReadOnly)) return false;
    qint64 size = fi.read(data, 12);
    fi.close();
    if (size < 12) return false;

    /* AVIF files are ISOBMFF containers starting with an ftyp box.
       Structure: [4 bytes box size][4 bytes "ftyp"][4 bytes brand]
       Brand is "avif" or "mif1" or "avis" */
    if (memcmp(data + 4, "ftyp", 4) != 0) return false;
    if (memcmp(data + 8, "avif", 4) == 0) return true;
    if (memcmp(data + 8, "mif1", 4) == 0) return true;
    if (memcmp(data + 8, "avis", 4) == 0) return true;
    return false;
}

void avifCodec::slotStart()
{
    bool result = check(threadFilename);
    if (result)
    {
        *threadImage = decode(threadFilename).copy();
    }
    emit done(result, fromCache);
}

QImage avifCodec::decode(QString fileName)
{
    QImage qimage;

    /* Read file into memory */
    QFile fi(fileName);
    if (!fi.open(QIODevice::ReadOnly))
    {
        addToLog(QString("AVIF: cannot open file %1").arg(fileName), LOGIMAG);
        return qimage;
    }
    QByteArray fileData = fi.readAll();
    fi.close();

    if (fileData.isEmpty())
    {
        addToLog("AVIF: empty file", LOGIMAG);
        return qimage;
    }

    /* Create decoder */
    avifDecoder *decoder = avifDecoderCreate();
    if (!decoder)
    {
        addToLog("AVIF: failed to create decoder", LOGIMAG);
        return qimage;
    }

    avifResult result = avifDecoderSetIOMemory(decoder,
        (const uint8_t *)fileData.constData(), fileData.size());
    if (result != AVIF_RESULT_OK)
    {
        addToLog(QString("AVIF: setIOMemory failed: %1").arg(avifResultToString(result)), LOGIMAG);
        avifDecoderDestroy(decoder);
        return qimage;
    }

    result = avifDecoderParse(decoder);
    if (result != AVIF_RESULT_OK)
    {
        addToLog(QString("AVIF: parse failed: %1").arg(avifResultToString(result)), LOGIMAG);
        avifDecoderDestroy(decoder);
        return qimage;
    }

    result = avifDecoderNextImage(decoder);
    if (result != AVIF_RESULT_OK)
    {
        addToLog(QString("AVIF: nextImage failed: %1").arg(avifResultToString(result)), LOGIMAG);
        avifDecoderDestroy(decoder);
        return qimage;
    }

    avifImage *avImage = decoder->image;
    int width = avImage->width;
    int height = avImage->height;

    /* Convert to RGB */
    avifRGBImage rgb;
    memset(&rgb, 0, sizeof(rgb));
    avifRGBImageSetDefaults(&rgb, avImage);
    rgb.depth = 8;
    rgb.format = AVIF_RGB_FORMAT_RGBA;
    avifRGBImageAllocatePixels(&rgb);

    result = avifImageYUVToRGB(avImage, &rgb);
    if (result != AVIF_RESULT_OK)
    {
        addToLog(QString("AVIF: YUV to RGB failed: %1").arg(avifResultToString(result)), LOGIMAG);
        avifRGBImageFreePixels(&rgb);
        avifDecoderDestroy(decoder);
        return qimage;
    }

    /* Create QImage from RGBA data */
    qimage = QImage(width, height, QImage::Format_RGB32);
    QRgb *bits = (QRgb *)qimage.bits();

    for (int y = 0; y < height; y++)
    {
        const uint8_t *row = rgb.pixels + y * rgb.rowBytes;
        for (int x = 0; x < width; x++)
        {
            int idx = x * 4;
            bits[y * width + x] = qRgb(row[idx], row[idx + 1], row[idx + 2]);
        }
    }

    avifRGBImageFreePixels(&rgb);
    avifDecoderDestroy(decoder);
    return qimage;
}

QByteArray avifCodec::encode(QImage qimage, QImage &newImage, int &fileSize,
                           int compressionRatio)
{
    QByteArray byteArray;
    fileSize = 0;

    if (qimage.isNull()) return byteArray;

    /* Convert to RGB32 if needed */
    QImage srcImage = qimage.convertToFormat(QImage::Format_RGB32);
    int width = srcImage.width();
    int height = srcImage.height();

    /* Create avifImage */
    avifImage *avImage = avifImageCreate(width, height, 8, AVIF_PIXEL_FORMAT_YUV420);
    if (!avImage)
    {
        addToLog("AVIF: failed to create image", LOGIMAG);
        return byteArray;
    }

    /* Convert QImage RGB to avifImage via RGB intermediary */
    avifRGBImage rgb;
    memset(&rgb, 0, sizeof(rgb));
    avifRGBImageSetDefaults(&rgb, avImage);
    rgb.depth = 8;
    rgb.format = AVIF_RGB_FORMAT_RGBA;
    avifRGBImageAllocatePixels(&rgb);

    /* Fill RGB pixels from QImage */
    const QRgb *srcBits = (const QRgb *)srcImage.constBits();
    for (int y = 0; y < height; y++)
    {
        uint8_t *row = rgb.pixels + y * rgb.rowBytes;
        for (int x = 0; x < width; x++)
        {
            QRgb pixel = srcBits[y * width + x];
            int idx = x * 4;
            row[idx]     = qRed(pixel);
            row[idx + 1] = qGreen(pixel);
            row[idx + 2] = qBlue(pixel);
            row[idx + 3] = 255; /* alpha */
        }
    }

    avifResult result = avifImageRGBToYUV(avImage, &rgb);
    avifRGBImageFreePixels(&rgb);

    if (result != AVIF_RESULT_OK)
    {
        addToLog(QString("AVIF: RGB to YUV failed: %1").arg(avifResultToString(result)), LOGIMAG);
        avifImageDestroy(avImage);
        return byteArray;
    }

    /* Create encoder */
    avifEncoder *encoder = avifEncoderCreate();
    if (!encoder)
    {
        addToLog("AVIF: failed to create encoder", LOGIMAG);
        avifImageDestroy(avImage);
        return byteArray;
    }

    /* Map compressionRatio to AVIF quality.
       compressionRatio is imageBytes/targetSize (higher = more compression).
       AVIF quality: 0 = worst, 63 = lossless.
       For ham radio, typical ratios are 10-100.
       Map: ratio 1 → quality 60, ratio 100 → quality 10 */
    int quality;
    if (compressionRatio <= 0)
    {
        quality = 50; /* default */
    }
    else if (compressionRatio <= 1)
    {
        quality = 60;
    }
    else
    {
        quality = 60 - (int)(50.0 * (compressionRatio - 1.0) / 99.0);
        if (quality < 5) quality = 5;
        if (quality > 60) quality = 60;
    }

    encoder->quality = quality;
    encoder->qualityAlpha = quality;
    encoder->speed = 6; /* 0=slowest/best, 10=fastest */

    /* Encode to memory */
    avifRWData output = AVIF_DATA_EMPTY;
    result = avifEncoderWrite(encoder, avImage, &output);
    avifEncoderDestroy(encoder);
    avifImageDestroy(avImage);

    if (result != AVIF_RESULT_OK)
    {
        addToLog(QString("AVIF: encode failed: %1").arg(avifResultToString(result)), LOGIMAG);
        avifRWDataFree(&output);
        return byteArray;
    }

    /* Write to temp file (matching jp2IO pattern) */
    QString fn = QString("%1/%2").arg(QDir::tempPath()).arg("qsstv_avif.tmp");
    QFile fi(fn);
    if (fi.open(QIODevice::WriteOnly))
    {
        fi.write((const char *)output.data, output.size);
        fi.close();
    }
    avifRWDataFree(&output);

    /* Read back as QByteArray */
    if (fi.open(QIODevice::ReadOnly))
    {
        byteArray = fi.readAll();
        fileSize = byteArray.count();
    }
    fi.close();

    /* Decode back to get newImage (matching jp2IO interface) */
    newImage = decode(fn);

    /* Clean up temp file */
    QFile::remove(fn);

    return byteArray;
}
