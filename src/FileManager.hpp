#pragma once
#include <QString>
#include <QStringList>
#include <QSettings>
#include "ImageConvert.hpp"

class FileManager {
public:
    static constexpr int kMaxRecent = 10;

    // Filter string for QFileDialog
    static QString imageFilter();

    // Load/save via Qt (supports JPG, PNG, BMP, TIFF, etc.)
    static cimg_library::CImg<unsigned char> loadImage(const QString& path, QString* error = nullptr);
    static bool saveImage(const cimg_library::CImg<unsigned char>& img,
                          const QString& path, QString* error = nullptr);

    // Recent files (persisted with QSettings)
    static QStringList recentFiles();
    static void addRecentFile(const QString& path);
    static void clearRecentFiles();
};
