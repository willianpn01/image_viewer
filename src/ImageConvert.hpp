#pragma once
// Conversion utilities between CImg<unsigned char> and QImage.
// This header may be included by files that use both Qt and CImg.

#define cimg_display 0
#include "CImg.h"
#include <QImage>
#include <QPainter>

namespace ImageConvert {

inline QImage toQImage(const cimg_library::CImg<unsigned char>& img) {
    if (img.is_empty()) return {};

    if (img.spectrum() == 1) {
        QImage q(img.width(), img.height(), QImage::Format_Grayscale8);
        for (int y = 0; y < img.height(); ++y) {
            uchar* line = q.scanLine(y);
            for (int x = 0; x < img.width(); ++x)
                line[x] = img(x, y, 0, 0);
        }
        return q;
    }

    if (img.spectrum() >= 4) {
        // RGBA — Format_RGBA8888: byte order R,G,B,A in memory
        QImage q(img.width(), img.height(), QImage::Format_RGBA8888);
        for (int y = 0; y < img.height(); ++y) {
            uchar* line = q.scanLine(y);
            for (int x = 0; x < img.width(); ++x) {
                line[x * 4 + 0] = img(x, y, 0, 0);  // R
                line[x * 4 + 1] = img(x, y, 0, 1);  // G
                line[x * 4 + 2] = img(x, y, 0, 2);  // B
                line[x * 4 + 3] = img(x, y, 0, 3);  // A
            }
        }
        return q;
    }

    // RGB
    QImage q(img.width(), img.height(), QImage::Format_RGB888);
    for (int y = 0; y < img.height(); ++y) {
        uchar* line = q.scanLine(y);
        for (int x = 0; x < img.width(); ++x) {
            line[x * 3 + 0] = img(x, y, 0, 0);
            line[x * 3 + 1] = img(x, y, 0, 1);
            line[x * 3 + 2] = img(x, y, 0, 2);
        }
    }
    return q;
}

// Composite an RGBA QImage over a standard white/grey checkerboard for display.
// Returns an opaque RGB QImage (Format_RGB888) suitable for QLabel/QPixmap.
// Uses straight-alpha (non-premultiplied) compositing to avoid halo artefacts.
inline QImage compositeOnCheckerboard(const QImage& rgba, int tileSize = 12) {
    if (!rgba.hasAlphaChannel()) return rgba;

    // Work in ARGB32 (straight alpha) — avoids premultiplied rounding at borders
    QImage src = rgba.convertToFormat(QImage::Format_ARGB32);
    QImage out(src.size(), QImage::Format_RGB888);

    // Standard Photoshop/GIMP checkerboard: white + light-grey tiles
    const int L = 255, D = 191;   // #FFFFFF / #BFBFBF

    for (int y = 0; y < out.height(); ++y) {
        uchar* dstLine = out.scanLine(y);
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            // Checkerboard tile colour for this pixel
            int bg = ((x / tileSize + y / tileSize) % 2 == 0) ? L : D;

            QRgb px   = srcLine[x];
            int  a    = qAlpha(px);
            int  invA = 255 - a;

            // Straight-alpha composite: out = src*a/255 + bg*(1-a/255)
            dstLine[x * 3 + 0] = (unsigned char)((qRed(px)   * a + bg * invA) / 255);
            dstLine[x * 3 + 1] = (unsigned char)((qGreen(px) * a + bg * invA) / 255);
            dstLine[x * 3 + 2] = (unsigned char)((qBlue(px)  * a + bg * invA) / 255);
        }
    }
    return out;
}

inline cimg_library::CImg<unsigned char> toCImg(const QImage& src) {
    if (src.isNull()) return {};

    const bool isGray = (src.format() == QImage::Format_Grayscale8 ||
                         src.format() == QImage::Format_Grayscale16);

    if (isGray) {
        QImage img = src.convertToFormat(QImage::Format_Grayscale8);
        cimg_library::CImg<unsigned char> out(img.width(), img.height(), 1, 1);
        for (int y = 0; y < img.height(); ++y) {
            const uchar* line = img.constScanLine(y);
            for (int x = 0; x < img.width(); ++x)
                out(x, y, 0, 0) = line[x];
        }
        return out;
    }

    QImage img = src.convertToFormat(QImage::Format_RGB888);
    cimg_library::CImg<unsigned char> out(img.width(), img.height(), 1, 3);
    for (int y = 0; y < img.height(); ++y) {
        const uchar* line = img.constScanLine(y);
        for (int x = 0; x < img.width(); ++x) {
            out(x, y, 0, 0) = line[x * 3 + 0];
            out(x, y, 0, 1) = line[x * 3 + 1];
            out(x, y, 0, 2) = line[x * 3 + 2];
        }
    }
    return out;
}

} // namespace ImageConvert
