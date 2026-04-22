#include "OnnxEngine.hpp"
#include "Logger.hpp"
#include <QElapsedTimer>
#include <QString>
// GetDimensions() is deprecated in favour of GetShape(), but GetShape() throws
// std::length_error for models with symbolic dimensions in ORT 1.17.1 — we use
// GetDimensions() with an explicit sanity check on GetDimensionsCount() instead.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

OnnxEngine::OnnxEngine()
    : m_env(ORT_LOGGING_LEVEL_ERROR, "CImgViewer")
{}

OnnxEngine& OnnxEngine::instance() {
    static OnnxEngine inst;
    return inst;
}

void OnnxEngine::initialize() {
    if (m_initialized) return;
    m_initialized = true;

    LOG_INFO("OnnxEngine", "Initializing ONNX Runtime 1.17.1");

    // Probe CUDA by trying to append CUDAExecutionProvider.
    // On CPU-only builds or when cuDNN is absent this throws.
    Ort::SessionOptions probe;
    try {
        OrtCUDAProviderOptions cudaOpts{};
        cudaOpts.device_id = 0;
        probe.AppendExecutionProvider_CUDA(cudaOpts);
        m_hasCuda = true;
    } catch (...) {
        m_hasCuda = false;
        LOG_WARNING("OnnxEngine",
            "CUDA provider não disponível — ONNX Runtime instalado é CPU-only. "
            "Para habilitar GPU (RTX 3060), veja README seção 'Habilitando CUDA'.");
    }

    LOG_INFO("OnnxEngine",
        QString("Active execution provider: %1").arg(providerName()));
}

std::unique_ptr<Ort::Session> OnnxEngine::createSession(
        const std::string& modelPath, std::string* outError)
{
    if (!m_initialized) initialize();

    QElapsedTimer timer;
    timer.start();

    try {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(4);
        opts.SetGraphOptimizationLevel(ORT_ENABLE_EXTENDED);

        if (m_hasCuda) {
            try {
                OrtCUDAProviderOptions cudaOpts{};
                cudaOpts.device_id = 0;
                opts.AppendExecutionProvider_CUDA(cudaOpts);
            } catch (const std::exception& e) {
                m_hasCuda = false;
                LOG_WARNING("OnnxEngine",
                    QString("CUDA provider failed for session (%1) — downgrading to CPU")
                        .arg(e.what()));
            }
        }

#ifdef _WIN32
        // On Windows, ORTCHAR_T is wchar_t — convert the path
        std::wstring wPath(modelPath.begin(), modelPath.end());
        auto session = std::make_unique<Ort::Session>(m_env, wPath.c_str(), opts);
#else
        auto session = std::make_unique<Ort::Session>(m_env, modelPath.c_str(), opts);
#endif

        // Log input/output shapes — isolated in its own try so that ORT 1.17.1's
        // GetShape() bug (throws std::length_error for symbolic/dynamic dims in
        // older ONNX IR models) does not kill the successfully created session.
        QString shapeInfo;
        try {
            Ort::AllocatorWithDefaultOptions alloc;

            auto safeShape = [](auto& info) -> QString {
                size_t ndim = info.GetDimensionsCount();
                if (ndim == 0 || ndim > 32) return "[?]";
                std::vector<int64_t> dims(ndim);
                info.GetDimensions(dims.data(), ndim);
                QString s = "[";
                for (size_t j = 0; j < ndim; j++) {
                    if (j) s += ",";
                    s += (dims[j] < 0) ? QString("?") : QString::number(dims[j]);
                }
                return s + "]";
            };

            for (size_t i = 0; i < session->GetInputCount(); i++) {
                auto name = session->GetInputNameAllocated(i, alloc);
                auto info = session->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo();
                shapeInfo += QString("in%1=%2 ").arg(i).arg(safeShape(info));
            }
            for (size_t i = 0; i < session->GetOutputCount(); i++) {
                auto name = session->GetOutputNameAllocated(i, alloc);
                auto info = session->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo();
                shapeInfo += QString("out%1=%2 ").arg(i).arg(safeShape(info));
            }
        } catch (...) {
            shapeInfo = "(shape query failed)";
        }

        LOG_INFO("OnnxEngine",
            QString("Session created for '%1' in %2 ms — shapes: %3")
                .arg(QString::fromStdString(modelPath))
                .arg(timer.elapsed())
                .arg(shapeInfo.trimmed()));

        return session;

    } catch (const Ort::Exception& e) {
        QString msg = QString::fromLatin1(e.what());
        LOG_ERROR("OnnxEngine",
            QString("Failed to create session for '%1': %2")
                .arg(QString::fromStdString(modelPath))
                .arg(msg));
        if (outError) *outError = e.what();
        return nullptr;
    } catch (const std::exception& e) {
        QString msg = QString::fromLatin1(e.what());
        LOG_ERROR("OnnxEngine",
            QString("Exception creating session for '%1': %2")
                .arg(QString::fromStdString(modelPath))
                .arg(msg));
        if (outError) *outError = e.what();
        return nullptr;
    }
}
