#include <benchmark/benchmark.h>
#include "common/cache.h"
#include <thread>
#include <vector>

using namespace mdfh;

// Benchmark: Single write to cache
static void BM_CacheUpdate(benchmark::State& state) {
    SymbolCache cache(100);
    
    for (auto _ : state) {
        cache.update_bid(0, 2450.25, 1000);
        benchmark::ClobberMemory();
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CacheUpdate);

// Benchmark: Read from cache
static void BM_CacheRead(benchmark::State& state) {
    SymbolCache cache(100);
    cache.update_bid(0, 2450.25, 1000);
    
    for (auto _ : state) {
        MarketSnapshot snapshot = cache.get_snapshot(0);
        benchmark::DoNotOptimize(snapshot);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CacheRead);

// Benchmark: Concurrent updates (single writer)
static void BM_CacheUpdateBatch(benchmark::State& state) {
    SymbolCache cache(100);
    int num_symbols = state.range(0);
    
    for (auto _ : state) {
        for (int i = 0; i < num_symbols; ++i) {
            cache.update_bid(i % 100, 2450.25 + i, 1000 + i);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * num_symbols);
}
BENCHMARK(BM_CacheUpdateBatch)->Range(10, 1000);

// Benchmark: Mixed read/write workload
static void BM_CacheMixedWorkload(benchmark::State& state) {
    SymbolCache cache(100);
    
    // Pre-populate
    for (int i = 0; i < 100; ++i) {
        cache.update_bid(i, 2450.25, 1000);
    }
    
    for (auto _ : state) {
        // 70% reads, 30% writes
        for (int i = 0; i < 100; ++i) {
            if (i % 10 < 7) {
                MarketSnapshot snapshot = cache.get_snapshot(i % 100);
                benchmark::DoNotOptimize(snapshot);
            } else {
                cache.update_ask(i % 100, 2450.75, 1100);
            }
        }
    }
    
    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(BM_CacheMixedWorkload);

// Benchmark: Trade update
static void BM_CacheTradeUpdate(benchmark::State& state) {
    SymbolCache cache(100);
    
    for (auto _ : state) {
        cache.update_trade(0, 2450.50, 500);
        benchmark::ClobberMemory();
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CacheTradeUpdate);

// Benchmark: Cache statistics calculation
static void BM_CacheStatistics(benchmark::State& state) {
    SymbolCache cache(100);
    
    // Pre-populate with different update counts
    for (int i = 0; i < 100; ++i) {
        for (int j = 0; j < i * 10; ++j) {
            cache.update_bid(i, 2450.25 + j, 1000);
        }
    }
    
    for (auto _ : state) {
        uint64_t total = cache.get_total_updates();
        benchmark::DoNotOptimize(total);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CacheStatistics);

// Benchmark: Multi-threaded reads (1 writer, N readers)
static void BM_CacheMultiThreadRead(benchmark::State& state) {
    SymbolCache cache(100);
    
    // Pre-populate
    for (int i = 0; i < 100; ++i) {
        cache.update_bid(i, 2450.25, 1000);
    }
    
    if (state.thread_index == 0) {
        // Writer thread
        for (auto _ : state) {
            for (int i = 0; i < 100; ++i) {
                cache.update_bid(i, 2450.25 + state.iterations(), 1000);
            }
        }
    } else {
        // Reader threads
        for (auto _ : state) {
            for (int i = 0; i < 100; ++i) {
                MarketSnapshot snapshot = cache.get_snapshot(i);
                benchmark::DoNotOptimize(snapshot);
            }
        }
    }
    
    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(BM_CacheMultiThreadRead)->Threads(4)->UseRealTime();

BENCHMARK_MAIN();
