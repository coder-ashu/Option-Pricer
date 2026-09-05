#include "MarketData.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

Action MarketDataParser::parseAction(const std::string& actStr) {
    if (actStr == "ADD") return Action::ADD;
    if (actStr == "MODIFY") return Action::MODIFY;
    if (actStr == "CANCEL") return Action::CANCEL;
    return Action::UNKNOWN;
}

Side MarketDataParser::parseSide(const std::string& sideStr) {
    if (sideStr == "BID") return Side::BID;
    if (sideStr == "ASK") return Side::ASK;
    return Side::UNKNOWN;
}

std::vector<Tick> MarketDataParser::loadFromCSV(const std::string& filepath) {
    std::vector<Tick> ticks;
    streamFromCSV(filepath, [&ticks](const Tick& tick) {
        ticks.push_back(tick);
    });
    return ticks;
}

void MarketDataParser::streamFromCSV(const std::string& filepath, const std::function<void(const Tick&)>& onTick) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath);
    }

    std::string line;
    // Skip CSV header
    if (!std::getline(file, line)) {
        return;
    }

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string token;
        Tick tick;

        if (!std::getline(ss, tick.timestamp, ',')) continue;
        if (!std::getline(ss, token, ',')) continue;
        tick.action = parseAction(token);

        if (!std::getline(ss, token, ',')) continue;
        tick.side = parseSide(token);

        if (!std::getline(ss, token, ',')) continue;
        tick.price = std::stod(token);

        if (!std::getline(ss, token, ',')) continue;
        tick.qty = std::stoi(token);

        onTick(tick);
    }
}
