#pragma once

#include "types.h"
#include "board.h"
#include "search.h"

namespace chess {

class UCI {
public:
    UCI();
    void loop();

private:
    void handleUci();
    void handleIsReady();
    void handleUciNewGame();
    void handlePosition(const std::string& line);
    void handleGo(const std::string& line);
    void handleStop();
    void handleSetOption(const std::string& line);

    Board board_;
    Search search_;
};

} // namespace chess
