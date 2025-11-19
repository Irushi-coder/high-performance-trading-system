#ifndef LOCKFREE_QUEUE_HPP
#define LOCKFREE_QUEUE_HPP

#include <atomic>
#include <memory>
#include <cstddef>

namespace trading {
namespace utils {

/**
 * LockFreeQueue - Single Producer Single Consumer (SPSC) lock-free queue.
 * Optimized for low-latency message passing between threads.
 */
template<typename T, size_t Size = 4096>
class LockFreeQueue {
public:
    LockFreeQueue() : head_(0), tail_(0) {
        static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");
    }

    /**
     * Try to push an item to the queue.
     * Returns true on success, false if queue is full.
     */
    bool tryPush(const T& item) {
        size_t currentTail = tail_.load(std::memory_order_relaxed);
        size_t nextTail = (currentTail + 1) & (Size - 1);
        
        if (nextTail == head_.load(std::memory_order_acquire)) {
            return false; // Queue is full
        }
        
        buffer_[currentTail] = item;
        tail_.store(nextTail, std::memory_order_release);
        return true;
    }

    /**
     * Try to push an item using move semantics.
     */
    bool tryPush(T&& item) {
        size_t currentTail = tail_.load(std::memory_order_relaxed);
        size_t nextTail = (currentTail + 1) & (Size - 1);
        
        if (nextTail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        
        buffer_[currentTail] = std::move(item);
        tail_.store(nextTail, std::memory_order_release);
        return true;
    }

    /**
     * Try to pop an item from the queue.
     * Returns true on success, false if queue is empty.
     */
    bool tryPop(T& item) {
        size_t currentHead = head_.load(std::memory_order_relaxed);
        
        if (currentHead == tail_.load(std::memory_order_acquire)) {
            return false; // Queue is empty
        }
        
        item = std::move(buffer_[currentHead]);
        head_.store((currentHead + 1) & (Size - 1), std::memory_order_release);
        return true;
    }

    /**
     * Check if queue is empty.
     */
    bool isEmpty() const {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }

    /**
     * Check if queue is full.
     */
    bool isFull() const {
        size_t currentTail = tail_.load(std::memory_order_acquire);
        size_t nextTail = (currentTail + 1) & (Size - 1);
        return nextTail == head_.load(std::memory_order_acquire);
    }

    /**
     * Get approximate size (not exact due to concurrency).
     */
    size_t size() const {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        return (tail >= head) ? (tail - head) : (Size - head + tail);
    }

    /**
     * Get capacity.
     */
    constexpr size_t capacity() const {
        return Size - 1; // One slot reserved for full/empty distinction
    }

private:
    // Cache line padding to prevent false sharing
    static constexpr size_t CACHE_LINE_SIZE = 64;
    
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_;
    alignas(CACHE_LINE_SIZE) T buffer_[Size];
};

/**
 * Multi-Producer Single Consumer (MPSC) lock-free queue.
 * Suitable for multiple threads submitting orders to a single matching engine.
 */
template<typename T>
class MPSCQueue {
private:
    struct Node {
        T data;
        std::atomic<Node*> next;
        
        Node(const T& item) : data(item), next(nullptr) {}
        Node(T&& item) : data(std::move(item)), next(nullptr) {}
    };

public:
    MPSCQueue() {
        Node* dummy = new Node(T{});
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
    }

    ~MPSCQueue() {
        T item;
        while (tryPop(item)) {}
        
        Node* front = head_.load(std::memory_order_relaxed);
        delete front;
    }

    /**
     * Push item to queue (thread-safe for multiple producers).
     */
    void push(const T& item) {
        Node* newNode = new Node(item);
        pushNode(newNode);
    }

    void push(T&& item) {
        Node* newNode = new Node(std::move(item));
        pushNode(newNode);
    }

    /**
     * Try to pop item (single consumer only).
     */
    bool tryPop(T& item) {
        Node* head = head_.load(std::memory_order_relaxed);
        Node* next = head->next.load(std::memory_order_acquire);
        
        if (next == nullptr) {
            return false; // Queue is empty
        }
        
        item = std::move(next->data);
        head_.store(next, std::memory_order_release);
        delete head; // Safe because we're single consumer
        
        return true;
    }

    /**
     * Check if queue is empty (approximate).
     */
    bool isEmpty() const {
        Node* head = head_.load(std::memory_order_acquire);
        Node* next = head->next.load(std::memory_order_acquire);
        return next == nullptr;
    }

private:
    alignas(64) std::atomic<Node*> head_;
    alignas(64) std::atomic<Node*> tail_;

    void pushNode(Node* newNode) {
        Node* prevTail = tail_.exchange(newNode, std::memory_order_acq_rel);
        prevTail->next.store(newNode, std::memory_order_release);
    }
};

} // namespace utils
} // namespace trading

#endif // LOCKFREE_QUEUE_HPP