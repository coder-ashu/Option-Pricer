#ifndef FETCH_DATA_HPP
#define FETCH_DATA_HPP

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <array>
#include <vector>
#include <cstdint>
#include <cstring>

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
    char symbol[32]{0}; // Identifier for routing multi-symbol updates
    uint64_t timestamp{0};
    uint64_t change_id{0};
    bool is_snapshot{false};
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
    std::vector<std::string> instruments_; // Vector for multiple symbols
    L3Queue& queue_;

    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;

    void run_wss_loop();

public:
    DeribitDataFetcher(const std::string& host, const std::string& port, 
                       const std::vector<std::string>& instruments, L3Queue& queue);
    ~DeribitDataFetcher();

    void start();
    void stop();
};

#endif // FETCH_DATA_HPP
