#pragma once
#include <atomic>
#include <cstddef>
#include <vector>

namespace aerohedge {

template <typename T, size_t Capacity>
class SPSCQueue {
private:
    // Capacity must be a power of 2 for the bitwise AND trick to work
    static_assert((Capacity != 0) && ((Capacity & (Capacity - 1)) == 0), "[SPSC_QUEUE]: Capacity must be a power of 2");
    static constexpr size_t Mask = Capacity - 1;

    // alignas(64) forces these variables onto separate CPU cache lines.
    // This prevents "False Sharing" where Core 1 and Core 2 constantly 
    // invalidate each other's L1 cache, which would ruin performance.
    
    alignas(64) std::atomic<size_t> head_{0}; // Producer writes here
    alignas(64) std::atomic<size_t> tail_{0}; // Consumer reads here
    
    // The actual memory pool for our objects
    alignas(64) T buffer_[Capacity]; 

public:
    SPSCQueue() = default;

    // Push: Called ONLY by the Network Thread (Producer Thread)
    bool push(const T& item) {
        // std::memory_order_relaxed: performs an atomic read of a variable
        // without enforcing any synchronization or ordering constraints on surrounding operations
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (current_head + 1) & Mask;
        
        // std::memory_order_acquire ensures that any subsequent read or write operations 
        // in the current thread cannot be reordered before this atomic read. 
        // If the next spot is the tail, the queue is full
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false; 
        }

        buffer_[current_head] = item;
        
        // memory_order_release ensures the item is fully written to memory 
        // BEFORE the head index is updated for the consumer to see.
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    // Pop: Called ONLY by the Execution/Pricing Thread (Consumer Thread)
    bool pop(T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);

        // If tail == head, the queue is empty
        if (current_tail == head_.load(std::memory_order_acquire)) {
            return false; 
        }

        item = buffer_[current_tail];
        
        tail_.store((current_tail + 1) & Mask, std::memory_order_release);
        return true;
    }
};

} // namespace aerohedge