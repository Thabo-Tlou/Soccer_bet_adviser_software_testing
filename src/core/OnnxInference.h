#pragma once
// Random-Forest inference through ONNX Runtime. When the project is built without
// ONNX Runtime (SBA_HAS_ONNX undefined) load() fails gracefully and the app
// continues with the Poisson model only.
#include <memory>
#include <string>
#include <vector>
#include "Types.h"

namespace sba {

class OnnxInference {
public:
    OnnxInference();
    ~OnnxInference();
    OnnxInference(const OnnxInference&) = delete;
    OnnxInference& operator=(const OnnxInference&) = delete;

    static bool compiledWithOnnx();
    bool load(const std::string& modelPath, size_t expectedFeatures, std::string& error);
    bool isLoaded() const;

    // Class order is Home, Draw, Away. NaN inputs are imputed by the model itself.
    bool predict(const std::vector<float>& features, Probs3& out, std::string& error) const;
    bool predictBatch(const std::vector<std::vector<float>>& rows, std::vector<Probs3>& out, std::string& error) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace sba
