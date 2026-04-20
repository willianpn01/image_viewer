#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QString>

// Dock-widget contents: displays EXIF metadata for the current image in a
// two-column table (Key | Value). Uses the header-only TinyEXIF library.
class ExifPanel : public QWidget {
    Q_OBJECT
public:
    explicit ExifPanel(QWidget* parent = nullptr);

    // Load EXIF data from the given image file path.
    // Pass an empty string to clear the table.
    void loadFile(const QString& filePath);

private:
    QTableWidget* m_table = nullptr;

    void addRow(const QString& key, const QString& value);
    void showNoExif();
};
