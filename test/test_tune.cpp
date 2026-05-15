#include "engine/attacks.h"
#include "engine/board.h"
#include "engine/eval.h"
#include "engine/tune.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static int failures = 0;

#define CHECK(expr) do { \
    if (!(expr)) { \
        std::cerr << "FAIL: " << #expr << std::endl; \
        failures++; \
    } \
} while (0)

#define CHECK_CLOSE(a, b, tol) do { \
    if (std::abs((a) - (b)) > (tol)) { \
        std::cerr << "FAIL: " << #a << "=" << (a) << " != " << #b << "=" << (b) \
                  << " (tol=" << tol << ")" << std::endl; \
        failures++; \
    } \
} while (0)

#define RUN_TEST(name) do { \
    int before = failures; \
    test_##name(); \
    if (failures == before) \
        std::cout << "  " #name ": PASSED" << std::endl; \
} while (0)

// ---------- parser tests ----------

static void test_parse_valid_lines() {
    // Non-strict: valid lines are loaded, no warnings
    std::string tmpPath = "test_tune_valid.tmp";
    {
        std::ofstream out(tmpPath);
        out << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 1/2-1/2\n";
        out << "# comment line\n";
        out << "\n";
        out << "8/8/8/4k3/4P3/4K3/8/8 w - - 0 1 1-0\n";
        out << "8/8/8/4k3/4p3/4K3/8/8 w - - 0 1 0-1\n";
    }
    auto ds = chess::TuningDataset::load(tmpPath, false);
    CHECK(ds.has_value());
    CHECK(ds->warnings.empty());
    CHECK(ds->entries.size() == 3);
    CHECK(ds->entries[0].target == 0.5);
    CHECK(ds->entries[1].target == 1.0);
    CHECK(ds->entries[2].target == 0.0);
    std::remove(tmpPath.c_str());
}

static void test_parse_strict_mode() {
    // Strict mode returns nullopt on malformed lines
    std::string tmpPath = "test_tune_strict.tmp";
    {
        std::ofstream out(tmpPath);
        out << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 1/2-1/2\n";
        out << "bad line no result\n";
    }
    auto ds = chess::TuningDataset::load(tmpPath, true);
    CHECK(!ds.has_value());
    std::remove(tmpPath.c_str());
}

static void test_parse_nonstrict_collects_warnings() {
    // Non-strict mode loads valid lines and warns on bad lines
    std::string tmpPath = "test_tune_warn.tmp";
    {
        std::ofstream out(tmpPath);
        out << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 1-0\n";
        out << "garbage line with no result\n";
        out << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 invalid-result\n";
    }
    auto ds = chess::TuningDataset::load(tmpPath, false);
    CHECK(ds.has_value());
    CHECK(ds->entries.size() == 1);
    CHECK(ds->warnings.size() == 2);
    std::remove(tmpPath.c_str());
}

static void test_parse_invalid_fen_rejected() {
    std::string tmpPath = "test_tune_badfen.tmp";
    {
        std::ofstream out(tmpPath);
        out << "not a fen string 1-0\n";
    }
    auto ds = chess::TuningDataset::load(tmpPath, false);
    CHECK(ds.has_value());
    CHECK(ds->entries.empty());
    CHECK(ds->warnings.size() == 1);
    std::remove(tmpPath.c_str());
}

static void test_parse_file_not_found() {
    auto ds = chess::TuningDataset::load("nonexistent_file.txt", false);
    CHECK(!ds.has_value());
}

static void test_parse_carriage_return() {
    // Lines ending with \r\n should be parsed correctly
    std::string tmpPath = "test_tune_cr.tmp";
    {
        std::ofstream out(tmpPath);
        out << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 1-0\r\n";
    }
    auto ds = chess::TuningDataset::load(tmpPath, false);
    CHECK(ds.has_value());
    CHECK(ds->entries.size() == 1);
    CHECK(ds->entries[0].target == 1.0);
    std::remove(tmpPath.c_str());
}

static void test_parse_trailing_spaces() {
    // Trailing spaces after the result token should be trimmed
    std::string tmpPath = "test_tune_trail.tmp";
    {
        std::ofstream out(tmpPath);
        out << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 1-0   \n";
    }
    auto ds = chess::TuningDataset::load(tmpPath, false);
    CHECK(ds.has_value());
    CHECK(ds->warnings.empty());
    CHECK(ds->entries.size() == 1);
    CHECK(ds->entries[0].target == 1.0);
    std::remove(tmpPath.c_str());
}

static void test_parse_leading_whitespace_comment() {
    // Lines with leading whitespace before # should be treated as comments
    std::string tmpPath = "test_tune_lead.tmp";
    {
        std::ofstream out(tmpPath);
        out << "   # this is a comment with leading spaces\n";
        out << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 1/2-1/2\n";
    }
    auto ds = chess::TuningDataset::load(tmpPath, false);
    CHECK(ds.has_value());
    CHECK(ds->warnings.empty());
    CHECK(ds->entries.size() == 1);
    std::remove(tmpPath.c_str());
}

static void test_parse_tabs_around_result() {
    // Tabs around the result token should be trimmed
    std::string tmpPath = "test_tune_tabs.tmp";
    {
        std::ofstream out(tmpPath);
        out << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\t0-1\n";
    }
    auto ds = chess::TuningDataset::load(tmpPath, false);
    CHECK(ds.has_value());
    CHECK(ds->warnings.empty());
    CHECK(ds->entries.size() == 1);
    CHECK(ds->entries[0].target == 0.0);
    std::remove(tmpPath.c_str());
}

// ---------- loss function tests ----------

static void test_loss_range() {
    // Loss must be in [0, 1] for MSE with sigmoid targets
    std::string tmpPath = "test_tune_loss.tmp";
    {
        std::ofstream out(tmpPath);
        out << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 1/2-1/2\n";
        out << "8/8/8/4k3/4P3/4K3/8/8 w - - 0 1 1-0\n";
    }
    auto ds = chess::TuningDataset::load(tmpPath, false);
    CHECK(ds.has_value());

    chess::EvalParams params;
    double loss = chess::computeLoss(*ds, params, 1.0);
    CHECK(loss >= 0.0);
    CHECK(loss <= 1.0);
    std::remove(tmpPath.c_str());
}

static void test_loss_deterministic() {
    std::string tmpPath = "test_tune_det.tmp";
    {
        std::ofstream out(tmpPath);
        out << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 1/2-1/2\n";
    }
    auto ds = chess::TuningDataset::load(tmpPath, false);
    CHECK(ds.has_value());

    chess::EvalParams params;
    double l1 = chess::computeLoss(*ds, params, 1.0);
    double l2 = chess::computeLoss(*ds, params, 1.0);
    CHECK_CLOSE(l1, l2, 1e-9);
    std::remove(tmpPath.c_str());
}

static void test_loss_empty_dataset() {
    chess::TuningDataset empty;
    chess::EvalParams params;
    double loss = chess::computeLoss(empty, params, 1.0);
    CHECK_CLOSE(loss, 0.0, 1e-9);
}

// ---------- K optimisation tests ----------

static void test_optimize_k_produces_result() {
    std::string tmpPath = "test_tune_optk.tmp";
    {
        std::ofstream out(tmpPath);
        out << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 1/2-1/2\n";
        out << "8/8/8/4k3/4P3/4K3/8/8 w - - 0 1 1-0\n";
        out << "8/8/8/4k3/4p3/4K3/8/8 w - - 0 1 0-1\n";
    }
    auto ds = chess::TuningDataset::load(tmpPath, false);
    CHECK(ds.has_value());

    chess::EvalParams params;
    auto result = chess::optimizeK(*ds, params);

    CHECK(result.baselineK == 1.0);
    CHECK(result.baselineLoss >= 0.0);
    CHECK(result.baselineLoss <= 1.0);
    CHECK(result.bestK >= 0.1);
    CHECK(result.bestK <= 5.0);
    CHECK(result.bestLoss >= 0.0);
    CHECK(result.bestLoss <= result.baselineLoss + 1e-9);
    std::remove(tmpPath.c_str());
}

static void test_optimize_k_deterministic() {
    std::string tmpPath = "test_tune_optk_det.tmp";
    {
        std::ofstream out(tmpPath);
        out << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 1/2-1/2\n";
        out << "8/8/8/4k3/4P3/4K3/8/8 w - - 0 1 1-0\n";
    }
    auto ds = chess::TuningDataset::load(tmpPath, false);
    CHECK(ds.has_value());

    chess::EvalParams params;
    auto r1 = chess::optimizeK(*ds, params);
    auto r2 = chess::optimizeK(*ds, params);

    CHECK_CLOSE(r1.bestK, r2.bestK, 1e-9);
    CHECK_CLOSE(r1.bestLoss, r2.bestLoss, 1e-9);
    std::remove(tmpPath.c_str());
}

// ---------- fixture smoke test ----------

static void test_fixture_loads_clean() {
    // The committed fixture must load without warnings in non-strict mode
    auto ds = chess::TuningDataset::load("../test/fixtures/tune-small.txt", false);
    CHECK(ds.has_value());
    CHECK(ds->warnings.empty());
    CHECK(ds->entries.size() == 10);
}

int main() {
    chess::attacks::init();
    chess::Board::initZobrist();

    RUN_TEST(parse_valid_lines);
    RUN_TEST(parse_strict_mode);
    RUN_TEST(parse_nonstrict_collects_warnings);
    RUN_TEST(parse_invalid_fen_rejected);
    RUN_TEST(parse_file_not_found);
    RUN_TEST(parse_carriage_return);
    RUN_TEST(parse_trailing_spaces);
    RUN_TEST(parse_leading_whitespace_comment);
    RUN_TEST(parse_tabs_around_result);
    RUN_TEST(loss_range);
    RUN_TEST(loss_deterministic);
    RUN_TEST(loss_empty_dataset);
    RUN_TEST(optimize_k_produces_result);
    RUN_TEST(optimize_k_deterministic);
    RUN_TEST(fixture_loads_clean);

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "\nAll tune tests passed." << std::endl;
    return 0;
}
