#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <cstdint>
#include <vector>
#include <mutex>
#include <memory>

namespace mdfh {

class MemoryPool {
public:
    MemoryPool(size_t block_size, size_t num_blocks);
    ~MemoryPool();
    
    // Allocate a block from pool
    void* allocate();
    
    // Return block to pool
    void deallocate(void* ptr);
    
    // Pool statistics
    size_t get_block_size() const { return block_size_; }
    size_t get_total_blocks() const { return num_blocks_; }
    size_t get_available_blocks() const;
    
private:
    size_t block_size_;
    size_t num_blocks_;
    uint8_t* memory_;
    std::vector<void*> free_list_;
    mutable std::mutex mutex_;
};

// RAII wrapper for pool allocation
template<typename T>
class PoolPtr {
public:
    PoolPtr(MemoryPool& pool) : pool_(pool), ptr_(nullptr) {
        ptr_ = static_cast<T*>(pool_.allocate());
    }
    
    ~PoolPtr() {
        if (ptr_) {
            pool_.deallocate(ptr_);
        }
    }
    
    T* get() { return ptr_; }
    const T* get() const { return ptr_; }
    
    T& operator*() { return *ptr_; }
    const T& operator*() const { return *ptr_; }
    
    T* operator->() { return ptr_; }
    const T* operator->() const { return ptr_; }
    
    PoolPtr(const PoolPtr&) = delete;
    PoolPtr& operator=(const PoolPtr&) = delete;
    
private:
    MemoryPool& pool_;
    T* ptr_;
};

} // namespace mdfh

#endif // MEMORY_POOL_H
