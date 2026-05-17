#pragma once

#include <optional>
#include <string>
#include <vector>

namespace chess {

struct EvalParams;

struct TuningEntry {
    std::string fen;
    double target; // 1.0 for 1-0, 0.5 for 1/2-1/2, 0.0 for 0-1
};

struct TuningDataset {
    std::vector<TuningEntry> entries;
    std::vector<std::string> warnings;

    static std::optional<TuningDataset> load(const std::string& path, bool strict = false);
};

struct KOptimizeResult {
    double baselineK;
    double baselineLoss;
    double bestK;
    double bestLoss;
};

struct TuningDatasetSummary {
    int totalLines = 0;
    int parsedPositions = 0;
    int skippedBlankComment = 0;
    int invalidFenCount = 0;
    int invalidResultCount = 0;
    int warningCount = 0;
    int resultWin = 0;   // 1-0
    int resultDraw = 0;  // 1/2-1/2
    int resultLoss = 0;  // 0-1
    int sideWhite = 0;
    int sideBlack = 0;
    int duplicateFenCount = 0;
};

double computeLoss(const TuningDataset& dataset, const EvalParams& params, double k);

KOptimizeResult optimizeK(const TuningDataset& dataset, const EvalParams& params);

TuningDatasetSummary summarizeDataset(const std::string& path);

} // namespace chess
