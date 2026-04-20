#include "ExifPanel.hpp"

// TinyEXIF lives in third_party/
#include "TinyEXIF.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QFile>
#include <cmath>

ExifPanel::ExifPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);

    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({"Field", "Value"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table);
}

void ExifPanel::addRow(const QString& key, const QString& value) {
    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(key));
    m_table->setItem(row, 1, new QTableWidgetItem(value));
}

void ExifPanel::showNoExif() {
    m_table->setRowCount(0);
    addRow("Info", "Sem metadados EXIF");
}

void ExifPanel::loadFile(const QString& filePath) {
    m_table->setRowCount(0);

    if (filePath.isEmpty()) {
        showNoExif();
        return;
    }

    // Read raw file bytes for TinyEXIF
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) { showNoExif(); return; }
    QByteArray data = f.readAll();
    f.close();

    TinyEXIF::EXIFInfo exif;
    int code = exif.parseFrom(
        reinterpret_cast<const uint8_t*>(data.constData()),
        (unsigned)data.size());

    if (code != TinyEXIF::PARSE_SUCCESS || exif.ImageWidth == 0) {
        showNoExif();
        return;
    }

    // ── Camera ────────────────────────────────────────────────────────────────
    if (!exif.Make.empty())
        addRow("Fabricante",    QString::fromStdString(exif.Make));
    if (!exif.Model.empty())
        addRow("Modelo",        QString::fromStdString(exif.Model));
    if (!exif.Software.empty())
        addRow("Software",      QString::fromStdString(exif.Software));

    // ── Date / time ───────────────────────────────────────────────────────────
    if (!exif.DateTime.empty())
        addRow("Data/hora",     QString::fromStdString(exif.DateTime));

    // ── Image dimensions ──────────────────────────────────────────────────────
    if (exif.ImageWidth > 0 && exif.ImageHeight > 0)
        addRow("Resolução",     QString("%1 × %2 px")
                                    .arg(exif.ImageWidth).arg(exif.ImageHeight));

    // ── Exposure ──────────────────────────────────────────────────────────────
    if (exif.ExposureTime > 0) {
        if (exif.ExposureTime < 1.0)
            addRow("Exposição", QString("1/%1 s").arg((int)std::round(1.0 / exif.ExposureTime)));
        else
            addRow("Exposição", QString("%1 s").arg(exif.ExposureTime, 0, 'f', 2));
    }
    if (exif.FNumber > 0)
        addRow("Abertura",      QString("f/%1").arg(exif.FNumber, 0, 'f', 1));
    if (exif.ISOSpeedRatings > 0)
        addRow("ISO",           QString::number(exif.ISOSpeedRatings));
    if (exif.FocalLength > 0)
        addRow("Focal length",  QString("%1 mm").arg(exif.FocalLength, 0, 'f', 1));

    // ── Flash ──────────────────────────────────────────────────────────────────
    addRow("Flash", (exif.Flash != 0) ? "Sim" : "Não");

    // ── GPS ───────────────────────────────────────────────────────────────────
    if (exif.GeoLocation.hasLatLon()) {
        addRow("Latitude",  QString::number(exif.GeoLocation.Latitude,  'f', 6));
        addRow("Longitude", QString::number(exif.GeoLocation.Longitude, 'f', 6));
    }
    if (exif.GeoLocation.hasAltitude()) {
        addRow("Altitude",  QString("%1 m").arg(exif.GeoLocation.Altitude, 0, 'f', 1));
    }

    if (m_table->rowCount() == 0) showNoExif();
}
