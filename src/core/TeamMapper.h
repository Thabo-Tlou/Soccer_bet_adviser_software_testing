#pragma once
// Maps provider team names ("Manchester City FC") to dataset names ("Man City").
#include <string>
#include <vector>

namespace sba {
std::string mapTeamName(const std::string& providerName, const std::vector<std::string>& datasetTeams);
std::string normaliseTeamName(const std::string& s);
}
