#ifndef TRADING_ENGINE_HPP
#define TRADING_ENGINE_HPP

#include "FetchData.hpp"
#include "OrderBook.hpp" 
#include "BlackScholes.hpp"
#include <iostream>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <unordered_set>

// 1. Define the simple metadata struct
struct OptionMetadata {
    double strike_price{0.0};
    bool is_call{true};
    bool is_option{false};
    double years_to_expiry{0.0};
};

class TradingEngine {
private:
    L3Queue& queue_;
    std::unordered_map<std::string, OrderBook> books_;
    std::unordered_map<std::string, OptionMetadata> metadata_map_;
    std::unordered_map<std::string, double> current_ivs_; // Warm start IVs
    std::unordered_set<std::string> pricing_wait_reported_;

    std::atomic<bool> is_running_{false};
    std::thread consumer_thread_;

    double risk_free_rate_{0.05}; 
    std::atomic<double> underlying_spot_price_{0.0};

    static double expiry_years(const std::string& token) {
        if (token.size() != 7) return 0.0;
        static const std::unordered_map<std::string, unsigned> months{
            {"JAN",1},{"FEB",2},{"MAR",3},{"APR",4},{"MAY",5},{"JUN",6},
            {"JUL",7},{"AUG",8},{"SEP",9},{"OCT",10},{"NOV",11},{"DEC",12}};
        try {
            unsigned day = static_cast<unsigned>(std::stoul(token.substr(0, 2)));
            auto it = months.find(token.substr(2, 3));
            int year = std::stoi(token.substr(5, 2)) + 2000;
            if (it == months.end()) return 0.0;
            std::chrono::year_month_day ymd{std::chrono::year{year}, std::chrono::month{it->second}, std::chrono::day{day}};
            if (!ymd.ok()) return 0.0;
            auto expiry = std::chrono::sys_days{ymd} + std::chrono::hours{8};
            auto now = std::chrono::system_clock::now();
            auto seconds = std::chrono::duration<double>(expiry - now).count();
            return std::max(0.0, seconds / (365.0 * 24.0 * 60.0 * 60.0));
        } catch (...) { return 0.0; }
    }

    // 2. Simple string parser to extract Strike and Call/Put
    OptionMetadata parse_symbol(const std::string& instrument) {
        OptionMetadata meta;
        std::stringstream ss(instrument);
        std::string token;
        std::vector<std::string> parts;

        // Split "BTC-27SEP26-60000-C" by '-'
        while (std::getline(ss, token, '-')) {
            parts.push_back(token);
        }

        if (parts.size() >= 4) {
            meta.is_option = true;
            meta.years_to_expiry = expiry_years(parts[1]);
            try {
                meta.strike_price = std::stod(parts[2]); // e.g., 60000.0
                meta.is_call = (parts[3] == "C" || parts[3] == "CALL");
            } catch (...) {
                meta.is_option = false;
            }
        }
        return meta;
    }

    void run_consumer_loop() {
        L3BookUpdateEvent<64> event;

        while (is_running_) {
            if (queue_.pop(event)) {
                std::string symbol = event.symbol;
                auto book_it = books_.find(symbol);
                if (book_it == books_.end()) continue;
                OrderBook& book = book_it->second;
                if (event.is_snapshot) book.clear();

                // Update Orderbook
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

                // Pricing and Greeks Calculation
                if (book.hasBids() && book.hasAsks()) {
                    try {
                        const auto& meta = metadata_map_[symbol];
                        
                        double spot = underlying_spot_price_.load(std::memory_order_relaxed);
                        if (meta.is_option && meta.strike_price > 0.0 && meta.years_to_expiry > 0.0 && spot > 0.0) {
                            
                            // Deribit BTC option premiums are quoted in BTC; Black-Scholes uses USD.
                            double option_mid_quote = book.getMidPrice();
                            double S = spot;
                            double option_mid_price = option_mid_quote * S;
                            double initial_iv = current_ivs_.count(symbol) ? current_ivs_[symbol] : 0.50;

                            // Solve for Implied Volatility
                            double implied_vol = meta.is_call
                                ? BlackScholes::impliedVolatilityCall(option_mid_price, S, meta.strike_price, meta.years_to_expiry, risk_free_rate_, initial_iv)
                                : BlackScholes::impliedVolatilityPut(option_mid_price, S, meta.strike_price, meta.years_to_expiry, risk_free_rate_, initial_iv);

                            current_ivs_[symbol] = implied_vol;

                            // Calculate Greeks
                            Greeks greeks = meta.is_call 
                                ? BlackScholes::calculateCallGreeks(S, meta.strike_price, meta.years_to_expiry, risk_free_rate_, implied_vol)
                                : BlackScholes::calculatePutGreeks(S, meta.strike_price, meta.years_to_expiry, risk_free_rate_, implied_vol);

                            std::cout << std::fixed << std::setprecision(2)
                                      << "[" << symbol << "] "
                                      << "OptMid: " << option_mid_quote << " BTC ($" << option_mid_price << ")"
                                      << " | IV: " << (implied_vol * 100.0) << "%"
                                      << " | Delta: " << std::setprecision(4) << greeks.delta 
                                      << " | Spread: $" << std::setprecision(2) << book.getSpread() << "\n";
                        } else if (pricing_wait_reported_.insert(symbol).second) {
                            std::cout << "[PRICING WAIT] " << symbol << ": ";
                            if (!meta.is_option || meta.strike_price <= 0.0) {
                                std::cout << "symbol did not parse as an option contract.\n";
                            } else if (meta.years_to_expiry <= 0.0) {
                                std::cout << "contract is expired or expiry could not be parsed.\n";
                            } else if (spot <= 0.0) {
                                std::cout << "waiting for a valid underlying spot price.\n";
                            }
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
        // 3. Pre-parse and store metadata at startup!
        for (const auto& symbol : instruments) {
            metadata_map_[symbol] = parse_symbol(symbol);
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
    
    void set_underlying_spot(double spot) {
        if (std::isfinite(spot) && spot > 0.0) underlying_spot_price_.store(spot, std::memory_order_relaxed);
    }
};

#endif // TRADING_ENGINE_HPP
