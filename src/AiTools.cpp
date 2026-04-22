#include "AiTools.hpp"
#include "OnnxEngine.hpp"
#include "ModelManager.hpp"
#include "Logger.hpp"
#include <QProgressDialog>
#include <QMessageBox>
#include <QWidget>
#include <QApplication>
#include <QElapsedTimer>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

// ── COCO class names (YOLOv8, 80 classes) ─────────────────────────────────

static const char* COCO_CLASSES[] = {
    "person","bicycle","car","motorcycle","airplane","bus","train","truck",
    "boat","traffic light","fire hydrant","stop sign","parking meter","bench",
    "bird","cat","dog","horse","sheep","cow","elephant","bear","zebra","giraffe",
    "backpack","umbrella","handbag","tie","suitcase","frisbee","skis","snowboard",
    "sports ball","kite","baseball bat","baseball glove","skateboard","surfboard",
    "tennis racket","bottle","wine glass","cup","fork","knife","spoon","bowl",
    "banana","apple","sandwich","orange","broccoli","carrot","hot dog","pizza",
    "donut","cake","chair","couch","potted plant","bed","dining table","toilet",
    "tv","laptop","mouse","remote","keyboard","cell phone","microwave","oven",
    "toaster","sink","refrigerator","book","clock","vase","scissors",
    "teddy bear","hair drier","toothbrush"
};
static constexpr int COCO_COUNT = 80;

// ── Tensor helpers ─────────────────────────────────────────────────────────

static std::vector<float> imgToTensorRGB(const CImgU8& img, int W, int H) {
    CImgU8 resized;
    if (img.width() == W && img.height() == H)
        resized = img;
    else
        resized = ImageProcessor::resize(img, W, H);

    std::vector<float> data(3 * H * W);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int r = resized(x, y, 0, 0);
            int g = (resized.spectrum() > 1) ? resized(x, y, 0, 1) : r;
            int b = (resized.spectrum() > 2) ? resized(x, y, 0, 2) : r;
            data[0 * H * W + y * W + x] = r / 255.f;
            data[1 * H * W + y * W + x] = g / 255.f;
            data[2 * H * W + y * W + x] = b / 255.f;
        }
    }
    return data;
}

// ── AiWorker constructor ────────────────────────────────────────────────────

AiWorker::AiWorker(Operation op, const CImgU8& img,
                   const QString& modelPath, QObject* parent)
    : QObject(parent), m_op(op), m_img(img), m_modelPath(modelPath)
{}

// ── Run dispatcher ─────────────────────────────────────────────────────────
//
// Each runXxx() either:
//  - returns a non-empty CImgU8  → run() emits finished()
//  - returns {}  AND already emitted error() → run() does NOT emit finished()
//
// This prevents emitting both error() and finished() on the same operation,
// which could leave the QThread cleanup in an inconsistent state.

void AiWorker::run() {
    try {
        switch (m_op) {
            case RemoveBg: {
                CImgU8 r = runRemoveBg();
                if (!r.is_empty()) emit finished(r, "Remove Background");
                break;
            }
            case SuperRes: {
                CImgU8 r = runSuperRes();
                if (!r.is_empty()) emit finished(r, "Super Resolution ×4");
                break;
            }
            case NeuralStyle: {
                CImgU8 r = runNeuralStyle();
                if (!r.is_empty()) emit finished(r, "Neural Style");
                break;
            }
            case Detect:
                runDetect();   // emits objectsDetected() or error() internally
                break;
        }
    } catch (const std::exception& e) {
        // Catch anything that escaped the per-op try/catch
        QString msg = QString("Unexpected error in AI worker: %1").arg(e.what());
        LOG_ERROR("AiTools", msg);
        emit error(msg);
    } catch (...) {
        QString msg = "Unknown exception in AI worker";
        LOG_ERROR("AiTools", msg);
        emit error(msg);
    }
}

// ── Remove Background ──────────────────────────────────────────────────────

CImgU8 AiWorker::runRemoveBg() {
    std::string err;
    auto session = OnnxEngine::instance().createSession(m_modelPath.toStdString(), &err);
    if (!session) {
        LOG_ERROR("AiTools", QString("RMBG session failed: %1").arg(err.c_str()));
        emit error("RMBG model error: " + QString::fromStdString(err));
        return {};
    }

    // RMBG-1.4: fixed input [1, 3, 1024, 1024], ImageNet normalisation
    const int SZ = 1024;
    int origW = m_img.width(), origH = m_img.height();
    LOG_DEBUG("AiTools", QString("RMBG: expected input [1,3,%1,%1]").arg(SZ));
    LOG_DEBUG("AiTools", QString("RMBG: original image %1×%2").arg(origW).arg(origH));
    LOG_DEBUG("AiTools", QString("RMBG: resizing to %1×%1").arg(SZ));

    std::vector<float> input = imgToTensorRGB(m_img, SZ, SZ);  // returns [0,1] float

    const float mean[] = {0.485f, 0.456f, 0.406f};
    const float stdv[] = {0.229f, 0.224f, 0.225f};
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < SZ*SZ; i++)
            input[c*SZ*SZ + i] = (input[c*SZ*SZ + i] - mean[c]) / stdv[c];

    std::vector<int64_t> inShape = {1, 3, SZ, SZ};
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inTensor = Ort::Value::CreateTensor<float>(
        memInfo, input.data(), input.size(), inShape.data(), inShape.size());

    Ort::AllocatorWithDefaultOptions alloc;
    auto inName  = session->GetInputNameAllocated(0, alloc);
    auto outName = session->GetOutputNameAllocated(0, alloc);
    const char* inNames[]  = { inName.get()  };
    const char* outNames[] = { outName.get() };

    std::vector<Ort::Value> outputs;
    try {
        QElapsedTimer t; t.start();
        outputs = session->Run(Ort::RunOptions{nullptr},
                               inNames, &inTensor, 1, outNames, 1);
        LOG_INFO("AiTools",
            QString("RMBG inference: [1,3,%1,%1] in %2 ms").arg(SZ).arg(t.elapsed()));
    } catch (const Ort::Exception& e) {
        LOG_ERROR("AiTools",
            QString("RMBG inference failed: %1 — model: %2")
                .arg(e.what()).arg(m_modelPath));
        emit error("RMBG inference failed: " + QString::fromLatin1(e.what()));
        return {};
    }

    // ── Output diagnostics ────────────────────────────────────────────────────
    float* maskData = outputs[0].GetTensorMutableData<float>();
    auto outTypeInfo = outputs[0].GetTensorTypeAndShapeInfo();
    auto outShape = outTypeInfo.GetShape();  // concrete tensor — GetShape() is safe here

    QString shapeStr = "[";
    for (size_t i = 0; i < outShape.size(); i++) {
        if (i) shapeStr += ",";
        shapeStr += QString::number(outShape[i]);
    }
    shapeStr += "]";
    LOG_DEBUG("AiTools", QString("RMBG output shape: %1").arg(shapeStr));

    int totalElems = 1;
    for (auto d : outShape) totalElems *= (int)d;
    float vmin = *std::min_element(maskData, maskData + totalElems);
    float vmax = *std::max_element(maskData, maskData + totalElems);
    int aboveThresh = (int)std::count_if(maskData, maskData + totalElems,
                                         [](float v){ return v >= 0.5f; });
    LOG_DEBUG("AiTools",
        QString("RMBG mask: min=%1 max=%2 above0.5=%3/%4")
            .arg(vmin, 0, 'f', 4).arg(vmax, 0, 'f', 4)
            .arg(aboveThresh).arg(totalElems));

    // Mask dimensions from output shape (last two dims)
    int maskH = (outShape.size() >= 4) ? (int)outShape[outShape.size()-2] : SZ;
    int maskW = (outShape.size() >= 4) ? (int)outShape[outShape.size()-1] : SZ;

    // ── Build CImg<float> mask, invert, bilinear-resize to original dims ────────
    // RMBG-1.4 outputs high values for the BACKGROUND — invert to get foreground.
    cimg_library::CImg<float> floatMask(maskW, maskH, 1, 1);
    for (int y = 0; y < maskH; y++)
        for (int x = 0; x < maskW; x++)
            floatMask(x, y, 0, 0) = 1.0f - maskData[y * maskW + x];  // invert

    floatMask.resize(origW, origH, 1, 1, 3);  // bilinear interpolation

    // Log mask stats after inversion
    int fgPixels = 0;
    cimg_forXY(floatMask, x, y)
        if (floatMask(x, y) >= 0.5f) fgPixels++;
    LOG_DEBUG("AiTools",
        QString("RMBG mask (inverted): foreground pixels = %1/%2 (%3%)")
            .arg(fgPixels).arg(origW * origH)
            .arg(fgPixels * 100 / (origW * origH)));

    // ── Compose RGBA result ───────────────────────────────────────────────────
    // RGB channels: straight copy from original — no premultiplication.
    // Alpha: smooth ramp in the 0.3–0.7 transition band for clean edges.
    CImgU8 result(origW, origH, 1, 4, 0);
    for (int y = 0; y < origH; y++) {
        for (int x = 0; x < origW; x++) {
            float m = floatMask(x, y, 0, 0);
            float a;
            if      (m <= 0.3f) a = 0.f;
            else if (m >= 0.7f) a = 1.f;
            else                a = (m - 0.3f) / 0.4f;   // linear ramp

            result(x, y, 0, 0) = m_img(x, y, 0, 0);
            result(x, y, 0, 1) = (m_img.spectrum() > 1) ? m_img(x, y, 0, 1) : m_img(x, y, 0, 0);
            result(x, y, 0, 2) = (m_img.spectrum() > 2) ? m_img(x, y, 0, 2) : m_img(x, y, 0, 0);
            result(x, y, 0, 3) = (unsigned char)(a * 255.f);
        }
    }
    return result;
}

// ── Super Resolution ────────────────────────────────────────────────────────
// Real-ESRGAN tile-based model: fixed input [1,3,64,64] float32 [0,1],
// output [1,3,256,256] float32 [0,1].  We tile with 8-pixel overlap so that
// seam artefacts are hidden; only the inner STRIDE×STRIDE (48×48) region of
// each input tile contributes to the assembled output.

CImgU8 AiWorker::runSuperRes() {
    std::string err;
    auto session = OnnxEngine::instance().createSession(m_modelPath.toStdString(), &err);
    if (!session) {
        LOG_ERROR("AiTools", QString("SuperRes session failed: %1").arg(err.c_str()));
        emit error("Super-res model error: " + QString::fromStdString(err));
        return {};
    }

    constexpr int TILE_IN  = 64;             // model fixed input side
    constexpr int SCALE    = 4;              // upscale factor
    constexpr int TILE_OUT = TILE_IN * SCALE; // 256
    constexpr int OVERLAP  = 8;              // overlap pixels (each side)
    constexpr int STRIDE   = TILE_IN - 2 * OVERLAP;  // 48 — step in source space
    constexpr int SKIP_OUT = OVERLAP * SCALE;         // 32 — output pixels to discard per edge
    constexpr int INNER    = STRIDE * SCALE;          // 192 — usable output pixels per tile

    int origW = m_img.width(), origH = m_img.height();
    int outW  = origW * SCALE, outH = origH * SCALE;

    LOG_DEBUG("AiTools", QString("SuperRes: input tile [1,3,%1,%1] → [1,3,%2,%2], overlap=%3")
        .arg(TILE_IN).arg(TILE_OUT).arg(OVERLAP));
    LOG_DEBUG("AiTools", QString("SuperRes: original %1×%2 → output %3×%4")
        .arg(origW).arg(origH).arg(outW).arg(outH));

    // Ensure 3-channel source
    CImgU8 src3;
    if (m_img.spectrum() == 1)
        src3 = m_img.get_resize(-100, -100, 1, 3);
    else
        src3 = m_img.get_channels(0, std::min(2, m_img.spectrum() - 1));

    CImgU8 result(outW, outH, 1, 3, 0);
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::AllocatorWithDefaultOptions alloc;
    auto inNamePtr  = session->GetInputNameAllocated(0, alloc);
    auto outNamePtr = session->GetOutputNameAllocated(0, alloc);
    const char* inNames[]  = { inNamePtr.get()  };
    const char* outNames[] = { outNamePtr.get() };

    int numTilesX = (origW + STRIDE - 1) / STRIDE;
    int numTilesY = (origH + STRIDE - 1) / STRIDE;
    int totalTiles = numTilesX * numTilesY;
    int doneTiles  = 0;

    for (int ty = 0; ty < origH && !m_cancel; ty += STRIDE) {
        for (int tx = 0; tx < origW && !m_cancel; tx += STRIDE) {

            // Source patch with overlap (clamped to image bounds)
            int sx = std::max(0, tx - OVERLAP);
            int sy = std::max(0, ty - OVERLAP);
            int padL = tx - sx;   // actual left overlap this tile has
            int padT = ty - sy;   // actual top overlap this tile has

            CImgU8 patch = src3.get_crop(
                sx, sy, 0, 0,
                std::min(sx + TILE_IN - 1, origW - 1),
                std::min(sy + TILE_IN - 1, origH - 1),
                0, 2);

            // Pad to exactly TILE_IN×TILE_IN with mirror padding at edges
            if (patch.width() < TILE_IN || patch.height() < TILE_IN)
                patch.resize(TILE_IN, TILE_IN, 1, 3, 2);

            LOG_DEBUG("AiTools",
                QString("SuperRes tile (%1,%2) src=(%3,%4) padL=%5 padT=%6")
                    .arg(tx).arg(ty).arg(sx).arg(sy).arg(padL).arg(padT));

            // Build CHW float [0,1] tensor
            std::vector<float> input(3 * TILE_IN * TILE_IN);
            for (int c = 0; c < 3; c++)
                for (int y = 0; y < TILE_IN; y++)
                    for (int x = 0; x < TILE_IN; x++)
                        input[c * TILE_IN * TILE_IN + y * TILE_IN + x] =
                            patch(x, y, 0, c) / 255.0f;

            std::vector<int64_t> inShape = {1, 3, TILE_IN, TILE_IN};
            Ort::Value inTensor = Ort::Value::CreateTensor<float>(
                memInfo, input.data(), input.size(), inShape.data(), inShape.size());

            std::vector<Ort::Value> outputs;
            try {
                QElapsedTimer t; t.start();
                outputs = session->Run(Ort::RunOptions{nullptr},
                                       inNames, &inTensor, 1, outNames, 1);
                LOG_DEBUG("AiTools",
                    QString("SuperRes tile (%1,%2) inference in %3 ms")
                        .arg(tx).arg(ty).arg(t.elapsed()));
            } catch (const Ort::Exception& e) {
                LOG_ERROR("AiTools",
                    QString("SuperRes inference failed: %1 — model: %2")
                        .arg(e.what()).arg(m_modelPath));
                emit error("Super-res inference failed: " + QString::fromLatin1(e.what()));
                return {};
            }

            float* out = outputs[0].GetTensorMutableData<float>();

            // Which output pixels to use: skip the overlap margins, keep inner region
            int outSkipL = padL * SCALE;   // skip left  (= 0 for tx==0)
            int outSkipT = padT * SCALE;   // skip top   (= 0 for ty==0)
            int dstX     = tx * SCALE;
            int dstY     = ty * SCALE;
            int copyW    = std::min(INNER, outW - dstX);
            int copyH    = std::min(INNER, outH - dstY);

            for (int y = 0; y < copyH; y++)
                for (int x = 0; x < copyW; x++)
                    for (int c = 0; c < 3; c++) {
                        float v = out[c * TILE_OUT * TILE_OUT +
                                      (outSkipT + y) * TILE_OUT +
                                      (outSkipL + x)] * 255.f;
                        result(dstX + x, dstY + y, 0, c) =
                            (unsigned char)std::clamp((int)std::round(v), 0, 255);
                    }

            emit progress(++doneTiles * 100 / totalTiles);
        }
    }
    return result;
}

// ── Object Detection ────────────────────────────────────────────────────────

static float iou(QRectF a, QRectF b) {
    QRectF inter = a.intersected(b);
    if (inter.isEmpty()) return 0.f;
    float ai = inter.width() * inter.height();
    float au = a.width()*a.height() + b.width()*b.height() - ai;
    return au > 0 ? ai / au : 0.f;
}

void AiWorker::runDetect() {
    std::string err;
    auto session = OnnxEngine::instance().createSession(m_modelPath.toStdString(), &err);
    if (!session) {
        LOG_ERROR("AiTools", QString("YOLO session failed: %1").arg(err.c_str()));
        emit error("YOLO model error: " + QString::fromStdString(err));
        return;
    }

    const int SZ = 640;
    int origW = m_img.width(), origH = m_img.height();
    float scaleX = (float)origW / SZ, scaleY = (float)origH / SZ;

    std::vector<float> input = imgToTensorRGB(m_img, SZ, SZ);
    std::vector<int64_t> inShape = {1, 3, SZ, SZ};
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inTensor = Ort::Value::CreateTensor<float>(
        memInfo, input.data(), input.size(), inShape.data(), inShape.size());

    Ort::AllocatorWithDefaultOptions alloc;
    auto inName  = session->GetInputNameAllocated(0, alloc);
    auto outName = session->GetOutputNameAllocated(0, alloc);
    const char* inNames[]  = { inName.get()  };
    const char* outNames[] = { outName.get() };

    std::vector<Ort::Value> outputs;
    try {
        QElapsedTimer t; t.start();
        outputs = session->Run(Ort::RunOptions{nullptr},
                               inNames, &inTensor, 1, outNames, 1);
        LOG_INFO("AiTools",
            QString("YOLOv8 inference: [1,3,%1,%1] in %2 ms").arg(SZ).arg(t.elapsed()));
    } catch (const Ort::Exception& e) {
        LOG_ERROR("AiTools",
            QString("Detection inference failed: %1 — model: %2")
                .arg(e.what()).arg(m_modelPath));
        emit error("Detection inference failed: " + QString::fromLatin1(e.what()));
        return;
    }

    float* out = outputs[0].GetTensorMutableData<float>();
    auto outShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
    bool transposed  = (outShape[1] == 8400);
    int numBoxes     = transposed ? (int)outShape[1] : (int)outShape[2];
    int numAttribs   = transposed ? (int)outShape[2] : (int)outShape[1];

    auto getVal = [&](int box, int attr) -> float {
        return transposed ? out[box * numAttribs + attr]
                          : out[attr * numBoxes + box];
    };

    const float CONF_THRESH = 0.5f;
    const float IOU_THRESH  = 0.45f;

    struct Det { QRectF rect; float conf; int cls; };
    QVector<Det> dets;
    for (int b = 0; b < numBoxes; b++) {
        float cx = getVal(b, 0) * scaleX, cy = getVal(b, 1) * scaleY;
        float w  = getVal(b, 2) * scaleX, h  = getVal(b, 3) * scaleY;
        float bestConf = 0; int bestCls = 0;
        for (int c = 4; c < numAttribs && c < 4+COCO_COUNT; c++) {
            float s = getVal(b, c);
            if (s > bestConf) { bestConf = s; bestCls = c - 4; }
        }
        if (bestConf < CONF_THRESH) continue;
        dets.push_back({ QRectF(cx - w/2, cy - h/2, w, h), bestConf, bestCls });
    }

    std::sort(dets.begin(), dets.end(), [](const Det& a, const Det& b){
        return a.conf > b.conf;
    });
    QVector<bool> suppressed(dets.size(), false);
    QVector<QPair<QRectF,QString>> results;
    for (int i = 0; i < dets.size(); i++) {
        if (suppressed[i]) continue;
        const auto& d = dets[i];
        QString label = (d.cls < COCO_COUNT) ? COCO_CLASSES[d.cls] : "?";
        results.push_back({ d.rect, label });
        for (int j = i+1; j < dets.size(); j++)
            if (!suppressed[j] && dets[i].cls == dets[j].cls &&
                iou(d.rect, dets[j].rect) > IOU_THRESH)
                suppressed[j] = true;
    }
    LOG_INFO("AiTools", QString("YOLOv8: %1 detections after NMS").arg(results.size()));
    emit objectsDetected(results);
}

// ── Neural Style Transfer (ONNX Model Zoo fast_neural_style) ──────────────
// Models: candy-8 / mosaic-8 / pointilism-8 / rain-princess-8
// Input:  [1, 3, 224, 224]  float32  values in [0, 255]  (NO normalisation)
// Output: [1, 3, 224, 224]  float32  values in [0, 255]  (clamp + convert)
// After inference the 224×224 result is bilinearly upscaled to original size.

CImgU8 AiWorker::runNeuralStyle() {
    std::string err;
    auto session = OnnxEngine::instance().createSession(m_modelPath.toStdString(), &err);
    if (!session) {
        LOG_ERROR("AiTools",
            QString("NeuralStyle session failed: %1 — model: %2")
                .arg(err.c_str()).arg(m_modelPath));
        emit error("Neural style model error: " + QString::fromStdString(err));
        return {};
    }

    constexpr int SZ = 224;   // fast_neural_style fixed input size
    int origW = m_img.width(), origH = m_img.height();

    LOG_DEBUG("AiTools", QString("NeuralStyle: expected input [1,3,%1,%1]").arg(SZ));
    LOG_DEBUG("AiTools", QString("NeuralStyle: original image %1×%2").arg(origW).arg(origH));
    LOG_DEBUG("AiTools", QString("NeuralStyle: resizing to %1×%1").arg(SZ));

    // Ensure 3-channel source then resize to SZ×SZ
    CImgU8 src = (m_img.spectrum() == 1)
        ? m_img.get_resize(-100, -100, 1, 3)
        : m_img.get_channels(0, std::min(2, m_img.spectrum() - 1));

    CImgU8 resized = ImageProcessor::resize(src, SZ, SZ);

    // Build CHW float tensor [0, 255] — no normalisation
    std::vector<float> input(3 * SZ * SZ);
    for (int c = 0; c < 3; c++)
        for (int y = 0; y < SZ; y++)
            for (int x = 0; x < SZ; x++)
                input[c * SZ * SZ + y * SZ + x] = (float)resized(x, y, 0, c);

    std::vector<int64_t> inShape = {1, 3, SZ, SZ};
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inTensor = Ort::Value::CreateTensor<float>(
        memInfo, input.data(), input.size(), inShape.data(), inShape.size());

    Ort::AllocatorWithDefaultOptions alloc;
    auto inName  = session->GetInputNameAllocated(0, alloc);
    auto outName = session->GetOutputNameAllocated(0, alloc);
    const char* inNames[]  = { inName.get()  };
    const char* outNames[] = { outName.get() };

    std::vector<Ort::Value> outputs;
    try {
        QElapsedTimer t; t.start();
        outputs = session->Run(Ort::RunOptions{nullptr},
                               inNames, &inTensor, 1, outNames, 1);
        LOG_INFO("AiTools",
            QString("NeuralStyle inference: [1,3,%1,%1] in %2 ms").arg(SZ).arg(t.elapsed()));
    } catch (const Ort::Exception& e) {
        LOG_ERROR("AiTools",
            QString("NeuralStyle inference failed: %1 — model: %2")
                .arg(e.what()).arg(m_modelPath));
        emit error("Neural style inference failed: " + QString::fromLatin1(e.what()));
        return {};
    }

    // Output is [1, 3, 224, 224] float32 [0, 255]
    float* outData = outputs[0].GetTensorMutableData<float>();
    CImgU8 styled(SZ, SZ, 1, 3);
    for (int c = 0; c < 3; c++)
        for (int y = 0; y < SZ; y++)
            for (int x = 0; x < SZ; x++) {
                float v = outData[c * SZ * SZ + y * SZ + x];
                styled(x, y, 0, c) = (unsigned char)std::clamp((int)std::round(v), 0, 255);
            }

    // Bilinear upscale back to original dimensions
    if (origW != SZ || origH != SZ)
        return ImageProcessor::resize(styled, origW, origH);
    return styled;
}

// ── AiTools facade ─────────────────────────────────────────────────────────
//
// Thread lifecycle (PROBLEMA B fix):
//   thread->finished  →  worker->deleteLater()
//   thread->finished  →  thread->deleteLater()
//
// The error/finished lambdas only call thread->quit() — cleanup happens
// automatically after the thread finishes, avoiding use-after-free.

void AiTools::runOp(AiWorker::Operation op, const CImgU8& img,
                    const QString& modelId, QWidget* parent,
                    std::function<void(CImgU8, QString)> onDone,
                    std::function<void(QVector<QPair<QRectF,QString>>)> onDetected,
                    std::function<void(QString)> onError)
{
    if (!ModelManager::isDownloaded(modelId)) {
        QString msg = QString("Model '%1' not downloaded. Use AI > Manage Models.").arg(modelId);
        LOG_WARNING("AiTools", msg);
        onError(msg);
        return;
    }

    // Validate model file before starting the thread — avoids crash inside worker
    QString modelFilePath = ModelManager::modelPath(modelId);
    qint64 expectedMB = 0;
    for (const auto& m : ModelManager::availableModels())
        if (m.id == modelId) { expectedMB = m.expectedSizeMB; break; }

    QString validErr;
    if (!ModelManager::validateModel(modelFilePath, expectedMB, &validErr)) {
        LOG_ERROR("AiTools",
            QString("Model validation failed for '%1': %2").arg(modelId).arg(validErr));
        onError("Model file invalid — please re-download.\n" + validErr);
        return;
    }

    LOG_INFO("AiTools",
        QString("Starting '%1' on %2×%3").arg(modelId).arg(img.width()).arg(img.height()));

    auto* worker = new AiWorker(op, img, modelFilePath);
    auto* thread = new QThread;
    worker->moveToThread(thread);

    // Guaranteed cleanup: once the thread finishes, free both objects.
    QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    auto* progress = new QProgressDialog(
        QString("AI: running %1…").arg(modelId), "Cancel", 0, 0, parent);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(300);
    progress->show();

    QObject::connect(thread,   &QThread::started,   worker, &AiWorker::run);
    QObject::connect(worker,   &AiWorker::progress, progress, [progress](int p) {
        progress->setMaximum(100);
        progress->setValue(p);
    });
    QObject::connect(progress, &QProgressDialog::canceled,
                     worker,   &AiWorker::requestCancel);

    QWidget* ctx = parent ? parent : qApp->activeWindow();

    QObject::connect(worker, &AiWorker::finished, ctx,
                     [=](CImgU8 r, QString name) {
        progress->close();
        progress->deleteLater();
        thread->quit();   // triggers thread->finished → deleteLater chain
        onDone(r, name);
    }, Qt::QueuedConnection);

    QObject::connect(worker, &AiWorker::objectsDetected, ctx,
                     [=](QVector<QPair<QRectF,QString>> boxes) {
        progress->close();
        progress->deleteLater();
        thread->quit();
        if (onDetected) onDetected(boxes);
    }, Qt::QueuedConnection);

    QObject::connect(worker, &AiWorker::error, ctx,
                     [=](QString msg) {
        progress->close();
        progress->deleteLater();
        thread->quit();
        onError(msg);
    }, Qt::QueuedConnection);

    thread->start();
}

void AiTools::removeBackground(const CImgU8& img, QWidget* parent,
    std::function<void(CImgU8)> onDone,
    std::function<void(QString)> onError)
{
    runOp(AiWorker::RemoveBg, img, "rmbg", parent,
          [onDone](CImgU8 r, QString) { onDone(r); },
          nullptr, onError);
}

void AiTools::superResolution(const CImgU8& img, QWidget* parent,
    std::function<void(CImgU8)> onDone,
    std::function<void(QString)> onError)
{
    runOp(AiWorker::SuperRes, img, "realesrgan", parent,
          [onDone](CImgU8 r, QString) { onDone(r); },
          nullptr, onError);
}

void AiTools::detectObjects(const CImgU8& img, QWidget* parent,
    std::function<void(QVector<QPair<QRectF,QString>>)> onDone,
    std::function<void(QString)> onError)
{
    runOp(AiWorker::Detect, img, "yolov8n", parent,
          [](CImgU8, QString) {},
          onDone, onError);
}

void AiTools::styleTransfer(const CImgU8& img, const QString& styleId, QWidget* parent,
    std::function<void(CImgU8)> onDone,
    std::function<void(QString)> onError)
{
    runOp(AiWorker::NeuralStyle, img, styleId, parent,
          [onDone](CImgU8 r, QString) { onDone(r); },
          nullptr, onError);
}
