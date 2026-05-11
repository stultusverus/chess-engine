#pragma once

#include "types.h"
#include "board.h"
#include "search.h"
#include "book.h"
#include <atomic>
#include <thread>

namespace chess {

class UCI {
public:
    UCI();
    ~UCI();
    void loop();

private:
    void handleUci();
    void handleIsReady();
    void handleUciNewGame();
    void handlePosition(const std::string& line);
    void handleGo(const std::string& line);
    void handleStop();
    void handleSetOption(const std::string& line);
    void stopSearch();

    Board board_;
    Search search_;
    Book book_;
    bool bookEnabled_ = false;
    int moveOverheadMs_ = 0;
    bool showWdl_ = false;
    std::thread searchThread_;
    std::atomic<bool> searchRunning_{false};
};

} // namespace chess
