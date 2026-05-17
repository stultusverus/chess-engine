#include "tune.h"
#include "board.h"
#include "eval.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace chess {

static double sigmoid(double x) {
    return 1.0 / (1.0 + std::exp(-x));
}

static std::string trim(std::string s) {
    auto first = s.find_first_not_of(" \t");
    if (first == std::string::npos)
        return "";
    auto last = s.find_last_not_of(" \t");
    return s.substr(first, last - first + 1);
}

std::optional<TuningDataset> TuningDataset::load(const std::string& path, bool strict) {
    std::ifstream file(path);
    if (!file.is_open())
        return std::nullopt;

    TuningDataset dataset;
    std::string line;
    int lineNum = 0;

    while (std::getline(file, line)) {
        lineNum++;

        // Normalise line endings and whitespace
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        line = trim(line);
        for (auto& c : line)
            if (c == '\t') c = ' ';

        // Skip blank lines and comments
        if (line.empty() || line[0] == '#')
            continue;

        // Find the last space — everything after it is the result token
        auto pos = line.rfind(' ');
        if (pos == std::string::npos) {
            std::ostringstream oss;
            oss << "line " << lineNum << ": missing result token";
            dataset.warnings.push_back(oss.str());
            if (strict) return std::nullopt;
            continue;
        }

        std::string fen = line.substr(0, pos);
        std::string result = line.substr(pos + 1);

        // Validate result
        double target;
        if (result == "1-0")
            target = 1.0;
        else if (result == "1/2-1/2")
            target = 0.5;
        else if (result == "0-1")
            target = 0.0;
        else {
            std::ostringstream oss;
            oss << "line " << lineNum << ": invalid result '" << result << "'";
            dataset.warnings.push_back(oss.str());
            if (strict) return std::nullopt;
            continue;
        }

        // Validate FEN
        Board board;
        if (!board.setFen(fen)) {
            std::ostringstream oss;
            oss << "line " << lineNum << ": invalid FEN";
            dataset.warnings.push_back(oss.str());
            if (strict) return std::nullopt;
            continue;
        }

        dataset.entries.push_back({fen, target});
    }

    return dataset;
}

double computeLoss(const TuningDataset& dataset, const EvalParams& params, double k) {
    if (dataset.entries.empty())
        return 0.0;

    Eval eval;

    double totalSqError = 0.0;
    for (const auto& entry : dataset.entries) {
        Board board;
        board.setFen(entry.fen);
        int score = eval.evaluate(board, params);
        double p = sigmoid(k * score / 100.0);
        double err = entry.target - p;
        totalSqError += err * err;
    }

    return totalSqError / static_cast<double>(dataset.entries.size());
}

KOptimizeResult optimizeK(const TuningDataset& dataset, const EvalParams& params) {
    constexpr double phi = 1.618033988749895;
    constexpr int iterations = 30;

    double a = 0.1;
    double b = 5.0;
    double c = b - (b - a) / phi;
    double d = a + (b - a) / phi;
    double fc = computeLoss(dataset, params, c);
    double fd = computeLoss(dataset, params, d);

    for (int i = 0; i < iterations; i++) {
        if (fc < fd) {
            b = d;
            d = c;
            fd = fc;
            c = b - (b - a) / phi;
            fc = computeLoss(dataset, params, c);
        } else {
            a = c;
            c = d;
            fc = fd;
            d = a + (b - a) / phi;
            fd = computeLoss(dataset, params, d);
        }
    }

    double bestK = (a + b) / 2.0;

    KOptimizeResult result;
    result.baselineK = 1.0;
    result.baselineLoss = computeLoss(dataset, params, 1.0);
    result.bestK = bestK;
    result.bestLoss = computeLoss(dataset, params, bestK);
    return result;
}

TuningDatasetSummary summarizeDataset(const std::string& path) {
    TuningDatasetSummary summary;
    std::ifstream file(path);
    if (!file.is_open())
        return summary;

    std::string line;
    std::unordered_set<std::string> seenFens;

    while (std::getline(file, line)) {
        summary.totalLines++;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        line = trim(line);
        for (auto& c : line)
            if (c == '\t') c = ' ';

        if (line.empty() || line[0] == '#') {
            summary.skippedBlankComment++;
            continue;
        }

        auto pos = line.rfind(' ');
        if (pos == std::string::npos) {
            summary.invalidResultCount++;
            summary.warningCount++;
            continue;
        }

        std::string fen = line.substr(0, pos);
        std::string result = line.substr(pos + 1);

        // Validate result token first, then FEN. Only count result
        // distribution after both are valid so summary fields stay consistent.
        int resultType = -1; // 0=loss, 1=draw, 2=win
        if (result == "0-1")
            resultType = 0;
        else if (result == "1/2-1/2")
            resultType = 1;
        else if (result == "1-0")
            resultType = 2;
        else {
            summary.invalidResultCount++;
            summary.warningCount++;
            continue;
        }

        Board board;
        if (!board.setFen(fen)) {
            summary.invalidFenCount++;
            summary.warningCount++;
            continue;
        }

        // Both FEN and result are valid — now update distribution counters
        if (resultType == 0) summary.resultLoss++;
        else if (resultType == 1) summary.resultDraw++;
        else summary.resultWin++;

        if (board.sideToMove() == WHITE)
            summary.sideWhite++;
        else
            summary.sideBlack++;

        if (seenFens.count(fen))
            summary.duplicateFenCount++;
        seenFens.insert(fen);

        summary.parsedPositions++;
    }

    return summary;
}

} // namespace chess
