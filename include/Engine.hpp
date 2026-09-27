#ifndef TRADING_ENGINE_HPP
#define TRADING_ENGINE_HPP

#include "FetchData.hpp"
#include "Orderbook.hpp"
#include "BlackScholes.hpp"
#include <iostream>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <string>
#include <vector>
#include <iomanip>

class TradingEngine {
private:
    L3Queue& queue_;
    std::unordered_map<std::string, OrderBook> books_;
    std::unordered_map<std::string, OptionMetadata> metadata_map_;
    
    // WARM START: Store the last calculated IV for each symbol. 
    // Newton-Raphson converges much faster if initialVol is close to the real answer.
    std::unordered_map<std::string, double> current_ivs_;

    std::atomic<bool> is_running_{false};
    std::thread consumer_thread_;

    double risk_free_rate_{0.05}; 
    
    // In a real system, you'd fetch this from a perpetual WebSocket channel (e.g. BTC-PERPETUAL)
    // For now, we simulate a constant underlying BTC spot price.
    std::atomic<double> underlying_spot_price_{65000.0}; 

    void run_consumer_loop() {
        L3BookUpdateEvent<64> event;

        while (is_running_) {
            if (queue_.pop(event)) {
                std::string symbol = event.symbol;
                OrderBook& book = books_[symbol];

                // 1. Update Orderbook
                for (size_t i = 0; i < event.delta_count; ++i) {
                    const auto& delta = event.deltas[i];
                    if (delta.side == BookSide::BID) {
                        if (delta.action == BookAction::NEW) book.addBid(delta.price, delta.amount);
                        else if (delta.action == BookAction::CHANGE) book.modifyBid(delta.price, delta.amount);
                        else if (delta.action == BookAction::DELETE) book.cancelBid(delta.price);
                    } else {
                        if (delta.action == BookAction::NEW) book.addAsk(delta.price, delta.amount);
                        else if (delta.action == BookAction::CHANGE) book.modifyAsk(delta.price, delta.amount);
                        else if (delta.action == BookAction::DELETE) book.cancelAsk(delta.price);
                    }
                }

                // 2. Pricing and Greeks Calculation
                if (book.hasBids() && book.hasAsks()) {
                    try {
                        const auto& meta = metadata_map_[symbol];
                        
                        if (meta.is_option && meta.strike_price > 0.0) {
                            
                            // A: Option Market Premium (e.g., $3,000)
                            double option_mid_price = book.getMidPrice();
                            
                            // B: Underlying Spot (e.g., $65,000)
                            double S = underlying_spot_price_.load(std::memory_order_relaxed);
                            
                            // C: Warm Start Initial Volatility
                            double initial_iv = current_ivs_.count(symbol) ? current_ivs_[symbol] : 0.50;

                            // D: Solve for Implied Volatility
                            double implied_vol = meta.is_call
                                ? BlackScholes::impliedVolatilityCall(option_mid_price, S, meta.strike_price, meta.years_to_expiry, risk_free_rate_, initial_iv)
                                : BlackScholes::impliedVolatilityPut(option_mid_price, S, meta.strike_price, meta.years_to_expiry, risk_free_rate_, initial_iv);

                            // Save IV for the next tick's Newton-Raphson warm start
                            current_ivs_[symbol] = implied_vol;

                            // E: Calculate all Greeks
                            Greeks greeks = meta.is_call 
                                ? BlackScholes::calculateCallGreeks(S, meta.strike_price, meta.years_to_expiry, risk_free_rate_, implied_vol)
                                : BlackScholes::calculatePutGreeks(S, meta.strike_price, meta.years_to_expiry, risk_free_rate_, implied_vol);

                            // F: Display
                            std::cout << std::fixed << std::setprecision(2)
                                      << "[" << symbol << "] "
                                      << "OptPrice: $" << option_mid_price 
                                      << " | IV: " << (implied_vol * 100.0) << "%"
                                      << " | Delta: " << std::setprecision(4) << greeks.delta 
                                      << " | Spread: $" << std::setprecision(2) << book.getSpread() << "\n";
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[WARNING - " << symbol << "]: " << e.what() << "\n";
                    }
                }
            } else {
                SPSCQueue<L3BookUpdateEvent<64>, 10240>::cpu_pause();
            }
        }
    }

public:
    TradingEngine(L3Queue& queue, const std::vector<std::string>& instruments)
        : queue_(queue) {
        for (const auto& symbol : instruments) {
            metadata_map_[symbol] = parse_instrument_metadata(symbol); // Assuming this is declared in FetchData.hpp
            books_[symbol] = OrderBook(); 
        }
    }

    ~TradingEngine() { stop(); }

    void start() {
        is_running_ = true;
        consumer_thread_ = std::thread(&TradingEngine::run_consumer_loop, this);
    }

    void stop() {
        if (is_running_) {
            is_running_ = false;
            if (consumer_thread_.joinable()) {
                consumer_thread_.join();
            }
        }
    }
    
    // Allow the main thread or another WS connection to update the underlying BTC price real-time
    void set_underlying_spot(double spot) {
        underlying_spot_price_.store(spot, std::memory_order_relaxed);
    }
};

#endif // TRADING_ENGINE_HPP