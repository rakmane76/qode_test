#include <benchmark/benchmark.h>
#include "common/latency_tracker.h"
#include <random>
#include <thread>

using namespace mdfh;

// Benchmark: Single latency recording
static void BM_RecordLatency(benchmark::State& state) {
    LatencyTracker tracker;
    
    for (auto _ : state) {
        tracker.record(15000);  // 15 microseconds
        benchmark::ClobberMemory();
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RecordLatency);

// Benchmark: Batch recording
static void BM_RecordLatencyBatch(benchmark::State& state) {
    LatencyTracker tracker;
    int batch_size = state.range(0);
    
    std::mt19937 rng(42);
    std::normal_distribution<double> dist(15000, 5000);  // mean 15us, stddev 5us
    
    for (auto _ : state) {
        for (int i = 0; i < batch_size; ++i) {
            uint64_t latency = std::max(0.0, dist(rng));
            tracker.record(latency);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_RecordLatencyBatch)->Range(100, 10000);

// Benchmark: Get statistics
static void BM_GetStatistics(benchmark::State& state) {
    LatencyTracker tracker;
    
    // Pre-populate with realistic data
    std::mt19937 rng(42);
    std::normal_distribution<double> dist(15000, 5000);
    
    for (int i = 0; i < 100000; ++i) {
        uint64_t latency = std::max(0.0, dist(rng));
        tracker.record(latency);
    }
    
    for (auto _ : state) {
        LatencyStats stats = tracker.get_stats();
        benchmark::DoNotOptimize(stats);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GetStatistics);

// Benchmark: Concurrent recording (multi-threaded)
static void BM_RecordLatencyConcurrent(benchmark::State& state) {
    static LatencyTracker tracker;
    
    std::mt19937 rng(42 + state.thread_index);
    std::normal_distribution<double> dist(15000, 5000);
    
    for (auto _ : state) {
        uint64_t latency = std::max(0.0, dist(rng));
        tracker.record(latency);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RecordLatencyConcurrent)->Threads(4)->UseRealTime();

// Benchmark: Percentile calculation accuracy vs speed tradeoff
static void BM_PercentileCalculation(benchmark::State& state) {
    LatencyTracker tracker;
    
    // Create a known distribution
    for (int i = 0; i < 100000; ++i) {
        tracker.record(i * 100);  // 0, 100, 200, ... nanoseconds
    }
    
    for (auto _ : state) {
        LatencyStats stats = tracker.get_stats();
        benchmark::DoNotOptimize(stats.p50);
        benchmark::DoNotOptimize(stats.p95);
        benchmark::DoNotOptimize(stats.p99);
        benchmark::DoNotOptimize(stats.p999);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_PercentileCalculation);

// Benchmark: Export histogram
static void BM_ExportHistogram(benchmark::State& state) {
    LatencyTracker tracker;
    
    std::mt19937 rng(42);
    std::normal_distribution<double> dist(15000, 5000);
    
    for (int i = 0; i < 100000; ++i) {
        uint64_t latency = std::max(0.0, dist(rng));
        tracker.record(latency);
    }
    
    for (auto _ : state) {
        bool success = tracker.export_to_csv("/tmp/benchmark_histogram.csv");
        benchmark::DoNotOptimize(success);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ExportHistogram);

// Benchmark: Ring buffer wraparound performance
static void BM_RingBufferWraparound(benchmark::State& state) {
    LatencyTracker tracker;
    
    // Fill beyond capacity to test wraparound
    for (int i = 0; i < 2000000; ++i) {
        tracker.record(15000 + (i % 1000));
    }
    
    for (auto _ : state) {
        tracker.record(15000);
        benchmark::ClobberMemory();
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RingBufferWraparound);

// Benchmark: Reset operation
static void BM_Reset(benchmark::State& state) {
    LatencyTracker tracker;
    
    for (auto _ : state) {
        state.PauseTiming();
        // Fill with data
        for (int i = 0; i < 10000; ++i) {
            tracker.record(15000 + i);
        }
        state.ResumeTiming();
        
        tracker.reset();
        benchmark::ClobberMemory();
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Reset);

BENCHMARK_MAIN();
