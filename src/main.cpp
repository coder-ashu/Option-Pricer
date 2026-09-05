#include <iostream>
#include <iomanip>
#include "OrderBook.hpp"
#include "BlackScholes.hpp"
#include "MarketData.hpp"

int main() {
    std::cout << "[SYSTEM] Booting Vortex...\n";
    OrderBook book;
    
    std::string path = "../data/historical_ticks.csv";
    std::vector<Tick> data;
    
    try {
        data = MarketDataParser::loadFromCSV(path);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }

    double K = 155.00;
    double T = 30.0 / 365.0;
    double r = 0.05;
    double v = 0.20;
    
    double edge = 0.02; 

    std::cout << std::fixed << std::setprecision(3);

    for (const auto& t : data) {
        if (t.side == "BID") book.addBid(t.price, t.qty);
        else if (t.side == "ASK") book.addAsk(t.price, t.qty);

        try {
            double mid = book.getMidPrice();
            double theoCall = BlackScholes::calculateCallPrice(mid, K, T, r, v);
            
            double myBid = theoCall - edge;
            double myAsk = theoCall + edge;

            std::cout << "[" << t.timestamp << "] " 
                      << "Mid: $" << mid 
                      << " | Theo: $" << theoCall 
                      << " | QUOTE -> BID: $" << myBid << " ASK: $" << myAsk << "\n";

        } catch (...) {
            continue; 
        }
    }

    return 0;
}