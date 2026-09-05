#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <map>
#include <stdexcept>
#include <iostream>

class OrderBook {
private:
    // Bids: Buyers want the lowest price, but the 'best' bid is the highest price.
    // std::greater sorts highest to lowest.
    std::map<double, int, std::greater<double>> bids;
    
    // Asks: Sellers want the highest price, but the 'best' ask is the lowest price.
    // std::less sorts lowest to highest.
    std::map<double, int, std::less<double>> asks;

public:
    // Add liquidity to the buy side
    void addBid(double price, int quantity) {
        bids[price] += quantity;
    }

    // Add liquidity to the sell side
    void addAsk(double price, int quantity) {
        asks[price] += quantity;
    }

    double getBestBid() const {
        if (bids.empty()) return 0.0;
        return bids.begin()->first; // The top of the greater-sorted map
    }

    double getBestAsk() const {
        if (asks.empty()) return 0.0;
        return asks.begin()->first; // The top of the less-sorted map
    }

    // This is the critical number we will feed to our Black-Scholes model
    double getMidPrice() const {
        if (bids.empty() || asks.empty()) {
            throw std::runtime_error("Cannot calculate mid-price: Book is empty on one side.");
        }
        return (getBestBid() + getBestAsk()) / 2.0;
    }
};

#endif // ORDERBOOK_HPP