#include <gtest/gtest.h>
#include "common/memory_pool.h"
#include <thread>
#include <vector>

using namespace mdfh;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool = std::make_unique<MemoryPool>(1024, 100);
    }

    std::unique_ptr<MemoryPool> pool;
};

TEST_F(MemoryPoolTest, AllocateSingleBlock) {
    void* ptr = pool->allocate();
    EXPECT_NE(ptr, nullptr);
}

TEST_F(MemoryPoolTest, AllocateAndDeallocate) {
    void* ptr = pool->allocate();
    EXPECT_NE(ptr, nullptr);
    
    pool->deallocate(ptr);
    
    // Should be able to allocate again
    void* ptr2 = pool->allocate();
    EXPECT_NE(ptr2, nullptr);
}

TEST_F(MemoryPoolTest, AllocateAllBlocks) {
    std::vector<void*> blocks;
    
    for (int i = 0; i < 100; ++i) {
        void* ptr = pool->allocate();
        EXPECT_NE(ptr, nullptr);
        blocks.push_back(ptr);
    }
    
    // All blocks allocated, next should return nullptr
    void* ptr = pool->allocate();
    EXPECT_EQ(ptr, nullptr);
    
    // Deallocate all
    for (void* p : blocks) {
        pool->deallocate(p);
    }
}

TEST_F(MemoryPoolTest, ReuseBlocks) {
    void* ptr1 = pool->allocate();
    pool->deallocate(ptr1);
    
    void* ptr2 = pool->allocate();
    EXPECT_EQ(ptr1, ptr2) << "Should reuse deallocated block";
}

TEST_F(MemoryPoolTest, ConcurrentAllocation) {
    const int num_threads = 4;
    const int allocations_per_thread = 25; // Total 100 blocks
    
    std::vector<std::thread> threads;
    std::vector<std::vector<void*>> thread_blocks(num_threads);
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < allocations_per_thread; ++j) {
                void* ptr = pool->allocate();
                if (ptr) {
                    thread_blocks[i].push_back(ptr);
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    int total_allocated = 0;
    for (const auto& blocks : thread_blocks) {
        total_allocated += blocks.size();
    }
    
    EXPECT_EQ(total_allocated, 100);
}

TEST_F(MemoryPoolTest, AllocationSpeed) {
    const int iterations = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        void* ptr = pool->allocate();
        pool->deallocate(ptr);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double avg_time = static_cast<double>(duration) / (iterations * 2);
    
    std::cout << "Average allocate/deallocate time: " << avg_time << " ns" << std::endl;
    EXPECT_LT(avg_time, 100.0) << "Pool operations should be fast";
}

TEST_F(MemoryPoolTest, Alignment) {
    void* ptr = pool->allocate();
    EXPECT_NE(ptr, nullptr);
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % 64, 0) << "Memory should be 64-byte aligned";
    
    pool->deallocate(ptr);
}

TEST_F(MemoryPoolTest, Stats) {
    EXPECT_EQ(pool->get_total_blocks(), 100);
    EXPECT_EQ(pool->get_available_blocks(), 100);
    
    void* ptr1 = pool->allocate();
    void* ptr2 = pool->allocate();
    
    EXPECT_EQ(pool->get_available_blocks(), 98);
    
    pool->deallocate(ptr1);
    pool->deallocate(ptr2);
    
    EXPECT_EQ(pool->get_available_blocks(), 100);
}
