#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <atomic>
#include <cstddef>
#include <utility>
#include <new>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h> // For _mm_pause()
#endif


template<typename T, size_t Capacity>
class SPSCQueue {
private:
    static constexpr size_t BufferCapacity = Capacity + 1; // +1 to distinguish empty from full state
    T buffer_[BufferCapacity];

    // Hardware cache line alignment (64 bytes) to prevent "False Sharing"
    // between Producer (tail) and Consumer (head) threads across CPU cores.
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

public:
    SPSCQueue() = default;

    // SPSC queues cannot be safely copied or moved
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    /**
     * @brief Emplaces a new element at the back of the queue.
     *        MUST ONLY be called by the PRODUCER thread (WebSocket / Network thread).
     */
    template<typename... Args>
    bool emplace(Args&&... args) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) % BufferCapacity;

        // Check if queue is full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue full
        }

        buffer_[current_tail] = T{std::forward<Args>(args)...};
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pops an element from the front of the queue.
     *        MUST ONLY be called by the CONSUMER thread (Pricing / Engine thread).
     */
    bool pop(T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);

        // Check if queue is empty
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Queue empty
        }

        item = std::move(buffer_[current_head]);
        head_.store((current_head + 1) % BufferCapacity, std::memory_order_release);
        return true;
    }

    /**
     * @brief Checks if the queue is currently empty.
     */
    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Low-latency spin-pause hint for the CPU core while waiting for new data.
     */
    static void cpu_pause() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_pause();
#endif
    }
};

#endif // SPSC_QUEUE_HPP