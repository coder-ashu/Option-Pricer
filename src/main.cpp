#include <iostream>
#include <iomanip>
#include "OrderBook.hpp"
#include "BlackScholes.hpp"
#include "MarketData.hpp"

int main() {
    std::cout << "[SYSTEM] Booting Vortex MFT Engine...\n";
    OrderBook book;
    
    const std::string path = "../data/historical_ticks.csv";

    // Option Contract Specifications
    const double K = 155.00;          // Strike price
    const double T = 30.0 / 365.0;     // 30 days to maturity
    const double r = 0.05;             // Risk-free rate (5%)
    const double v = 0.20;             // Base implied volatility (20%)
    
    // MFT Quoting Parameters
    const double halfSpread = 0.03;    // Base quote half-spread
    const double gamma = 0.005;        // Inventory risk aversion parameter
    int inventory = 0;                 // Current net inventory (-short, +long)

    std::cout << std::fixed << std::setprecision(3);

    auto handleTick = [&](const Tick& t) {
        // 1. Maintain Level 2 Order Book state
        if (t.side == Side::BID) {
            if (t.action == Action::ADD) book.addBid(t.price, t.qty);
            else if (t.action == Action::MODIFY) book.modifyBid(t.price, t.qty);
            else if (t.action == Action::CANCEL) book.cancelBid(t.price);
        } else if (t.side == Side::ASK) {
            if (t.action == Action::ADD) book.addAsk(t.price, t.qty);
            else if (t.action == Action::MODIFY) book.modifyAsk(t.price, t.qty);
            else if (t.action == Action::CANCEL) book.cancelAsk(t.price);
        }

        // 2. Requires both sides to form a valid market
        if (!book.hasBids() || !book.hasAsks()) {
            return;
        }

        try {
            // 3. Compute microstructure signals
            double mid = book.getMidPrice();
            double micro = book.getMicroPrice();
            double obi = book.getOrderBookImbalance();

            // 4. Compute Black-Scholes Greeks using Micro-price as underlying fair reference
            Greeks g = BlackScholes::calculateCallGreeks(micro, K, T, r, v);

            // 5. Inventory-adjusted theoretical mid (Avellaneda-Stoikov reservation price skew)
            // reservationPrice = theo - inventory * gamma * (volatility^2)
            double reservationSkew = inventory * gamma * (v * v);
            double theoAdjusted = g.price - reservationSkew;

            // 6. Asymmetric quoting based on Order Book Imbalance (OBI)
            // When OBI > 0 (buy pressure), shade quotes slightly higher
            double obiSkew = obi * 0.01;
            double myBid = theoAdjusted - halfSpread + obiSkew;
            double myAsk = theoAdjusted + halfSpread + obiSkew;

            std::cout << "[" << t.timestamp << "] "
                      << "Mid: $" << mid
                      << " | Micro: $" << micro
                      << " | OBI: " << std::setw(6) << obi
                      << " | Call Theo: $" << g.price
                      << " | Delta: " << g.delta
                      << " | Gamma: " << g.gamma
                      << " | Quote: [" << myBid << " / " << myAsk << "]\n";

        } catch (const std::exception& e) {
            // Drop any malformed tick computations gracefully
        }
    };

    try {
        MarketDataParser::streamFromCSV(path, handleTick);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to run market stream: " << e.what() << "\n";
        return 1;
    }

    std::cout << "[SYSTEM] Vortex MFT Engine completed run.\n";
    return 0;
}
