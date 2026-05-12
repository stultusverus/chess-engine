#include <array>
#include <cstdio>
#include <iostream>
#include <string>

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "FAIL: " << #expr << std::endl; failures++; } } while(0)
#define RUN_TEST(name) do { \
    int before = failures; \
    test_##name(); \
    if (failures == before) std::cout << "  " #name ": PASSED" << std::endl; \
} while(0)

static std::string escapeForSingleQuotedPrintf(const std::string& input) {
    std::string escaped;
    for (char c : input) {
        if (c == '\'')
            escaped += "'\\''";
        else
            escaped += c;
    }
    return escaped;
}

static std::string runCommand(const std::string& command) {
    std::array<char, 256> buffer{};
    std::string output;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return output;

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
        output += buffer.data();
    pclose(pipe);

    return output;
}

static std::string runEngine(const std::string& input) {
    std::string escaped = escapeForSingleQuotedPrintf(input);
    std::string command = "printf '" + escaped + "' | ./chess-engine 2>&1";
    return runCommand(command);
}

static std::string runEngineWithDelayedQuit(const std::string& input, const std::string& delaySeconds = "0.1") {
    std::string escaped = escapeForSingleQuotedPrintf(input);
    std::string command = "{ printf '" + escaped + "'; sleep " + delaySeconds + "; printf 'quit\\n'; } | ./chess-engine 2>&1";
    return runCommand(command);
}

static std::string runEngineWithDelayedInput(const std::string& firstInput,
                                             const std::string& delayedInput,
                                             const std::string& delaySeconds = "0.1") {
    std::string first = escapeForSingleQuotedPrintf(firstInput);
    std::string delayed = escapeForSingleQuotedPrintf(delayedInput);
    std::string command = "{ printf '" + first + "'; sleep " + delaySeconds +
                          "; printf '" + delayed + "'; } | ./chess-engine 2>&1";
    return runCommand(command);
}

static int countOccurrences(const std::string& haystack, const std::string& needle) {
    int count = 0;
    std::string::size_type pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        count++;
        pos += needle.size();
    }
    return count;
}

static std::string lastInfoLine(const std::string& output) {
    std::string::size_type pos = output.rfind("info depth");
    if (pos == std::string::npos) return "";
    std::string::size_type end = output.find('\n', pos);
    if (end == std::string::npos) return output.substr(pos);
    return output.substr(pos, end - pos);
}

static std::string scoreField(const std::string& infoLine) {
    std::string::size_type pos = infoLine.find("score ");
    if (pos == std::string::npos) return "";
    std::string::size_type end = infoLine.find(" nodes", pos);
    if (end == std::string::npos) return infoLine.substr(pos);
    return infoLine.substr(pos, end - pos);
}

static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

static void expectIllegal(const std::string& move) {
    std::string output = runEngine("position startpos moves " + move + "\nisready\nquit\n");
    CHECK(contains(output, "[uci] illegal move: " + move));
    CHECK(contains(output, "readyok"));
}

static void expectLegal(const std::string& line) {
    std::string output = runEngine(line + "\nisready\nquit\n");
    CHECK(!contains(output, "[uci] illegal move:"));
    CHECK(contains(output, "readyok"));
}

void test_rejectsInvalidCoordinates() {
    expectIllegal("i2i4");
    expectIllegal("a9a8");
}

void test_rejectsNonPseudoLegalMoves() {
    expectIllegal("e2e5");
    expectIllegal("b1b3");
    expectIllegal("e2f3");
}

void test_rejectsMalformedPromotionTokens() {
    expectIllegal("e2e4q");
    expectIllegal("e2e4x");
    expectIllegal("e2e4qq");

    std::string missing = runEngine("position fen 4k3/P7/8/8/8/8/8/4K3 w - - 0 1 moves a7a8\nisready\nquit\n");
    CHECK(contains(missing, "[uci] illegal move: a7a8"));

    std::string badPiece = runEngine("position fen 4k3/P7/8/8/8/8/8/4K3 w - - 0 1 moves a7a8x\nisready\nquit\n");
    CHECK(contains(badPiece, "[uci] illegal move: a7a8x"));

    std::string tooLong = runEngine("position fen 4k3/P7/8/8/8/8/8/4K3 w - - 0 1 moves a7a8qq\nisready\nquit\n");
    CHECK(contains(tooLong, "[uci] illegal move: a7a8qq"));
}

void test_acceptsLegalMoves() {
    expectLegal("position startpos moves e2e4");
    expectLegal("position fen 4k3/P7/8/8/8/8/8/4K3 w - - 0 1 moves a7a8q");
    expectLegal("position fen 4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1 moves e5d6");
}

void test_searchInfoEmittedOncePerCompletedGo() {
    std::string output = runEngineWithDelayedQuit("position startpos\ngo depth 1\n");

    // Expected: UCI emits one completed-search info line, followed by bestmove.
    CHECK(countOccurrences(output, "info depth") == 1);
    CHECK(contains(output, "bestmove "));
}

void test_mateScoresUseUciMateFormat() {
    std::string output = runEngineWithDelayedQuit(
        "position fen 1k6/ppp5/8/8/8/8/PPP5/1K1R4 w - - 0 1\n"
        "go depth 1\n",
        "1");

    // Expected: mate scores use UCI score mate, not huge centipawn values.
    CHECK(contains(output, "score mate "));
    CHECK(!contains(output, "score cp 999"));
}

void test_repetitionIsScoredAsDraw() {
    std::string output = runEngineWithDelayedQuit(
        "position startpos moves g1f3 g8f6 f3g1 f6g8 g1f3 g8f6 f3g1 f6g8\n"
        "go depth 1\n");

    // Expected: the third occurrence of the same position is scored as a draw.
    CHECK(scoreField(lastInfoLine(output)) == "score cp 0");
}

void test_goRejectsMalformedNumericValues() {
    std::string output = runEngineWithDelayedQuit(
        "position startpos\n"
        "go depth nope movetime 1\n",
        "0.1");

    CHECK(contains(output, "[uci] illegal go depth: nope"));
    CHECK(contains(output, "bestmove "));
}

void test_goNodesStopsAndReportsInfo() {
    std::string output = runEngineWithDelayedQuit(
        "position startpos\n"
        "go nodes 1\n",
        "0.2");

    CHECK(contains(output, "info depth "));
    CHECK(contains(output, " nodes "));
    CHECK(contains(output, " nps "));
    CHECK(contains(output, " hashfull "));
    CHECK(contains(output, "bestmove "));
}

void test_goSearchMovesRestrictsRootMove() {
    std::string output = runEngineWithDelayedQuit(
        "position startpos\n"
        "go depth 1 searchmoves e2e4\n",
        "0.2");

    CHECK(contains(output, "bestmove e2e4"));
}

void test_ponderHitReturnsBestMove() {
    std::string output = runEngineWithDelayedInput(
        "position startpos\n"
        "go ponder\n",
        "ponderhit\nquit\n",
        "0.1");

    CHECK(contains(output, "bestmove "));
}

void test_multiPvReportsMultipleLines() {
    std::string output = runEngineWithDelayedQuit(
        "position startpos\n"
        "setoption name MultiPV value 2\n"
        "go depth 1\n",
        "0.2");

    CHECK(contains(output, "multipv 1"));
    CHECK(contains(output, "multipv 2"));
    CHECK(contains(output, "bestmove "));
}

void test_matedSideUsesUciMateFormat() {
    std::string output = runEngineWithDelayedQuit(
        "position fen 1k1R4/ppp5/8/8/8/8/PPP5/1K6 b - - 0 1\n"
        "go depth 1\n",
        "0.2");

    CHECK(contains(output, "score mate "));
    CHECK(!contains(output, "score cp -999"));
}

void test_invalidFenDoesNotReplaceCurrentPosition() {
    std::string output = runEngineWithDelayedQuit(
        "position startpos\n"
        "position fen 8/8/8/8/8/8/8/8 w - - 0 1\n"
        "go depth 1\n");

    // Expected: the invalid FEN is rejected and the previous valid startpos remains active.
    CHECK(contains(output, "[uci] illegal fen:"));
    CHECK(!contains(output, "bestmove 0000"));
}

void test_incompleteFenDoesNotReplaceCurrentPosition() {
    std::string output = runEngineWithDelayedQuit(
        "position startpos\n"
        "position fen 8/8/8/8/8/8/8/8 w - -\n"
        "go depth 1\n",
        "1");

    CHECK(contains(output, "[uci] illegal fen: 8/8/8/8/8/8/8/8 w - -"));
    CHECK(!contains(output, "bestmove 0000"));
}

void test_unknownPositionTypeDoesNotMutateBoard() {
    std::string output = runEngine(
        "position startpos\n"
        "position typo moves e2e4\n"
        "isready\n"
        "quit\n");

    CHECK(contains(output, "[uci] illegal position: typo"));
    CHECK(!contains(output, "[uci] illegal move:"));
    CHECK(contains(output, "readyok"));
}

int main() {
    std::cout << "Running UCI tests:" << std::endl;
    RUN_TEST(rejectsInvalidCoordinates);
    RUN_TEST(rejectsNonPseudoLegalMoves);
    RUN_TEST(rejectsMalformedPromotionTokens);
    RUN_TEST(acceptsLegalMoves);
    RUN_TEST(searchInfoEmittedOncePerCompletedGo);
    RUN_TEST(mateScoresUseUciMateFormat);
    RUN_TEST(repetitionIsScoredAsDraw);
    RUN_TEST(goRejectsMalformedNumericValues);
    RUN_TEST(goNodesStopsAndReportsInfo);
    RUN_TEST(goSearchMovesRestrictsRootMove);
    RUN_TEST(ponderHitReturnsBestMove);
    RUN_TEST(multiPvReportsMultipleLines);
    RUN_TEST(matedSideUsesUciMateFormat);
    RUN_TEST(invalidFenDoesNotReplaceCurrentPosition);
    RUN_TEST(incompleteFenDoesNotReplaceCurrentPosition);
    RUN_TEST(unknownPositionTypeDoesNotMutateBoard);

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "\nAll UCI tests passed." << std::endl;
    return 0;
}
