#ifndef FETCH_DATA_HPP
#define FETCH_DATA_HPP

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <array>
#include <cstdint>

#include "SPSCQueue.hpp" 

enum class BookAction : uint8_t { SNAPSHOT, NEW, CHANGE, DELETE };
enum class BookSide : uint8_t { BID, ASK };

struct BookOrderDelta {
    BookAction action;
    BookSide side;
    double price;
    double amount;
};

template<size_t MaxDeltas = 64>
struct L3BookUpdateEvent {
    uint64_t timestamp{0};
    uint64_t change_id{0};
    size_t delta_count{0};
    std::array<BookOrderDelta, MaxDeltas> deltas;

    bool addDelta(BookAction action, BookSide side, double price, double amount) {
        if (delta_count >= MaxDeltas) return false;
        deltas[delta_count++] = BookOrderDelta{action, side, price, amount};
        return true;
    }
};

using L3Queue = SPSCQueue<L3BookUpdateEvent<64>, 10240>;

class DeribitDataFetcher {
private:
    std::string host_;
    std::string port_;
    std::string instrument_;
    L3Queue& queue_;

    // --- Extracted Option Info ---
    std::string expiry_date_;  // e.g. "27SEP26"
    double strike_price_{0.0}; // e.g. 60000.0
    bool is_call_{true};       // true for 'C', false for 'P'
    bool is_option_{false};

    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;

    void parse_instrument(); // Simple string parser helper
    void run_wss_loop();

public:
    DeribitDataFetcher(const std::string& host, const std::string& port, 
                       const std::string& instrument, L3Queue& queue);
    ~DeribitDataFetcher();

    void start();
    void stop();

    // Getters for Black-Scholes engine
    double strike_price() const { return strike_price_; }
    bool is_call() const { return is_call_; }
    std::string expiry_date() const { return expiry_date_; }
};

#endif 