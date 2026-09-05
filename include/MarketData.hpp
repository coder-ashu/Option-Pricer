#ifndef MARKET_DATA_HPP
#define MARKET_DATA_HPP

#include <string>
#include <vector>
#include <functional>

enum class Action {
    ADD,
    MODIFY,
    CANCEL,
    UNKNOWN
};

enum class Side {
    BID,
    ASK,
    UNKNOWN
};

struct Tick {
    std::string timestamp;
    Action action{Action::ADD};
    Side side{Side::UNKNOWN};
    double price{0.0};
    int qty{0};
};

class MarketDataParser {
public:
    static Action parseAction(const std::string& actStr);
    static Side parseSide(const std::string& sideStr);

    // Reads the entire CSV into memory
    static std::vector<Tick> loadFromCSV(const std::string& filepath);

    // Streams CSV row-by-row to avoid large heap allocations (MFT friendly)
    static void streamFromCSV(const std::string& filepath, const std::function<void(const Tick&)>& onTick);
};

#endif // MARKET_DATA_HPP
