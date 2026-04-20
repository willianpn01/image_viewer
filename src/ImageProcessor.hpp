#pragma once
#define cimg_display 0
#include "CImg.h"
#include <vector>
#include <string>

using CImgU8 = cimg_library::CImg<unsigned char>;

class ImageProcessor {
public:
    // ── Point processing (Stage 4) ─────────────────────────────────────────
    static CImgU8 brightness(const CImgU8& img, int delta);
    static CImgU8 contrast(const CImgU8& img, double factor);
    static CImgU8 gamma(const CImgU8& img, double g);
    static CImgU8 toGrayscale(const CImgU8& img);
    static CImgU8 negative(const CImgU8& img);
    static CImgU8 histogramEqualization(const CImgU8& img);

    // ── Filters (Stage 5) ──────────────────────────────────────────────────
    static CImgU8 gaussianBlur(const CImgU8& img, double sigma);
    static CImgU8 medianBlur(const CImgU8& img, int radius);
    static CImgU8 sharpen(const CImgU8& img, double amount);
    static CImgU8 sobelEdge(const CImgU8& img);
    static CImgU8 cannyEdge(const CImgU8& img, double sigma);
    static CImgU8 emboss(const CImgU8& img);

    // ── Geometric transforms (Stage 6) ────────────────────────────────────
    static CImgU8 rotate90CW(const CImgU8& img);
    static CImgU8 rotate90CCW(const CImgU8& img);
    static CImgU8 flipH(const CImgU8& img);
    static CImgU8 flipV(const CImgU8& img);
    static CImgU8 crop(const CImgU8& img, int x0, int y0, int x1, int y1);
    static CImgU8 resize(const CImgU8& img, int w, int h);

    // ── Morphological operations (Stage 7) ────────────────────────────────
    static CImgU8 erode(const CImgU8& img, int radius);
    static CImgU8 dilate(const CImgU8& img, int radius);
    static CImgU8 morphOpen(const CImgU8& img, int radius);
    static CImgU8 morphClose(const CImgU8& img, int radius);
    static CImgU8 morphGradient(const CImgU8& img, int radius);

    // ── Histogram utility ─────────────────────────────────────────────────
    // Returns one histogram per channel (1 for gray, 3 for RGB)
    static std::vector<std::vector<int>> computeHistograms(const CImgU8& img);

private:
    static CImgU8 toGrayscaleInternal(const CImgU8& img); // always returns 1-channel
};
