#include <gtest/gtest.h>
#include "common/latency_tracker.h"
#include <thread>
#include <vector>
#include <random>

using namespace mdfh;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class LatencyTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tracker = std::make_unique<LatencyTracker>();
    }

    std::unique_ptr<LatencyTracker> tracker;
};

TEST_F(LatencyTrackerTest, InitialState) {
    auto stats = tracker->get_stats();
    
    EXPECT_EQ(stats.sample_count, 0);
    EXPECT_EQ(stats.min, 0);
    EXPECT_EQ(stats.max, 0);
    EXPECT_EQ(stats.mean, 0);
}

TEST_F(LatencyTrackerTest, RecordSingleSample) {
    tracker->record(1000);
    
    auto stats = tracker->get_stats();
    EXPECT_EQ(stats.sample_count, 1);
    EXPECT_EQ(stats.min, 1000);
    EXPECT_EQ(stats.max, 1000);
    EXPECT_EQ(stats.mean, 1000);
}

TEST_F(LatencyTrackerTest, RecordMultipleSamples) {
    tracker->record(1000);
    tracker->record(2000);
    tracker->record(3000);
    
    auto stats = tracker->get_stats();
    EXPECT_EQ(stats.sample_count, 3);
    EXPECT_EQ(stats.min, 1000);
    EXPECT_EQ(stats.max, 3000);
    EXPECT_EQ(stats.mean, 2000);
}

TEST_F(LatencyTrackerTest, PercentilesCalculation) {
    // Record 1000 samples from 1 to 1000
    for (int i = 1; i <= 1000; ++i) {
        tracker->record(i);
    }
    
    auto stats = tracker->get_stats();
    EXPECT_EQ(stats.sample_count, 1000);
    
    // Allow some tolerance for percentile approximation
    EXPECT_NEAR(stats.p50, 500, 50);
    EXPECT_NEAR(stats.p95, 950, 50);
    EXPECT_NEAR(stats.p99, 990, 20);
    EXPECT_NEAR(stats.p999, 999, 10);
}

TEST_F(LatencyTrackerTest, RingBufferWrap) {
    // Record more than the buffer size
    for (int i = 0; i < 1500000; ++i) {
        tracker->record(i);
    }
    
    auto stats = tracker->get_stats();
    // Should keep samples up to the buffer size (rounded to power of 2)
    // Original request was 1M, but it gets rounded to 2^20 = 1048576
    EXPECT_LE(stats.sample_count, 1048576);
}

TEST_F(LatencyTrackerTest, ConcurrentRecording) {
    const int num_threads = 4;
    const int samples_per_thread = 10000;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < samples_per_thread; ++j) {
                tracker->record(i * 1000 + j);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto stats = tracker->get_stats();
    EXPECT_EQ(stats.sample_count, num_threads * samples_per_thread);
}

TEST_F(LatencyTrackerTest, RecordOverhead) {
    const int iterations = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        tracker->record(1000);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double avg_overhead = static_cast<double>(duration) / iterations;
    
    std::cout << "Average record overhead: " << avg_overhead << " ns" << std::endl;
    
#ifdef DEBUG
    // Debug build may be slower, allow up to 50ns
    EXPECT_LT(avg_overhead, 50.0) << "Record overhead should be < 50ns (debug build)";
#else
    // Release build should be < 30ns
    EXPECT_LT(avg_overhead, 30.0) << "Record overhead should be < 30ns";
#endif
}

TEST_F(LatencyTrackerTest, RealisticLatencyDistribution) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dist(15000, 5000); // mean=15us, stddev=5us
    
    for (int i = 0; i < 100000; ++i) {
        uint64_t latency = std::max(0.0, dist(gen));
        tracker->record(latency);
    }
    
    auto stats = tracker->get_stats();
    EXPECT_EQ(stats.sample_count, 100000);
    EXPECT_NEAR(stats.mean, 15000, 500);
    EXPECT_LT(stats.p99, stats.max);
    EXPECT_GT(stats.p99, stats.p95);
}

TEST_F(LatencyTrackerTest, ExportHistogram) {
    for (int i = 1; i <= 10000; ++i) {
        tracker->record(i);
    }
    
    bool exported = tracker->export_to_csv("test_histogram.csv");
    EXPECT_TRUE(exported);
}

TEST_F(LatencyTrackerTest, Reset) {
    tracker->record(1000);
    tracker->record(2000);
    tracker->record(3000);
    
    tracker->reset();
    
    auto stats = tracker->get_stats();
    EXPECT_EQ(stats.sample_count, 0);
}

TEST_F(LatencyTrackerTest, ExtremeValues) {
    tracker->record(1); // Very low
    tracker->record(1000000000); // Very high
    
    auto stats = tracker->get_stats();
    EXPECT_EQ(stats.min, 1);
    EXPECT_EQ(stats.max, 1000000000);
}
