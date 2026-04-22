#include "ModelManagerDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>

ModelManagerDialog::ModelManagerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("AI Model Manager");
    resize(700, 320);

    auto* layout = new QVBoxLayout(this);

    auto* info = new QLabel(
        "Models are stored in: <b>" + ModelManager::modelsDir() + "</b>", this);
    info->setWordWrap(true);
    layout->addWidget(info);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"Model", "Function", "Size (MB)", "Status", "Action"});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(btns);

    // Connect download signals
    auto& mgr = ModelManager::instance();
    connect(&mgr, &ModelManager::downloadProgress,
            this, &ModelManagerDialog::onDownloadProgress);
    connect(&mgr, &ModelManager::downloadFinished,
            this, &ModelManagerDialog::onDownloadFinished);

    refreshTable();
}

void ModelManagerDialog::refreshTable() {
    auto models = ModelManager::availableModels();
    m_table->setRowCount(models.size());

    for (int r = 0; r < models.size(); r++) {
        const auto& m = models[r];
        m_table->setItem(r, 0, new QTableWidgetItem(m.name));
        m_table->setItem(r, 1, new QTableWidgetItem(m.description));
        m_table->setItem(r, 2, new QTableWidgetItem(QString("~%1").arg(m.expectedSizeMB)));

        bool downloaded = ModelManager::isDownloaded(m.id);
        auto* statusItem = new QTableWidgetItem(downloaded ? "✓ Downloaded" : "Not downloaded");
        statusItem->setForeground(downloaded ? Qt::darkGreen : Qt::red);
        m_table->setItem(r, 3, statusItem);

        auto* btn = new QPushButton(downloaded ? "Re-download" : "Download", m_table);
        btn->setEnabled(!ModelManager::instance().isDownloading());
        connect(btn, &QPushButton::clicked, this, [this, r]() { onDownloadClicked(r); });
        m_table->setCellWidget(r, 4, btn);
    }
    m_table->resizeColumnsToContents();
}

void ModelManagerDialog::onDownloadClicked(int row) {
    auto models = ModelManager::availableModels();
    if (row >= models.size()) return;
    const QString id = models[row].id;

    // Replace button with progress bar
    auto* pb = new QProgressBar(m_table);
    pb->setRange(0, 100);
    pb->setValue(0);
    pb->setFormat("Starting…");
    m_table->setCellWidget(row, 4, pb);
    m_table->item(row, 3)->setText("Downloading…");

    ModelManager::instance().startDownload(id, this);
}

int ModelManagerDialog::rowForId(const QString& id) const {
    auto models = ModelManager::availableModels();
    for (int i = 0; i < models.size(); i++)
        if (models[i].id == id) return i;
    return -1;
}

void ModelManagerDialog::onDownloadProgress(QString id, qint64 recv, qint64 total) {
    int row = rowForId(id);
    if (row < 0) return;
    auto* pb = qobject_cast<QProgressBar*>(m_table->cellWidget(row, 4));
    if (!pb) return;
    if (total > 0) {
        int pct = (int)(recv * 100 / total);
        pb->setValue(pct);
        pb->setFormat(QString("%1 / %2 MB (%3%)").arg(recv/1048576).arg(total/1048576).arg(pct));
    } else {
        pb->setFormat(QString("%1 MB").arg(recv/1048576));
    }
}

void ModelManagerDialog::onDownloadFinished(QString id, bool success, QString error) {
    int row = rowForId(id);
    if (row < 0) return;

    if (!success) {
        QMessageBox::warning(this, "Download Failed",
            QString("Failed to download '%1':\n%2").arg(id, error));
    }
    refreshTable();
}
