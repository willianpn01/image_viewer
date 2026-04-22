#pragma once
#include <onnxruntime_cxx_api.h>
#include <QString>
#include <string>
#include <memory>

// Singleton managing the global ONNX Runtime environment and session options.
// Detects CUDA availability at init time; falls back to CPU silently.
class OnnxEngine {
public:
    static OnnxEngine& instance();

    // Must be called once before creating sessions.
    void initialize();

    bool isCudaAvailable() const { return m_hasCuda; }
    QString providerName() const { return m_hasCuda ? "CUDA (RTX 3060)" : "CPU"; }

    // Create a session for the given model file.
    // Returns nullptr on error (caller should show a message).
    std::unique_ptr<Ort::Session> createSession(const std::string& modelPath,
                                                std::string* outError = nullptr);

    Ort::Env& env() { return m_env; }

private:
    OnnxEngine();
    OnnxEngine(const OnnxEngine&) = delete;

    Ort::Env m_env;
    bool m_hasCuda     = false;
    bool m_initialized = false;
};
