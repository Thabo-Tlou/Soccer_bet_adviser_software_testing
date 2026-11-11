#pragma once
// Locates the dataset and model files at run time (installed app or source tree).
#include <string>
#include <vector>

namespace sba {

struct ResourcePaths {
    std::string root;             // folder that contains data/ and models/
    std::string dataCsv, onnxModel, poissonJson, decisionConfig;
    std::vector<std::string> updateCsvs;   // newer results (e.g. downloaded from football-data.co.uk) merged into the dataset at start-up
    bool complete() const { return !dataCsv.empty() && !poissonJson.empty() && !decisionConfig.empty(); }
    // The ONNX file is optional (Poisson-only mode) so it is not part of complete().
};

// Searches, in order: $SBA_RESOURCES, <exeDir>/resources, <exeDir>/../resources,
// <sourceRoot>/ml_training, ./ml_training.
ResourcePaths locateResources(const std::string& exeDir, const std::string& sourceRoot);

}  // namespace sba
