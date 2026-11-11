#include "OnnxInference.h"
#include <cmath>
#include <filesystem>
#include <array>

#ifdef SBA_HAS_ONNX
#include <onnxruntime_cxx_api.h>
#endif

namespace sba {

#ifdef SBA_HAS_ONNX
struct OnnxInference::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "soccer_bet_adviser"};
    std::unique_ptr<Ort::Session> session;
    std::string inputName;
    std::vector<std::string> outputNames;
    size_t features = 0;
    int probIndex = -1;
};

OnnxInference::OnnxInference() : m_impl(new Impl) {}
OnnxInference::~OnnxInference() = default;
bool OnnxInference::compiledWithOnnx() { return true; }
bool OnnxInference::isLoaded() const { return m_impl && m_impl->session != nullptr; }

// ONNX Runtime is initialised here: creates the Ort::Session for the Random Forest model,
// checks the input width and finds the 'probabilities' output. Called once from PredictionService::initialise().
bool OnnxInference::load(const std::string& path, size_t expectedFeatures, std::string& error) {
    try {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);
#ifdef _WIN32
        const std::wstring wpath = std::filesystem::u8path(path).wstring();   // UTF-8 -> UTF-16 for Windows
        m_impl->session = std::make_unique<Ort::Session>(m_impl->env, wpath.c_str(), opts);
#else
        m_impl->session = std::make_unique<Ort::Session>(m_impl->env, path.c_str(), opts);
#endif
        Ort::AllocatorWithDefaultOptions alloc;
        if (m_impl->session->GetInputCount() != 1) { error = "Model must have exactly one input"; m_impl->session.reset(); return false; }
        m_impl->inputName = m_impl->session->GetInputNameAllocated(0, alloc).get();
        auto shape = m_impl->session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        if (shape.size() != 2 || (shape[1] > 0 && static_cast<size_t>(shape[1]) != expectedFeatures)) {
            error = "Model expects a different number of features than the application provides";
            m_impl->session.reset();
            return false;
        }
        m_impl->features = expectedFeatures;
        m_impl->outputNames.clear();
        m_impl->probIndex = -1;
        for (size_t i = 0; i < m_impl->session->GetOutputCount(); ++i) {
            std::string n = m_impl->session->GetOutputNameAllocated(i, alloc).get();
            if (n == "probabilities") m_impl->probIndex = static_cast<int>(i);
            m_impl->outputNames.push_back(n);
        }
        if (m_impl->probIndex < 0) { error = "Model has no 'probabilities' output"; m_impl->session.reset(); return false; }
        return true;
    } catch (const std::exception& e) {
        error = std::string("ONNX Runtime error: ") + e.what();
        m_impl->session.reset();
        return false;
    }
}

bool OnnxInference::predictBatch(const std::vector<std::vector<float>>& rows, std::vector<Probs3>& out, std::string& error) const {
    out.clear();
    if (!isLoaded()) { error = "ONNX model is not loaded"; return false; }
    if (rows.empty()) return true;
    try {
        const size_t n = rows.size(), f = m_impl->features;
        std::vector<float> flat(n * f);
        for (size_t i = 0; i < n; ++i) {
            if (rows[i].size() != f) { error = "Feature vector has the wrong length"; return false; }
            std::copy(rows[i].begin(), rows[i].end(), flat.begin() + i * f);
        }
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<int64_t, 2> shape{static_cast<int64_t>(n), static_cast<int64_t>(f)};
        Ort::Value input = Ort::Value::CreateTensor<float>(mem, flat.data(), flat.size(), shape.data(), shape.size());
        const char* inNames[] = {m_impl->inputName.c_str()};
        std::vector<const char*> outNames;
        for (const auto& s : m_impl->outputNames) outNames.push_back(s.c_str());
        auto result = m_impl->session->Run(Ort::RunOptions{nullptr}, inNames, &input, 1, outNames.data(), outNames.size());
        auto& pv = result[static_cast<size_t>(m_impl->probIndex)];
        auto info = pv.GetTensorTypeAndShapeInfo();
        auto s = info.GetShape();
        if (s.size() != 2 || s[0] != static_cast<int64_t>(n) || s[1] != 3) { error = "Unexpected probability tensor shape"; return false; }
        const float* p = pv.GetTensorData<float>();
        for (size_t i = 0; i < n; ++i) {
            Probs3 r{p[i * 3], p[i * 3 + 1], p[i * 3 + 2]};
            for (double v : r) if (!std::isfinite(v) || v < -1e-6 || v > 1.0 + 1e-6) { error = "Model returned invalid probabilities"; return false; }
            out.push_back(normalised(r));
        }
        return true;
    } catch (const std::exception& e) {
        error = std::string("ONNX inference failed: ") + e.what();
        return false;
    }
}
#else   // ---- built without ONNX Runtime -------------------------------------------------
struct OnnxInference::Impl {};
OnnxInference::OnnxInference() : m_impl(new Impl) {}
OnnxInference::~OnnxInference() = default;
bool OnnxInference::compiledWithOnnx() { return false; }
bool OnnxInference::isLoaded() const { return false; }
bool OnnxInference::load(const std::string&, size_t, std::string& error) {
    error = "This build does not include ONNX Runtime (configure with -DONNXRUNTIME_ROOT=... or allow the automatic download)";
    return false;
}
bool OnnxInference::predictBatch(const std::vector<std::vector<float>>&, std::vector<Probs3>&, std::string& error) const {
    error = "ONNX Runtime not available";
    return false;
}
#endif

bool OnnxInference::predict(const std::vector<float>& features, Probs3& out, std::string& error) const {
    std::vector<Probs3> r;
    if (!predictBatch({features}, r, error) || r.empty()) return false;
    out = r.front();
    return true;
}

}  // namespace sba
