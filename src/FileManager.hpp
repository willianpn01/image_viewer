#pragma once
#include <QString>
#include <QStringList>
#include <QSettings>
#include "ImageConvert.hpp"

class FileManager {
public:
    static constexpr int kMaxRecent = 10;

    // Centralized list of supported extensions (wildcard patterns for QDir / QFileDialog)
    static QStringList supportedExtensions();

    // Filter string for QFileDialog
    static QString imageFilter();

    // Filter string for Save As dialog (formats Qt can reliably write)
    static QString saveFilter();

    // Load image: tries Qt first, then CImg fallback for TGA/HDR/etc.
    static cimg_library::CImg<unsigned char> loadImage(const QString& path, QString* error = nullptr);

    // Save image: tries Qt first, then CImg fallback for TGA
    static bool saveImage(const cimg_library::CImg<unsigned char>& img,
                          const QString& path, QString* error = nullptr);

    // Recent files (persisted with QSettings)
    static QStringList recentFiles();
    static void addRecentFile(const QString& path);
    static void clearRecentFiles();
};
