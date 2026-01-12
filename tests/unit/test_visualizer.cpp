#include <gtest/gtest.h>
#include "client/visualizer.h"
#include "common/cache.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

using namespace mdfh;

class VisualizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create cache with test symbols
        cache_ = std::make_unique<SymbolCache>(num_symbols_);
        
        // Populate cache with test data
        for (uint16_t i = 0; i < num_symbols_; ++i) {
            double bid = 100.0 + i;
            double ask = 100.5 + i;
            double ltp = 100.25 + i;
            uint32_t qty = 1000 * (i + 1);
            
            cache_->update_quote(i, bid, qty, ask, qty);
            cache_->update_trade(i, ltp, qty);
        }
    }
    
    void TearDown() override {
        cache_.reset();
    }
    
    const size_t num_symbols_ = 10;
    std::unique_ptr<SymbolCache> cache_;
};

// Test: Visualizer construction and destruction
TEST_F(VisualizerTest, ConstructionDestruction) {
    EXPECT_NO_THROW({
        Visualizer viz(*cache_, num_symbols_);
    });
}

// Test: Start and stop visualizer
TEST_F(VisualizerTest, StartStop) {
    Visualizer viz(*cache_, num_symbols_);
    
    EXPECT_NO_THROW(viz.start());
    
    // Let it run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_NO_THROW(viz.stop());
}

// Test: Multiple start/stop cycles
TEST_F(VisualizerTest, MultipleStartStopCycles) {
    Visualizer viz(*cache_, num_symbols_);
    
    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW(viz.start());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        EXPECT_NO_THROW(viz.stop());
    }
}

// Test: Update statistics
TEST_F(VisualizerTest, UpdateStatistics) {
    Visualizer viz(*cache_, num_symbols_);
    
    LatencyStats stats;
    stats.sample_count = 1000;
    stats.min = 1000;
    stats.max = 50000;
    stats.mean = 10000;
    stats.p50 = 9500;
    stats.p95 = 20000;
    stats.p99 = 45000;
    stats.p999 = 48000;
    
    EXPECT_NO_THROW(viz.update_stats(5000, 10000, stats));
}

// Test: Set connection info
TEST_F(VisualizerTest, SetConnectionInfo) {
    Visualizer viz(*cache_, num_symbols_);
    
    EXPECT_NO_THROW(viz.set_connection_info("127.0.0.1", 9999, true));
    EXPECT_NO_THROW(viz.set_connection_info("localhost", 8888, false));
}

// Test: Visualizer with live updates
TEST_F(VisualizerTest, LiveDataUpdates) {
    Visualizer viz(*cache_, num_symbols_);
    
    viz.set_connection_info("127.0.0.1", 9999, true);
    viz.start();
    
    // Simulate live data updates
    for (int tick = 0; tick < 10; ++tick) {
        for (uint16_t i = 0; i < num_symbols_; ++i) {
            double bid = 100.0 + i + (tick * 0.1);
            double ask = 100.5 + i + (tick * 0.1);
            double ltp = 100.25 + i + (tick * 0.1);
            uint32_t qty = 1000 * (i + 1) + tick * 100;
            
            cache_->update_quote(i, bid, qty, ask, qty);
            cache_->update_trade(i, ltp, qty);
        }
        
        // Update stats
        LatencyStats stats;
        stats.sample_count = 1000 + tick * 100;
        stats.min = 1000;
        stats.max = 50000;
        stats.mean = 10000;
        stats.p50 = 9500;
        stats.p95 = 20000;
        stats.p99 = 45000;
        stats.p999 = 48000;
        
        viz.update_stats(1000 + tick * 100, 10000, stats);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    viz.stop();
}

// Test: Visualizer with empty cache
TEST_F(VisualizerTest, EmptyCache) {
    SymbolCache empty_cache(5);
    Visualizer viz(empty_cache, 5);
    
    viz.set_connection_info("127.0.0.1", 9999, false);
    
    EXPECT_NO_THROW(viz.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(viz.stop());
}

// Test: Visualizer with high frequency updates
TEST_F(VisualizerTest, HighFrequencyUpdates) {
    Visualizer viz(*cache_, num_symbols_);
    
    viz.set_connection_info("127.0.0.1", 9999, true);
    viz.start();
    
    // Rapid fire updates
    auto start_time = std::chrono::steady_clock::now();
    uint64_t update_count = 0;
    
    while (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(500)) {
        for (uint16_t i = 0; i < num_symbols_; ++i) {
            double bid = 100.0 + i + (rand() % 100) * 0.01;
            double ask = 100.5 + i + (rand() % 100) * 0.01;
            double ltp = 100.25 + i + (rand() % 100) * 0.01;
            uint32_t qty = 1000 * (i + 1) + rand() % 1000;
            
            cache_->update_quote(i, bid, qty, ask, qty);
            cache_->update_trade(i, ltp, qty);
            update_count++;
        }
        
        LatencyStats stats;
        stats.sample_count = update_count;
        stats.min = 1000 + rand() % 500;
        stats.max = 50000 + rand() % 10000;
        stats.mean = 10000 + rand() % 2000;
        stats.p50 = 9500;
        stats.p95 = 20000;
        stats.p99 = 45000;
        stats.p999 = 48000;
        
        viz.update_stats(update_count, 10000, stats);
    }
    
    viz.stop();
    
    EXPECT_GT(update_count, 0);
}

// Test: Visualizer with varying connection states
TEST_F(VisualizerTest, ConnectionStateChanges) {
    Visualizer viz(*cache_, num_symbols_);
    viz.start();
    
    // Connected
    viz.set_connection_info("127.0.0.1", 9999, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Disconnected
    viz.set_connection_info("127.0.0.1", 9999, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Reconnected with different port
    viz.set_connection_info("192.168.1.100", 8888, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    viz.stop();
}

// Test: Thread safety - concurrent updates
TEST_F(VisualizerTest, ConcurrentUpdates) {
    Visualizer viz(*cache_, num_symbols_);
    viz.set_connection_info("127.0.0.1", 9999, true);
    viz.start();
    
    std::atomic<bool> running(true);
    
    // Thread 1: Update cache
    std::thread cache_updater([this, &running]() {
        while (running) {
            for (uint16_t i = 0; i < num_symbols_; ++i) {
                double bid = 100.0 + i;
                double ask = 100.5 + i;
                double ltp = 100.25 + i;
                uint32_t qty = 1000 * (i + 1);
                
                cache_->update_quote(i, bid, qty, ask, qty);
                cache_->update_trade(i, ltp, qty);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Thread 2: Update stats
    std::thread stats_updater([&viz, &running]() {
        uint64_t msg_count = 0;
        while (running) {
            LatencyStats stats;
            stats.sample_count = msg_count++;
            stats.min = 1000;
            stats.max = 50000;
            stats.mean = 10000;
            stats.p50 = 9500;
            stats.p95 = 20000;
            stats.p99 = 45000;
            stats.p999 = 48000;
            
            viz.update_stats(msg_count, 10000, stats);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
    
    // Let threads run
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running = false;
    
    cache_updater.join();
    stats_updater.join();
    
    viz.stop();
}

// Test: Large number of symbols
TEST_F(VisualizerTest, LargeSymbolSet) {
    const size_t large_num_symbols = 100;
    SymbolCache large_cache(large_num_symbols);
    
    // Populate with data
    for (uint16_t i = 0; i < large_num_symbols; ++i) {
        double bid = 100.0 + i;
        double ask = 100.5 + i;
        double ltp = 100.25 + i;
        uint32_t qty = 1000 * (i + 1);
        
        large_cache.update_quote(i, bid, qty, ask, qty);
        large_cache.update_trade(i, ltp, qty);
    }
    
    Visualizer viz(large_cache, large_num_symbols);
    viz.set_connection_info("127.0.0.1", 9999, true);
    
    EXPECT_NO_THROW(viz.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_NO_THROW(viz.stop());
}

// Test: Stop without start should be safe
TEST_F(VisualizerTest, StopWithoutStart) {
    Visualizer viz(*cache_, num_symbols_);
    EXPECT_NO_THROW(viz.stop());
}

// Test: Double stop should be safe
TEST_F(VisualizerTest, DoubleStop) {
    Visualizer viz(*cache_, num_symbols_);
    
    viz.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_NO_THROW(viz.stop());
    EXPECT_NO_THROW(viz.stop()); // Second stop should be safe
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
