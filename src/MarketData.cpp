#include "MarketData.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

std::vector<Tick> MarketDataParser::loadFromCSV(const std::string& filepath) {
    std::vector<Tick> ticks;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath);
    }

    std::string line;
    // Read and discard the header row (timestamp,action,side,price,qty)
    std::getline(file, line);

    // Parse line by line
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        Tick tick;

        // Split by comma
        std::getline(ss, tick.timestamp, ',');
        std::getline(ss, tick.action, ',');
        std::getline(ss, tick.side, ',');
        
        std::getline(ss, token, ',');
        tick.price = std::stod(token); // Convert string to double
        
        std::getline(ss, token, ',');
        tick.qty = std::stoi(token);   // Convert string to integer

        ticks.push_back(tick);
    }

    file.close();
    return ticks;
}