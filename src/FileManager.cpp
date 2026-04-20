#include "FileManager.hpp"
#include <QImage>

QString FileManager::imageFilter() {
    return "Images (*.jpg *.jpeg *.png *.bmp *.tiff *.tif *.pbm *.pgm *.ppm);;"
           "JPEG (*.jpg *.jpeg);;"
           "PNG (*.png);;"
           "BMP (*.bmp);;"
           "TIFF (*.tiff *.tif);;"
           "All files (*)";
}

cimg_library::CImg<unsigned char> FileManager::loadImage(const QString& path, QString* error) {
    QImage qimg;
    if (!qimg.load(path)) {
        if (error) *error = QString("Cannot open \"%1\"").arg(path);
        return {};
    }
    return ImageConvert::toCImg(qimg);
}

bool FileManager::saveImage(const cimg_library::CImg<unsigned char>& img,
                             const QString& path, QString* error) {
    QImage qimg = ImageConvert::toQImage(img);
    if (qimg.isNull()) {
        if (error) *error = "No image to save";
        return false;
    }
    if (!qimg.save(path)) {
        if (error) *error = QString("Cannot save \"%1\"").arg(path);
        return false;
    }
    return true;
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
