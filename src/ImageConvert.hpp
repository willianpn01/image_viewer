#pragma once
// Conversion utilities between CImg<unsigned char> and QImage.
// This header may be included by files that use both Qt and CImg.

#define cimg_display 0
#include "CImg.h"
#include <QImage>

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

    // RGB (or more channels — use first 3)
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
