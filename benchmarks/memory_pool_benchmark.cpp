#include <benchmark/benchmark.h>
#include "common/memory_pool.h"
#include <vector>
#include <memory>

using namespace mdfh;

// Benchmark: Single allocation from pool
static void BM_PoolAllocate(benchmark::State& state) {
    MemoryPool pool(4096, 1000);
    
    for (auto _ : state) {
        void* ptr = pool.allocate();
        benchmark::DoNotOptimize(ptr);
        pool.deallocate(ptr);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_PoolAllocate);

// Benchmark: Compare with malloc/free
static void BM_MallocFree(benchmark::State& state) {
    for (auto _ : state) {
        void* ptr = malloc(4096);
        benchmark::DoNotOptimize(ptr);
        free(ptr);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MallocFree);

// Benchmark: Batch allocation
static void BM_PoolAllocateBatch(benchmark::State& state) {
    MemoryPool pool(4096, 1000);
    int batch_size = state.range(0);
    std::vector<void*> ptrs(batch_size);
    
    for (auto _ : state) {
        // Allocate batch
        for (int i = 0; i < batch_size; ++i) {
            ptrs[i] = pool.allocate();
        }
        benchmark::ClobberMemory();
        
        // Deallocate batch
        for (int i = 0; i < batch_size; ++i) {
            pool.deallocate(ptrs[i]);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_PoolAllocateBatch)->Range(10, 500);

// Benchmark: Pool contention (multi-threaded)
static void BM_PoolConcurrent(benchmark::State& state) {
    static MemoryPool pool(4096, 10000);
    
    for (auto _ : state) {
        void* ptr = pool.allocate();
        benchmark::DoNotOptimize(ptr);
        // Do some work
        benchmark::ClobberMemory();
        pool.deallocate(ptr);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_PoolConcurrent)->Threads(4)->UseRealTime();

// Benchmark: Pool exhaustion handling
static void BM_PoolExhaustion(benchmark::State& state) {
    MemoryPool pool(4096, 100);
    std::vector<void*> ptrs;
    
    // Fill the pool
    for (int i = 0; i < 100; ++i) {
        void* ptr = pool.allocate();
        if (ptr) ptrs.push_back(ptr);
    }
    
    for (auto _ : state) {
        // Try to allocate when exhausted
        void* ptr = pool.allocate();
        benchmark::DoNotOptimize(ptr);
        
        if (ptr) {
            pool.deallocate(ptr);
        }
    }
    
    // Cleanup
    for (void* ptr : ptrs) {
        pool.deallocate(ptr);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_PoolExhaustion);

// Benchmark: Different block sizes
static void BM_PoolBlockSizes(benchmark::State& state) {
    size_t block_size = state.range(0);
    MemoryPool pool(block_size, 1000);
    
    for (auto _ : state) {
        void* ptr = pool.allocate();
        benchmark::DoNotOptimize(ptr);
        pool.deallocate(ptr);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetLabel(std::to_string(block_size) + " bytes");
}
BENCHMARK(BM_PoolBlockSizes)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(8192)
    ->Arg(65536);

// Benchmark: Pool reset operation
static void BM_PoolReset(benchmark::State& state) {
    MemoryPool pool(4096, 1000);
    
    for (auto _ : state) {
        state.PauseTiming();
        // Allocate some blocks
        std::vector<void*> ptrs;
        for (int i = 0; i < 100; ++i) {
            void* ptr = pool.allocate();
            if (ptr) ptrs.push_back(ptr);
        }
        state.ResumeTiming();
        
        // Deallocate all
        for (void* ptr : ptrs) {
            pool.deallocate(ptr);
        }
        benchmark::ClobberMemory();
    }
    
    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(BM_PoolReset);

BENCHMARK_MAIN();
