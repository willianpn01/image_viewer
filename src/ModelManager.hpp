#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>

// Manages AI model files: listing, download status, and downloading.
class ModelManager : public QObject {
    Q_OBJECT
public:
    struct ModelInfo {
        QString id;
        QString name;
        QString description;
        QString url;
        QString filename;
        qint64  expectedSizeMB;  // approximate
    };

    static ModelManager& instance();

    static QString modelsDir();
    static QString modelPath(const QString& id);
    static bool    isDownloaded(const QString& id);

    // Ordered list of all known models
    static QList<ModelInfo> availableModels();

    // Validates a downloaded model: checks file size (≥ 80% of expected) and
    // runs onnx.checker via python3 if available. Logs all results.
    // Returns false + sets outError if invalid.
    static bool validateModel(const QString& path, qint64 expectedSizeMB,
                              QString* outError = nullptr);

    // Start async download; probes URL with HEAD first, then GETs if 200.
    // Runs validateModel after download. Progress/completion via signals.
    void startDownload(const QString& id, QWidget* parent = nullptr);
    void cancelDownload();

    bool isDownloading() const { return m_activeReply != nullptr; }
    QString downloadingId() const { return m_downloadingId; }

signals:
    void downloadProgress(QString id, qint64 received, qint64 total);
    void downloadFinished(QString id, bool success, QString error);

private:
    explicit ModelManager(QObject* parent = nullptr);
    ModelManager(const ModelManager&) = delete;

    void startGet(const ModelInfo& info, const QString& destPath);

    QNetworkAccessManager* m_nam           = nullptr;
    QNetworkReply*         m_activeReply   = nullptr;
    QFile*                 m_destFile      = nullptr;
    QString                m_downloadingId;
};
