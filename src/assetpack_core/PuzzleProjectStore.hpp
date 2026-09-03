#pragma once

#include "Model.hpp"
#include "CsvCodec.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace AssetPackCore {

class PuzzleProjectStore {
public:
    static bool LoadDataFolder(const std::filesystem::path& selectedDataFolder,
                              PuzzleProject& project,
                              std::vector<ProjectIssue>& issues);

    static bool SaveAll(PuzzleProject& project,
                        std::vector<ProjectIssue>& issues);

    static void BuildIssuesFromException(std::vector<ProjectIssue>& issues,
                                        const std::string& file,
                                        const std::string& message);
};

} // namespace AssetPackCore
