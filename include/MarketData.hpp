#ifndef MARKET_DATA_HPP
#define MARKET_DATA_HPP

#include <string>
#include <vector>

// A struct representing a single row from our CSV
struct Tick {
    std::string timestamp;
    std::string action; // "ADD"
    std::string side;   // "BID" or "ASK"
    double price;
    int qty;
};

class MarketDataParser {
public:
    // Reads the CSV and loads all ticks into memory
    static std::vector<Tick> loadFromCSV(const std::string& filepath);
};

#endif // MARKET_DATA_HPP