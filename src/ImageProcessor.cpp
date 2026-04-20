#include "ImageProcessor.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── Helpers ──────────────────────────────────────────────────────────────────

static inline unsigned char clamp8(int v) {
    return (unsigned char)std::max(0, std::min(255, v));
}

CImgU8 ImageProcessor::toGrayscaleInternal(const CImgU8& img) {
    if (img.spectrum() == 1) return img;
    CImgU8 gray(img.width(), img.height(), 1, 1);
    cimg_forXY(img, x, y) {
        int r = img(x, y, 0, 0), g = img(x, y, 0, 1), b = img(x, y, 0, 2);
        gray(x, y, 0, 0) = clamp8((int)(0.299f * r + 0.587f * g + 0.114f * b));
    }
    return gray;
}

// ── Point processing ─────────────────────────────────────────────────────────

CImgU8 ImageProcessor::brightness(const CImgU8& img, int delta) {
    CImgU8 result = img;
    cimg_foroff(result, off) result[off] = clamp8((int)result[off] + delta);
    return result;
}

CImgU8 ImageProcessor::contrast(const CImgU8& img, double factor) {
    CImgU8 result = img;
    cimg_foroff(result, off) {
        int v = (int)std::round((result[off] - 128.0) * factor + 128.0);
        result[off] = clamp8(v);
    }
    return result;
}

CImgU8 ImageProcessor::gamma(const CImgU8& img, double g) {
    if (g <= 0) g = 0.01;
    double inv = 1.0 / g;
    CImgU8 result = img;
    cimg_foroff(result, off) {
        double v = std::pow(result[off] / 255.0, inv) * 255.0;
        result[off] = clamp8((int)std::round(v));
    }
    return result;
}

CImgU8 ImageProcessor::toGrayscale(const CImgU8& img) {
    return toGrayscaleInternal(img);
}

CImgU8 ImageProcessor::negative(const CImgU8& img) {
    CImgU8 result = img;
    cimg_foroff(result, off) result[off] = (unsigned char)(255 - result[off]);
    return result;
}

CImgU8 ImageProcessor::histogramEqualization(const CImgU8& img) {
    CImgU8 result = img;
    int total = img.width() * img.height();

    for (int c = 0; c < img.spectrum(); ++c) {
        int hist[256] = {};
        cimg_forXY(img, x, y) hist[img(x, y, 0, c)]++;

        // Build CDF
        int cdf[256] = {};
        cdf[0] = hist[0];
        for (int i = 1; i < 256; ++i) cdf[i] = cdf[i - 1] + hist[i];

        // Find min non-zero CDF
        int cdf_min = 0;
        for (int i = 0; i < 256; ++i) { if (cdf[i] > 0) { cdf_min = cdf[i]; break; } }

        int denom = total - cdf_min;
        if (denom <= 0) continue;

        // LUT
        unsigned char lut[256];
        for (int i = 0; i < 256; ++i) {
            lut[i] = clamp8((int)std::round((double)(cdf[i] - cdf_min) / denom * 255));
        }

        cimg_forXY(result, x, y) result(x, y, 0, c) = lut[img(x, y, 0, c)];
    }
    return result;
}

// ── Filters ──────────────────────────────────────────────────────────────────

CImgU8 ImageProcessor::gaussianBlur(const CImgU8& img, double sigma) {
    cimg_library::CImg<float> f = img;
    f.blur((float)sigma);
    f.cut(0, 255);
    return CImgU8(f);
}

CImgU8 ImageProcessor::medianBlur(const CImgU8& img, int radius) {
    int n = 2 * radius + 1;
    return img.get_blur_median(n);
}

CImgU8 ImageProcessor::sharpen(const CImgU8& img, double amount) {
    cimg_library::CImg<float> orig = img;
    cimg_library::CImg<float> blurred = orig;
    blurred.blur(1.5f);
    cimg_library::CImg<float> result = orig + (float)amount * (orig - blurred);
    result.cut(0, 255);
    return CImgU8(result);
}

CImgU8 ImageProcessor::sobelEdge(const CImgU8& img) {
    static const float kx[3][3] = { {-1,0,1},{-2,0,2},{-1,0,1} };
    static const float ky[3][3] = { {-1,-2,-1},{0,0,0},{1,2,1} };

    cimg_library::CImg<float> gray = toGrayscaleInternal(img);
    cimg_library::CImg<float> mag(img.width(), img.height(), 1, 1, 0.0f);

    for (int y = 1; y < img.height() - 1; ++y) {
        for (int x = 1; x < img.width() - 1; ++x) {
            float gx = 0, gy = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    float p = gray(x + dx, y + dy);
                    gx += kx[dy+1][dx+1] * p;
                    gy += ky[dy+1][dx+1] * p;
                }
            mag(x, y) = std::min(std::sqrt(gx*gx + gy*gy), 255.0f);
        }
    }
    return CImgU8(mag);
}

CImgU8 ImageProcessor::cannyEdge(const CImgU8& img, double sigma) {
    // 1. Grayscale + Gaussian blur
    cimg_library::CImg<float> gray = toGrayscaleInternal(img);
    gray.blur((float)sigma);

    const int W = gray.width(), H = gray.height();

    // 2. Gradient magnitude + direction
    cimg_library::CImg<float> mag(W, H, 1, 1, 0.0f);
    cimg_library::CImg<float> dir(W, H, 1, 1, 0.0f);

    static const float kx[3][3] = { {-1,0,1},{-2,0,2},{-1,0,1} };
    static const float ky[3][3] = { {-1,-2,-1},{0,0,0},{1,2,1} };

    for (int y = 1; y < H - 1; ++y) {
        for (int x = 1; x < W - 1; ++x) {
            float gx = 0, gy = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    gx += kx[dy+1][dx+1] * gray(x+dx, y+dy);
                    gy += ky[dy+1][dx+1] * gray(x+dx, y+dy);
                }
            mag(x, y) = std::sqrt(gx*gx + gy*gy);
            dir(x, y) = std::atan2(gy, gx);
        }
    }

    // 3. Non-maximum suppression
    cimg_library::CImg<float> nms(W, H, 1, 1, 0.0f);
    for (int y = 1; y < H - 1; ++y) {
        for (int x = 1; x < W - 1; ++x) {
            float angle = dir(x, y) * 180.0f / (float)M_PI;
            angle = std::fmod(angle + 180.0f, 180.0f);
            float m = mag(x, y), m1, m2;
            if      (angle < 22.5f || angle >= 157.5f) { m1 = mag(x+1,y);   m2 = mag(x-1,y);   }
            else if (angle < 67.5f)                    { m1 = mag(x+1,y+1); m2 = mag(x-1,y-1); }
            else if (angle < 112.5f)                   { m1 = mag(x,y+1);   m2 = mag(x,y-1);   }
            else                                       { m1 = mag(x+1,y-1); m2 = mag(x-1,y+1); }
            if (m >= m1 && m >= m2) nms(x, y) = m;
        }
    }

    // 4. Double threshold + hysteresis
    float maxMag = nms.max();
    float tHigh = maxMag * 0.20f;
    float tLow  = tHigh  * 0.50f;

    cimg_library::CImg<float> edges(W, H, 1, 1, 0.0f);
    cimg_forXY(nms, x, y) {
        if      (nms(x,y) >= tHigh) edges(x,y) = 255.0f;
        else if (nms(x,y) >= tLow)  edges(x,y) = 128.0f;
    }
    cimg_forXY(edges, x, y) {
        if (edges(x,y) == 128.0f) {
            bool strong = false;
            for (int dy = -1; dy <= 1 && !strong; ++dy)
                for (int dx = -1; dx <= 1 && !strong; ++dx) {
                    int nx = x+dx, ny = y+dy;
                    if (nx>=0 && nx<W && ny>=0 && ny<H && edges(nx,ny) == 255.0f)
                        strong = true;
                }
            edges(x,y) = strong ? 255.0f : 0.0f;
        }
    }
    return CImgU8(edges);
}

CImgU8 ImageProcessor::emboss(const CImgU8& img) {
    static const float k[3][3] = { {-2,-1,0},{-1,1,1},{0,1,2} };
    cimg_library::CImg<float> f = img;
    cimg_library::CImg<float> result(img.width(), img.height(), 1, img.spectrum(), 0.0f);

    for (int c = 0; c < img.spectrum(); ++c) {
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                float sum = 128.0f;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = std::clamp(x+dx, 0, img.width()-1);
                        int ny = std::clamp(y+dy, 0, img.height()-1);
                        sum += k[dy+1][dx+1] * f(nx, ny, 0, c);
                    }
                result(x, y, 0, c) = sum;
            }
        }
    }
    result.cut(0, 255);
    return CImgU8(result);
}

// ── Geometric transforms ──────────────────────────────────────────────────────

CImgU8 ImageProcessor::rotate90CW(const CImgU8& img) {
    // CImg rotate is CCW; -90 = 90° CW
    return img.get_rotate(-90);
}

CImgU8 ImageProcessor::rotate90CCW(const CImgU8& img) {
    return img.get_rotate(90);
}

CImgU8 ImageProcessor::flipH(const CImgU8& img) {
    return img.get_mirror('x');
}

CImgU8 ImageProcessor::flipV(const CImgU8& img) {
    return img.get_mirror('y');
}

CImgU8 ImageProcessor::crop(const CImgU8& img, int x0, int y0, int x1, int y1) {
    // Clamp and normalise so x0 <= x1, y0 <= y1
    x0 = std::clamp(x0, 0, img.width()  - 1);
    y0 = std::clamp(y0, 0, img.height() - 1);
    x1 = std::clamp(x1, 0, img.width()  - 1);
    y1 = std::clamp(y1, 0, img.height() - 1);
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    if (x0 == x1 || y0 == y1) return img; // degenerate rect
    return img.get_crop(x0, y0, 0, 0, x1, y1, 0, img.spectrum() - 1);
}

CImgU8 ImageProcessor::resize(const CImgU8& img, int w, int h) {
    return img.get_resize(w, h, -100, -100, 5); // 5 = bicubic
}

// ── Morphological ─────────────────────────────────────────────────────────────

CImgU8 ImageProcessor::erode(const CImgU8& img, int radius) {
    unsigned int n = (unsigned int)(2 * radius + 1);
    return img.get_erode(n);
}

CImgU8 ImageProcessor::dilate(const CImgU8& img, int radius) {
    unsigned int n = (unsigned int)(2 * radius + 1);
    return img.get_dilate(n);
}

CImgU8 ImageProcessor::morphOpen(const CImgU8& img, int radius) {
    return dilate(erode(img, radius), radius);
}

CImgU8 ImageProcessor::morphClose(const CImgU8& img, int radius) {
    return erode(dilate(img, radius), radius);
}

CImgU8 ImageProcessor::morphGradient(const CImgU8& img, int radius) {
    cimg_library::CImg<float> d = dilate(img, radius);
    cimg_library::CImg<float> e = erode(img, radius);
    cimg_library::CImg<float> g = d - e;
    g.cut(0, 255);
    return CImgU8(g);
}

// ── Histogram utility ─────────────────────────────────────────────────────────

std::vector<std::vector<int>> ImageProcessor::computeHistograms(const CImgU8& img) {
    int nc = img.spectrum();
    std::vector<std::vector<int>> hists(nc, std::vector<int>(256, 0));
    for (int c = 0; c < nc; ++c)
        cimg_forXY(img, x, y) hists[c][img(x, y, 0, c)]++;
    return hists;
}
