#include "FileManager.hpp"
#include <QImage>
#include <QFile>
#include <QFileInfo>

QStringList FileManager::supportedExtensions() {
    return {
        "*.jpg", "*.jpeg",          // JPEG
        "*.png",                     // PNG
        "*.bmp",                     // BMP
        "*.tiff", "*.tif",          // TIFF
        "*.webp",                    // WebP (Qt plugin or libwebp)
        "*.gif",                     // GIF  (qgif plugin)
        "*.ico",                     // ICO  (qico plugin)
        "*.svg", "*.svgz",          // SVG  (qsvg plugin)
        "*.tga",                     // TGA  (built-in reader/writer)
        "*.hdr",                     // Radiance HDR (via external tools)
        "*.avif",                    // AVIF (Qt 6.5+ / libavif)
        "*.heic", "*.heif",         // HEIC/HEIF (OS codec)
        "*.exr",                     // OpenEXR (libOpenEXR)
        "*.psd",                     // Photoshop (external)
        "*.pbm", "*.pgm", "*.ppm",  // Netpbm
    };
}

QString FileManager::imageFilter() {
    return
        "All Images ("
            "*.jpg *.jpeg *.png *.bmp *.tiff *.tif "
            "*.webp *.gif *.ico *.svg *.svgz "
            "*.tga *.hdr *.avif *.heic *.heif *.exr *.psd "
            "*.pbm *.pgm *.ppm"
        ");;"
        "JPEG (*.jpg *.jpeg);;"
        "PNG (*.png);;"
        "BMP (*.bmp);;"
        "TIFF (*.tiff *.tif);;"
        "WebP (*.webp);;"
        "GIF (*.gif);;"
        "ICO (*.ico);;"
        "SVG (*.svg *.svgz);;"
        "TGA (*.tga);;"
        "HDR / Radiance (*.hdr);;"
        "AVIF (*.avif);;"
        "HEIC / HEIF (*.heic *.heif);;"
        "OpenEXR (*.exr);;"
        "Photoshop (*.psd);;"
        "Netpbm (*.ppm *.pgm *.pbm);;"
        "All files (*)";
}

QString FileManager::saveFilter() {
    return
        "PNG (*.png);;"
        "JPEG (*.jpg *.jpeg);;"
        "BMP (*.bmp);;"
        "TIFF (*.tiff *.tif);;"
        "WebP (*.webp);;"
        "TGA (*.tga);;"
        "All files (*)";
}

// Minimal uncompressed TGA 1.0 writer (no external dependencies)
static bool writeTga(const cimg_library::CImg<unsigned char>& img, const QString& path,
                     QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = QString("Cannot create \"%1\"").arg(path);
        return false;
    }

    int w = img.width(), h = img.height(), ch = img.spectrum();
    // Image type: 2 = uncompressed true-color, 3 = uncompressed grayscale
    uint8_t imgType = (ch == 1) ? 3 : 2;
    uint8_t bpp     = (ch >= 4) ? 32 : (ch == 1) ? 8 : 24;

    // 18-byte TGA header
    uint8_t hdr[18] = {};
    hdr[2]  = imgType;
    hdr[12] = static_cast<uint8_t>(w & 0xff);
    hdr[13] = static_cast<uint8_t>((w >> 8) & 0xff);
    hdr[14] = static_cast<uint8_t>(h & 0xff);
    hdr[15] = static_cast<uint8_t>((h >> 8) & 0xff);
    hdr[16] = bpp;
    hdr[17] = 0x20; // bit 5 = top-left origin
    f.write(reinterpret_cast<char*>(hdr), 18);

    // Pixel data — TGA uses BGR / BGRA interleaving
    QByteArray row;
    row.reserve(w * (bpp / 8));
    for (int y = 0; y < h; ++y) {
        row.clear();
        for (int x = 0; x < w; ++x) {
            if (ch == 1) {
                row += static_cast<char>(img(x, y, 0, 0));
            } else if (ch >= 4) {
                row += static_cast<char>(img(x, y, 0, 2)); // B
                row += static_cast<char>(img(x, y, 0, 1)); // G
                row += static_cast<char>(img(x, y, 0, 0)); // R
                row += static_cast<char>(img(x, y, 0, 3)); // A
            } else {
                row += static_cast<char>(img(x, y, 0, 2)); // B
                row += static_cast<char>(img(x, y, 0, 1)); // G
                row += static_cast<char>(img(x, y, 0, 0)); // R
            }
        }
        f.write(row);
    }
    return true;
}

// Minimal uncompressed TGA 1.0 reader
static cimg_library::CImg<unsigned char> readTga(const QString& path, QString* error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QString("Cannot open \"%1\"").arg(path);
        return {};
    }
    QByteArray raw = f.readAll();
    if (raw.size() < 18) {
        if (error) *error = "TGA file too small";
        return {};
    }
    const uint8_t* d = reinterpret_cast<const uint8_t*>(raw.constData());
    int idLen     = d[0];
    // int colorMap  = d[1]; // unused
    int imgType   = d[2];
    // skip color map (5 bytes at 3-7)
    int w = d[12] | (d[13] << 8);
    int h = d[14] | (d[15] << 8);
    int bpp = d[16];
    int desc = d[17];
    bool topLeft = (desc & 0x20) != 0;

    // Only handle uncompressed true-color (2) and grayscale (3)
    if ((imgType != 2 && imgType != 3) || w <= 0 || h <= 0 ||
        (bpp != 8 && bpp != 24 && bpp != 32)) {
        if (error) *error = "Unsupported TGA variant";
        return {};
    }

    int ch = (imgType == 3) ? 1 : (bpp == 32) ? 4 : 3;
    int pixelBytes = bpp / 8;
    int dataOffset = 18 + idLen; // skip header + id string
    if (raw.size() < dataOffset + w * h * pixelBytes) {
        if (error) *error = "TGA data truncated";
        return {};
    }

    cimg_library::CImg<unsigned char> img(w, h, 1, ch);
    const uint8_t* px = reinterpret_cast<const uint8_t*>(raw.constData()) + dataOffset;

    for (int row = 0; row < h; ++row) {
        int y = topLeft ? row : (h - 1 - row);
        for (int x = 0; x < w; ++x) {
            const uint8_t* p = px + (row * w + x) * pixelBytes;
            if (ch == 1) {
                img(x, y, 0, 0) = p[0];
            } else {
                // TGA stores BGR / BGRA
                img(x, y, 0, 0) = p[2]; // R
                img(x, y, 0, 1) = p[1]; // G
                img(x, y, 0, 2) = p[0]; // B
                if (ch == 4) img(x, y, 0, 3) = p[3]; // A
            }
        }
    }
    return img;
}

cimg_library::CImg<unsigned char> FileManager::loadImage(const QString& path, QString* error) {
    // Try Qt first — handles JPG, PNG, BMP, TIFF, GIF, ICO, SVG (via plugins),
    // WebP (if qwebp plugin present), HEIC/AVIF (if OS codec), etc.
    QImage qimg;
    if (qimg.load(path))
        return ImageConvert::toCImg(qimg);

    // Qt failed — try format-specific fallbacks
    QString ext = QFileInfo(path).suffix().toLower();

    if (ext == "tga") {
        auto img = readTga(path, error);
        if (!img.is_empty()) return img;
        return {};
    }

    // Generic CImg fallback for remaining formats (BMP, PNM, TIFF with libtiff,
    // PNG with libpng, and anything external tools like ImageMagick can handle)
    try {
        cimg_library::CImg<unsigned char> img;
        img.load(path.toStdString().c_str());
        if (!img.is_empty()) return img;
    } catch (...) {}

    if (error) *error = QString("Cannot open \"%1\"").arg(path);
    return {};
}

bool FileManager::saveImage(const cimg_library::CImg<unsigned char>& img,
                             const QString& path, QString* error) {
    QImage qimg = ImageConvert::toQImage(img);
    if (qimg.isNull()) {
        if (error) *error = "No image to save";
        return false;
    }

    // TGA: use built-in writer (Qt has no TGA encoder)
    if (QFileInfo(path).suffix().toLower() == "tga")
        return writeTga(img, path, error);

    // All others: try Qt (handles PNG, JPG, BMP, TIFF, WebP if plugin present, etc.)
    if (qimg.save(path)) return true;

    if (error) *error = QString("Cannot save \"%1\" — format not supported").arg(path);
    return false;
}

QStringList FileManager::recentFiles() {
    QSettings s("CImgViewer", "CImgViewer");
    return s.value("recentFiles").toStringList();
}

void FileManager::addRecentFile(const QString& path) {
    QSettings s("CImgViewer", "CImgViewer");
    QStringList list = s.value("recentFiles").toStringList();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > kMaxRecent) list.removeLast();
    s.setValue("recentFiles", list);
}

void FileManager::clearRecentFiles() {
    QSettings s("CImgViewer", "CImgViewer");
    s.remove("recentFiles");
}
