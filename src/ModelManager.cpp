#include "ModelManager.hpp"
#include "Logger.hpp"
#include <QDir>
#include <QStandardPaths>
#include <QNetworkRequest>
#include <QMessageBox>
#include <QProcess>
#include <QWidget>

// ── Static model catalogue ─────────────────────────────────────────────────
//
// All URLs verified with HEAD requests on 2026-04-20:
//   briaai/RMBG-1.4  onnx/model.onnx                → 200 OK  (168 MB)
//   imgdesignart/realesrgan-x4-onnx  onnx/model.onnx → 200 OK   (64 MB)
//   s1777/yolo-v8n-onnx  yolov8n.onnx               → 200 OK   (12 MB)
//   onnx/models  candy/mosaic/pointilism/rain-princess-8.onnx → 200 OK (6 MB each)
//
// Dead URLs replaced:
//   xinntao/Real-ESRGAN v0.2.5.0 realesrgan-x4plus.onnx  → 404
//   ultralytics/assets v0.0.0 yolov8n.onnx               → 404
//   piddnad/ddcolor-models ddcolor.onnx                   → 404 (no public ONNX)
//   BiRefNet-general-epoch_244.onnx                       → 404
//   Xenova/real-esrgan-x4 real-esrgan-x4.onnx            → 401

static const QString NS_BASE =
    "https://media.githubusercontent.com/media/onnx/models/refs/heads/main/"
    "validated/vision/style_transfer/fast_neural_style/model/";

QList<ModelManager::ModelInfo> ModelManager::availableModels() {
    return {
        {
            "rmbg",
            "Background Removal",
            "RMBG-1.4 — removes image background, outputs PNG with transparency",
            "https://huggingface.co/briaai/RMBG-1.4/resolve/main/onnx/model.onnx",
            "rmbg.onnx",
            168
        },
        {
            "realesrgan",
            "Super Resolution ×4",
            "Real-ESRGAN x4 — upscales image 4× with AI-enhanced detail",
            "https://huggingface.co/imgdesignart/realesrgan-x4-onnx/resolve/main/onnx/model.onnx",
            "realesrgan.onnx",
            64
        },
        {
            "yolov8n",
            "Object Detection",
            "YOLOv8 nano — detects and labels objects in the image",
            "https://huggingface.co/s1777/yolo-v8n-onnx/resolve/main/yolov8n.onnx",
            "yolov8n.onnx",
            12
        },
        {
            "style_candy",
            "Style: Candy",
            "Fast Neural Style Transfer — Candy artistic style",
            NS_BASE + "candy-8.onnx",
            "style_candy.onnx",
            6
        },
        {
            "style_mosaic",
            "Style: Mosaic",
            "Fast Neural Style Transfer — Mosaic artistic style",
            NS_BASE + "mosaic-8.onnx",
            "style_mosaic.onnx",
            6
        },
        {
            "style_pointilism",
            "Style: Pointilism",
            "Fast Neural Style Transfer — Pointilism artistic style",
            NS_BASE + "pointilism-8.onnx",
            "style_pointilism.onnx",
            6
        },
        {
            "style_rain_princess",
            "Style: Rain Princess",
            "Fast Neural Style Transfer — Rain Princess artistic style",
            NS_BASE + "rain-princess-8.onnx",
            "style_rain_princess.onnx",
            6
        }
    };
}

// ── Path helpers ──────────────────────────────────────────────────────────

QString ModelManager::modelsDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!base.endsWith("cimg-viewer"))
        base += "/cimg-viewer";
    return base + "/models";
}

QString ModelManager::modelPath(const QString& id) {
    for (const auto& m : availableModels())
        if (m.id == id) return modelsDir() + "/" + m.filename;
    return {};
}

bool ModelManager::isDownloaded(const QString& id) {
    QString p = modelPath(id);
    return !p.isEmpty() && QFile::exists(p);
}

// ── ONNX model validation ─────────────────────────────────────────────────
//
// Checks:
//  1. File exists and size ≥ 80 % of expected (truncation guard)
//  2. python3 -m onnx check (optional — logs result, does not block)

bool ModelManager::validateModel(const QString& path, qint64 expectedSizeMB, QString* outError) {
    QFileInfo fi(path);
    if (!fi.exists()) {
        if (outError) *outError = "File not found: " + path;
        return false;
    }

    qint64 bytes       = fi.size();
    qint64 expectedMin = expectedSizeMB * 1024 * 1024 * 8 / 10; // 80 % of expected
    if (bytes < expectedMin) {
        QString msg = QString("Model file too small: %1 bytes, expected ≥ %2 MB (%3 B) — "
                              "likely truncated download")
            .arg(bytes).arg(expectedSizeMB).arg(expectedMin);
        LOG_ERROR("ModelManager", msg);
        if (outError) *outError = msg;
        return false;
    }

    LOG_INFO("ModelManager",
        QString("Size check OK: %1 (%2 MB)")
            .arg(fi.fileName())
            .arg(bytes / 1048576.0, 0, 'f', 1));

    // Try python3 onnx checker (best-effort; skipped if onnx not installed)
    // On Windows the executable is "python", on Unix it is "python3"
    QProcess proc;
#ifdef Q_OS_WIN
    const QString pythonExe = "python";
#else
    const QString pythonExe = "python3";
#endif
    proc.start(pythonExe, {"-c",
        QString("import onnx, sys; m=onnx.load('%1'); onnx.checker.check_model(m); "
                "print('ONNX check OK: ' + m.graph.name)").arg(path)});
    if (proc.waitForFinished(10000)) {
        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (proc.exitCode() == 0) {
            LOG_INFO("ModelManager", QString("onnx.checker: %1").arg(out));
        } else if (!err.contains("No module named")) {
            // onnx IS installed but check failed
            QString msg = QString("onnx.checker FAILED for %1: %2").arg(fi.fileName()).arg(err);
            LOG_ERROR("ModelManager", msg);
            if (outError) *outError = msg;
            return false;
        } else {
            LOG_INFO("ModelManager",
                QString("onnx python module not available — skipping onnx.checker for %1")
                    .arg(fi.fileName()));
        }
    }

    return true;
}

// ── Singleton ─────────────────────────────────────────────────────────────

ModelManager& ModelManager::instance() {
    static ModelManager inst;
    return inst;
}

ModelManager::ModelManager(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    // Apply redirect policy globally — covers both HEAD probes and GET downloads.
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

// ── Download ──────────────────────────────────────────────────────────────

void ModelManager::startDownload(const QString& id, QWidget* parent) {
    if (m_activeReply) {
        QMessageBox::warning(parent, "Download in Progress",
            "Another model is already downloading. Please wait.");
        return;
    }

    ModelInfo info;
    bool found = false;
    for (const auto& m : availableModels()) {
        if (m.id == id) { info = m; found = true; break; }
    }
    if (!found) return;

    QDir().mkpath(modelsDir());

    m_downloadingId = id;
    QString destPath = modelsDir() + "/" + info.filename;

    // Phase 1: HEAD probe — verify the URL returns 200 before downloading.
    LOG_INFO("ModelManager", QString("HEAD probe: %1").arg(info.url));

    QNetworkRequest headReq(info.url);
    m_activeReply = m_nam->head(headReq);

    connect(m_activeReply, &QNetworkReply::finished, this, [this, info, destPath]() {
        int httpStatus = m_activeReply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QNetworkReply::NetworkError netErr = m_activeReply->error();

        m_activeReply->deleteLater();
        m_activeReply = nullptr;

        // httpStatus == 0 means Qt resolved the redirect and the final status
        // is embedded; treat as success only if no network error.
        if (netErr != QNetworkReply::NoError ||
            (httpStatus != 0 && httpStatus != 200 && httpStatus != 206)) {
            QString err = QString("URL probe failed: HTTP %1 for %2")
                .arg(httpStatus).arg(info.url);
            LOG_ERROR("ModelManager", err);
            m_downloadingId.clear();
            emit downloadFinished(info.id, false, err);
            return;
        }

        LOG_INFO("ModelManager",
            QString("HEAD %1 → HTTP %2 OK — starting GET")
                .arg(info.url)
                .arg(httpStatus == 0 ? 200 : httpStatus));

        startGet(info, destPath);
    });
}

void ModelManager::startGet(const ModelInfo& info, const QString& destPath) {
    m_destFile = new QFile(destPath + ".part", this);
    if (!m_destFile->open(QIODevice::WriteOnly)) {
        QString err = "Cannot open file for writing: " + destPath;
        LOG_ERROR("ModelManager", err);
        emit downloadFinished(info.id, false, err);
        delete m_destFile; m_destFile = nullptr;
        m_downloadingId.clear();
        return;
    }

    LOG_INFO("ModelManager",
        QString("GET %1 → %2").arg(info.url).arg(info.filename));

    QNetworkRequest req(info.url);
    // Redirect policy is already set on m_nam globally in constructor.
    m_activeReply = m_nam->get(req);

    connect(m_activeReply, &QNetworkReply::downloadProgress,
            this, [this](qint64 recv, qint64 total) {
                emit downloadProgress(m_downloadingId, recv, total);
            });

    connect(m_activeReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_destFile) m_destFile->write(m_activeReply->readAll());
    });

    connect(m_activeReply, &QNetworkReply::finished, this, [this, destPath, info]() {
        QString id   = m_downloadingId;
        bool ok      = (m_activeReply->error() == QNetworkReply::NoError);
        int httpStatus = m_activeReply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString err;

        if (ok) {
            m_destFile->close();
            QFile::remove(destPath);
            ok = m_destFile->rename(destPath);
            if (ok) {
                // Validate before declaring success
                QString validErr;
                if (!validateModel(destPath, info.expectedSizeMB, &validErr)) {
                    ok  = false;
                    err = validErr;
                    QFile::remove(destPath);
                    LOG_ERROR("ModelManager",
                        QString("Post-download validation failed for %1: %2")
                            .arg(info.filename).arg(validErr));
                } else {
                    qint64 bytes = QFileInfo(destPath).size();
                    LOG_INFO("ModelManager",
                        QString("Download + validation OK: %1 (%2 MB) — HTTP %3")
                            .arg(info.filename)
                            .arg(bytes / 1048576.0, 0, 'f', 1)
                            .arg(httpStatus));
                }
            } else {
                err = "Failed to rename downloaded file";
                LOG_ERROR("ModelManager",
                    QString("Rename failed for %1").arg(destPath));
            }
        } else {
            err = m_activeReply->errorString();
            LOG_ERROR("ModelManager",
                QString("GET failed: %1 — HTTP %2 — %3")
                    .arg(info.filename).arg(httpStatus).arg(err));
            m_destFile->close();
            m_destFile->remove();
        }

        m_activeReply->deleteLater();
        m_activeReply = nullptr;
        delete m_destFile;
        m_destFile = nullptr;
        m_downloadingId.clear();

        emit downloadFinished(id, ok, err);
    });
}

void ModelManager::cancelDownload() {
    if (m_activeReply) m_activeReply->abort();
}
