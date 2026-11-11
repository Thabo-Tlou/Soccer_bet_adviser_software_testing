#include "Resources.h"
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace sba {

ResourcePaths locateResources(const std::string& exeDir, const std::string& sourceRoot) {
    std::vector<fs::path> roots;
    if (const char* env = std::getenv("SBA_RESOURCES")) roots.push_back(env);
    roots.push_back(fs::path(exeDir) / "resources");
    roots.push_back(fs::path(exeDir) / ".." / "resources");
    if (!sourceRoot.empty()) roots.push_back(fs::path(sourceRoot) / "ml_training");
    roots.push_back(fs::path("ml_training"));

    for (const auto& r : roots) {
        std::error_code ec;
        const fs::path csv = r / "data" / "EPL_merged_2017-2027.csv";
        const fs::path cfg = r / "models" / "epl_decision_config.json";
        if (fs::exists(csv, ec) && fs::exists(cfg, ec)) {
            ResourcePaths p;
            p.root = fs::weakly_canonical(r, ec).string();
            p.dataCsv = csv.string();
            p.decisionConfig = cfg.string();
            p.poissonJson = (r / "models" / "poisson_model.json").string();
            p.onnxModel = (r / "models" / "random_forest_epl.onnx").string();
            if (!fs::exists(p.poissonJson, ec)) p.poissonJson.clear();
            if (!fs::exists(p.onnxModel, ec)) p.onnxModel.clear();
            return p;
        }
    }
    return {};
}

}  // namespace sba
