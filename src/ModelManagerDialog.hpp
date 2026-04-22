#pragma once
#include <QDialog>
#include <QTableWidget>
#include <QProgressBar>
#include <QPushButton>
#include "ModelManager.hpp"

class ModelManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelManagerDialog(QWidget* parent = nullptr);

private slots:
    void onDownloadClicked(int row);
    void onDownloadProgress(QString id, qint64 received, qint64 total);
    void onDownloadFinished(QString id, bool success, QString error);

private:
    void refreshTable();
    int rowForId(const QString& id) const;

    QTableWidget* m_table = nullptr;
};
