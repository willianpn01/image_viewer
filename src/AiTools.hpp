#pragma once
#include <QObject>
#include <QThread>
#include <QString>
#include <QVector>
#include <QRectF>
#include <QPair>
#include "ImageProcessor.hpp"
#include "AnnotationLayer.hpp"

// Internal worker (runs in a separate QThread)
class AiWorker : public QObject {
    Q_OBJECT
public:
    enum Operation { RemoveBg, SuperRes, Detect, NeuralStyle };

    // For NeuralStyle, modelPath encodes which style via the filename.
    explicit AiWorker(Operation op, const CImgU8& img,
                      const QString& modelPath, QObject* parent = nullptr);

    void requestCancel() { m_cancel = true; }

signals:
    void finished(CImgU8 result, QString opName);
    void objectsDetected(QVector<QPair<QRectF, QString>> boxes);
    void error(QString message);
    void progress(int percent);

public slots:
    void run();

private:
    // Each run* method returns a valid image on success or an empty image
    // after emitting error(). The run() dispatcher only emits finished()
    // if the returned image is non-empty.
    CImgU8 runRemoveBg();
    CImgU8 runSuperRes();
    CImgU8 runNeuralStyle();
    void   runDetect();

    Operation m_op;
    CImgU8    m_img;
    QString   m_modelPath;
    bool      m_cancel = false;
};

// Public facade — call static methods to start AI operations.
// Results are delivered via the provided lambdas on the main thread.
class AiTools : public QObject {
    Q_OBJECT
public:
    static void removeBackground(const CImgU8& img, QWidget* parent,
        std::function<void(CImgU8)> onDone,
        std::function<void(QString)> onError);

    static void superResolution(const CImgU8& img, QWidget* parent,
        std::function<void(CImgU8)> onDone,
        std::function<void(QString)> onError);

    static void detectObjects(const CImgU8& img, QWidget* parent,
        std::function<void(QVector<QPair<QRectF,QString>>)> onDone,
        std::function<void(QString)> onError);

    // styleId: "style_candy" | "style_mosaic" | "style_pointilism" | "style_rain_princess"
    static void styleTransfer(const CImgU8& img, const QString& styleId, QWidget* parent,
        std::function<void(CImgU8)> onDone,
        std::function<void(QString)> onError);

private:
    static void runOp(AiWorker::Operation op, const CImgU8& img,
        const QString& modelId, QWidget* parent,
        std::function<void(CImgU8, QString)> onDone,
        std::function<void(QVector<QPair<QRectF,QString>>)> onDetected,
        std::function<void(QString)> onError);
};
