#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <map>
#include <stdexcept>
#include <cmath>

class OrderBook {
private:
    std::map<double, int, std::greater<double>> bids;
    std::map<double, int, std::less<double>> asks;

public:
    void addBid(double price, int quantity) {
        if (quantity <= 0) return;
        bids[price] += quantity;
    }

    void addAsk(double price, int quantity) {
        if (quantity <= 0) return;
        asks[price] += quantity;
    }

    void modifyBid(double price, int quantity) {
        if (quantity <= 0) {
            cancelBid(price);
        } else {
            bids[price] = quantity;
        }
    }

    void modifyAsk(double price, int quantity) {
        if (quantity <= 0) {
            cancelAsk(price);
        } else {
            asks[price] = quantity;
        }
    }

    void cancelBid(double price) {
        bids.erase(price);
    }

    void cancelAsk(double price) {
        asks.erase(price);
    }

    bool hasBids() const {
        return !bids.empty();
    }

    bool hasAsks() const {
        return !asks.empty();
    }

    double getBestBid() const {
        if (bids.empty()) return 0.0;
        return bids.begin()->first;
    }

    int getBestBidQty() const {
        if (bids.empty()) return 0;
        return bids.begin()->second;
    }

    double getBestAsk() const {
        if (asks.empty()) return 0.0;
        return asks.begin()->first;
    }

    int getBestAskQty() const {
        if (asks.empty()) return 0;
        return asks.begin()->second;
    }

    double getSpread() const {
        if (bids.empty() || asks.empty()) return 0.0;
        return getBestAsk() - getBestBid();
    }

    double getMidPrice() const {
        if (bids.empty() || asks.empty()) {
            throw std::runtime_error("Cannot calculate mid-price: Book is empty on one side.");
        }
        return (getBestBid() + getBestAsk()) / 2.0;
    }

    // Micro-price weighted by top-of-book volume:
    // P_micro = (Q_bid * P_ask + Q_ask * P_bid) / (Q_bid + Q_ask)
    double getMicroPrice() const {
        if (bids.empty() || asks.empty()) {
            throw std::runtime_error("Cannot calculate micro-price: Book is empty on one side.");
        }
        double bestBid = getBestBid();
        double bestAsk = getBestAsk();
        int bidQty = getBestBidQty();
        int askQty = getBestAskQty();

        int totalQty = bidQty + askQty;
        if (totalQty == 0) return (bestBid + bestAsk) / 2.0;

        return (static_cast<double>(bidQty) * bestAsk + static_cast<double>(askQty) * bestBid) / totalQty;
    }

    // Order Book Imbalance (OBI) at top of book, bounded in [-1.0, 1.0]
    // OBI > 0 implies buying pressure; OBI < 0 implies selling pressure.
    double getOrderBookImbalance() const {
        if (bids.empty() || asks.empty()) return 0.0;
        int bidQty = getBestBidQty();
        int askQty = getBestAskQty();
        int totalQty = bidQty + askQty;
        if (totalQty == 0) return 0.0;

        return static_cast<double>(bidQty - askQty) / totalQty;
    }
};

#endif // ORDERBOOK_HPP
