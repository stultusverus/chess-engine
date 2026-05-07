#include "bot/manager.h"
#include <iostream>
#include <fstream>
#include <cstdlib>

static std::string readTokenFromFile() {
    // Try current directory first
    std::ifstream f(".lichess.key");
    if (!f) {
        // Try parent directory (build/../)
        f.open("../.lichess.key");
    }
    if (f) {
        std::string token;
        std::getline(f, token);
        if (!token.empty()) return token;
    }
    return "";
}

int main(int argc, char* argv[]) {
    std::string token;
    std::string challengeUser;
    std::string bookPath;
    int challengeBots = 0;
    bool ratedOnly = false;
    bool debug = false;
    int minTime = 30;
    int maxTime = 1800;
    int minInc = 0;
    bool autoResign = true;
    int resignThreshold = -800;
    bool autoDraw = true;
    int drawThreshold = 20;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--token" && i + 1 < argc) {
            token = argv[++i];
        } else if (arg == "--book" && i + 1 < argc) {
            bookPath = argv[++i];
        } else if (arg == "--challenge" && i + 1 < argc) {
            challengeUser = argv[++i];
        } else if (arg == "--challenge-bots") {
            challengeBots = (i + 1 < argc && argv[i + 1][0] != '-') ? std::stoi(argv[++i]) : 1;
        } else if (arg == "--rated-only") {
            ratedOnly = true;
        } else if (arg == "--debug") {
            debug = true;
        } else if (arg == "--min-time" && i + 1 < argc) {
            minTime = std::stoi(argv[++i]);
        } else if (arg == "--max-time" && i + 1 < argc) {
            maxTime = std::stoi(argv[++i]);
        } else if (arg == "--min-increment" && i + 1 < argc) {
            minInc = std::stoi(argv[++i]);
        } else if (arg == "--no-resign") {
            autoResign = false;
        } else if (arg == "--resign-threshold" && i + 1 < argc) {
            resignThreshold = std::stoi(argv[++i]);
        } else if (arg == "--no-draw") {
            autoDraw = false;
        } else if (arg == "--draw-threshold" && i + 1 < argc) {
            drawThreshold = std::stoi(argv[++i]);
        }
    }

    if (token.empty()) {
        // Try environment variable
        const char* env = std::getenv("LICHESS_TOKEN");
        if (env) token = env;
    }

    if (token.empty()) {
        // Try .lichess.key file
        token = readTokenFromFile();
    }

    if (token.empty()) {
        std::cerr << "Usage: chess-bot --token <lichess_token> [options]" << std::endl;
        std::cerr << "  --token TOKEN       Lichess API token (env: LICHESS_TOKEN, file: .lichess.key)" << std::endl;
        std::cerr << "  --book FILE         Path to Polyglot opening book (.bin)" << std::endl;
        std::cerr << "  --challenge USER    Challenge a specific player then wait for the game" << std::endl;
        std::cerr << "  --challenge-bots N  Challenge N online bots (default: 1)" << std::endl;
        std::cerr << "  --rated-only        Only accept rated challenges" << std::endl;
        std::cerr << "  --debug             Enable verbose debug logging" << std::endl;
        std::cerr << "  --min-time N        Minimum clock seconds (default: 30)" << std::endl;
        std::cerr << "  --max-time N        Maximum clock seconds (default: 1800)" << std::endl;
        std::cerr << "  --min-increment N   Minimum clock increment (default: 0)" << std::endl;
        std::cerr << "  --no-resign         Disable auto-resign" << std::endl;
        std::cerr << "  --resign-threshold N  Resign eval threshold in centipawns (default: -800)" << std::endl;
        std::cerr << "  --no-draw           Disable auto-draw offer/accept" << std::endl;
        std::cerr << "  --draw-threshold N  Draw eval threshold in centipawns (default: 20)" << std::endl;
        return 1;
    }

    std::cout << "Starting Lichess bot..." << std::endl;
    if (debug) std::cout << "  Debug mode: enabled" << std::endl;
    std::cout << "  Rated only: " << (ratedOnly ? "yes" : "no") << std::endl;
    std::cout << "  Time control range: " << minTime << "s - " << maxTime << "s" << std::endl;
    std::cout << "  Min increment: " << minInc << "s" << std::endl;

    chess::bot::Manager manager(token);
    manager.setDebug(debug);
    manager.setAcceptRated(ratedOnly);
    manager.setMinTime(minTime);
    manager.setMaxTime(maxTime);
    manager.setMinIncrement(minInc);
    manager.setAutoResign(autoResign);
    manager.setResignThreshold(resignThreshold);
    manager.setAutoDraw(autoDraw);
    manager.setDrawThreshold(drawThreshold);

    if (!bookPath.empty()) {
        std::cout << "  Opening book: " << bookPath << std::endl;
        manager.setBookPath(bookPath);
    }

    if (!challengeUser.empty()) {
        manager.challengeOpponent(challengeUser, minTime + 60, std::max(minInc, 2));
    }

    if (challengeBots > 0) {
        manager.challengeBots(challengeBots);
    }

    manager.run();

    return 0;
}
