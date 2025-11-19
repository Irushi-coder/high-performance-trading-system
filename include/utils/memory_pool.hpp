#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP

#include <cstddef>
#include <cstdint>
#include <new>
#include <atomic>
#include <vector>

namespace trading {
namespace utils {

/**
 * MemoryPool provides fast, thread-safe allocation for fixed-size objects.
 * Eliminates malloc/free overhead in critical paths.
 */
template<typename T, size_t BlockSize = 1024>
class MemoryPool {
private:
    // Union for free list linkage
    union Node {
        T data;
        Node* next;
        
        Node() {}
        ~Node() {}
    };

    struct Block {
        Node nodes[BlockSize];
        Block* next;
        
        Block() : next(nullptr) {}
    };

public:
    MemoryPool() : head_(nullptr), blocks_(nullptr) {
        allocateBlock();
    }

    ~MemoryPool() {
        // Free all blocks
        Block* current = blocks_;
        while (current) {
            Block* next = current->next;
            delete current;
            current = next;
        }
    }

    // Disable copy and move
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    /**
     * Allocate memory for one object.
     * Returns nullptr if pool is exhausted (very rare with pre-allocation).
     */
    T* allocate() {
        Node* node = head_.load(std::memory_order_acquire);
        
        // Try to pop from free list (lock-free)
        while (node != nullptr) {
            Node* next = node->next;
            if (head_.compare_exchange_weak(node, next,
                                           std::memory_order_release,
                                           std::memory_order_acquire)) {
                return &node->data;
            }
        }

        // Free list empty, allocate new block (rare)
        allocateBlock();
        return allocate(); // Retry
    }

    /**
     * Deallocate memory (return to pool).
     */
    void deallocate(T* ptr) {
        if (!ptr) return;

        Node* node = reinterpret_cast<Node*>(ptr);
        Node* oldHead = head_.load(std::memory_order_acquire);
        
        // Push to free list (lock-free)
        do {
            node->next = oldHead;
        } while (!head_.compare_exchange_weak(oldHead, node,
                                             std::memory_order_release,
                                             std::memory_order_acquire));
    }

    /**
     * Construct object in-place.
     */
    template<typename... Args>
    T* construct(Args&&... args) {
        T* ptr = allocate();
        if (ptr) {
            new (ptr) T(std::forward<Args>(args)...);
        }
        return ptr;
    }

    /**
     * Destroy and deallocate object.
     */
    void destroy(T* ptr) {
        if (ptr) {
            ptr->~T();
            deallocate(ptr);
        }
    }

    /**
     * Get statistics.
     */
    struct Stats {
        size_t blocksAllocated;
        size_t totalCapacity;
    };

    Stats getStats() const {
        size_t blockCount = 0;
        Block* current = blocks_;
        while (current) {
            blockCount++;
            current = current->next;
        }
        
        return {blockCount, blockCount * BlockSize};
    }

private:
    std::atomic<Node*> head_;  // Free list head
    std::atomic<Block*> blocks_; // Block list head

    void allocateBlock() {
        Block* newBlock = new Block();
        
        // Initialize free list for this block
        for (size_t i = 0; i < BlockSize - 1; ++i) {
            newBlock->nodes[i].next = &newBlock->nodes[i + 1];
        }
        newBlock->nodes[BlockSize - 1].next = head_.load(std::memory_order_acquire);
        
        // Add block to block list
        Block* oldBlocks = blocks_.load(std::memory_order_acquire);
        do {
            newBlock->next = oldBlocks;
        } while (!blocks_.compare_exchange_weak(oldBlocks, newBlock,
                                               std::memory_order_release,
                                               std::memory_order_acquire));
        
        // Update free list head
        head_.store(&newBlock->nodes[0], std::memory_order_release);
    }
};

/**
 * STL-compatible allocator wrapper for MemoryPool.
 */
template<typename T>
class PoolAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    struct rebind {
        using other = PoolAllocator<U>;
    };

    PoolAllocator() = default;
    
    template<typename U>
    PoolAllocator(const PoolAllocator<U>&) noexcept {}

    pointer allocate(size_type n) {
        if (n != 1) {
            return static_cast<pointer>(::operator new(n * sizeof(T)));
        }
        return getPool().allocate();
    }

    void deallocate(pointer p, size_type n) {
        if (n != 1) {
            ::operator delete(p);
            return;
        }
        getPool().deallocate(p);
    }

    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        new (p) U(std::forward<Args>(args)...);
    }

    template<typename U>
    void destroy(U* p) {
        p->~U();
    }

private:
    static MemoryPool<T>& getPool() {
        static MemoryPool<T> pool;
        return pool;
    }
};

template<typename T, typename U>
bool operator==(const PoolAllocator<T>&, const PoolAllocator<U>&) {
    return true;
}

template<typename T, typename U>
bool operator!=(const PoolAllocator<T>&, const PoolAllocator<U>&) {
    return false;
}

} // namespace utils
} // namespace trading

#endif // MEMORY_POOL_HPP