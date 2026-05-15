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

double computeLoss(const TuningDataset& dataset, const EvalParams& params, double k);

KOptimizeResult optimizeK(const TuningDataset& dataset, const EvalParams& params);

} // namespace chess
