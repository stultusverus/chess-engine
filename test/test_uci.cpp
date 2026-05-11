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

static std::string runEngine(const std::string& input) {
    std::string escaped;
    for (char c : input) {
        if (c == '\'')
            escaped += "'\\''";
        else
            escaped += c;
    }

    std::string command = "printf '" + escaped + "' | ./chess-engine 2>&1";
    std::array<char, 256> buffer{};
    std::string output;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return output;

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
        output += buffer.data();
    pclose(pipe);

    return output;
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

int main() {
    std::cout << "Running UCI tests:" << std::endl;
    RUN_TEST(rejectsInvalidCoordinates);
    RUN_TEST(rejectsNonPseudoLegalMoves);
    RUN_TEST(rejectsMalformedPromotionTokens);
    RUN_TEST(acceptsLegalMoves);

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "\nAll UCI tests passed." << std::endl;
    return 0;
}
