#include "common/memory_pool.h"
#include <stdexcept>
#include <cstring>
#include <cstdlib>

namespace mdfh {

MemoryPool::MemoryPool(size_t block_size, size_t num_blocks)
    : block_size_(block_size), num_blocks_(num_blocks), memory_(nullptr) {
    
    // Round up block size to 64-byte alignment
    size_t aligned_block_size = (block_size + 63) & ~63;
    block_size_ = aligned_block_size;
    
    // Allocate memory with 64-byte alignment
    size_t total_size = aligned_block_size * num_blocks;
    memory_ = static_cast<uint8_t*>(aligned_alloc(64, total_size));
    
    if (!memory_) {
        throw std::bad_alloc();
    }
    
    // Initialize free list
    free_list_.reserve(num_blocks);
    for (size_t i = 0; i < num_blocks; ++i) {
        void* block = memory_ + (i * aligned_block_size);
        free_list_.push_back(block);
    }
}

MemoryPool::~MemoryPool() {
    if (memory_) {
        free(memory_);
    }
}

void* MemoryPool::allocate() {
    std::scoped_lock lock(mutex_);
    
    if (free_list_.empty()) {
        return nullptr;
    }
    
    void* ptr = free_list_.back();
    free_list_.pop_back();
    
    return ptr;
}

void MemoryPool::deallocate(void* ptr) {
    if (!ptr) return;
    
    std::scoped_lock lock(mutex_);
    free_list_.push_back(ptr);
}

size_t MemoryPool::get_available_blocks() const {
    std::scoped_lock lock(mutex_);
    return free_list_.size();
}

} // namespace mdfh
