#include <gtest/gtest.h>
#include "common/cache.h"
#include <thread>
#include <vector>
#include <chrono>

using namespace mdfh;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class CacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = std::make_unique<SymbolCache>(100);
    }

    std::unique_ptr<SymbolCache> cache;
};

TEST_F(CacheTest, InitialStateZero) {
    MarketSnapshot state = cache->get_snapshot(0);
    
    EXPECT_DOUBLE_EQ(state.best_bid, 0.0);
    EXPECT_DOUBLE_EQ(state.best_ask, 0.0);
    EXPECT_EQ(state.bid_quantity, 0);
    EXPECT_EQ(state.ask_quantity, 0);
    EXPECT_DOUBLE_EQ(state.last_traded_price, 0.0);
    EXPECT_EQ(state.last_traded_quantity, 0);
}

TEST_F(CacheTest, UpdateBid) {
    cache->update_bid(0, 1500.25, 1000);
    
    MarketSnapshot state = cache->get_snapshot(0);
    EXPECT_DOUBLE_EQ(state.best_bid, 1500.25);
    EXPECT_EQ(state.bid_quantity, 1000);
    EXPECT_EQ(state.update_count, 1);
}

TEST_F(CacheTest, UpdateAsk) {
    cache->update_ask(0, 1500.75, 800);
    
    MarketSnapshot state = cache->get_snapshot(0);
    EXPECT_DOUBLE_EQ(state.best_ask, 1500.75);
    EXPECT_EQ(state.ask_quantity, 800);
    EXPECT_EQ(state.update_count, 1);
}

TEST_F(CacheTest, UpdateTrade) {
    cache->update_trade(0, 1500.50, 500);
    
    MarketSnapshot state = cache->get_snapshot(0);
    EXPECT_DOUBLE_EQ(state.last_traded_price, 1500.50);
    EXPECT_EQ(state.last_traded_quantity, 500);
    EXPECT_EQ(state.update_count, 1);
}

TEST_F(CacheTest, MultipleUpdates) {
    cache->update_bid(0, 1500.25, 1000);
    cache->update_ask(0, 1500.75, 800);
    cache->update_trade(0, 1500.50, 500);
    
    MarketSnapshot state = cache->get_snapshot(0);
    EXPECT_DOUBLE_EQ(state.best_bid, 1500.25);
    EXPECT_DOUBLE_EQ(state.best_ask, 1500.75);
    EXPECT_DOUBLE_EQ(state.last_traded_price, 1500.50);
    EXPECT_EQ(state.update_count, 3);
}

TEST_F(CacheTest, MultipleSymbols) {
    cache->update_bid(0, 1500.25, 1000);
    cache->update_bid(1, 2450.50, 1500);
    cache->update_bid(2, 3678.75, 2000);
    
    EXPECT_DOUBLE_EQ(cache->get_snapshot(0).best_bid, 1500.25);
    EXPECT_DOUBLE_EQ(cache->get_snapshot(1).best_bid, 2450.50);
    EXPECT_DOUBLE_EQ(cache->get_snapshot(2).best_bid, 3678.75);
}

TEST_F(CacheTest, ConcurrentWriteRead) {
    const int num_updates = 100000;
    
    std::thread writer([&]() {
        for (int i = 0; i < num_updates; ++i) {
            cache->update_bid(0, 1500.0 + i * 0.01, 1000 + i);
            cache->update_ask(0, 1500.5 + i * 0.01, 800 + i);
        }
    });
    
    std::thread reader([&]() {
        for (int i = 0; i < num_updates; ++i) {
            MarketSnapshot state = cache->get_snapshot(0);
            // Should never see torn reads
            EXPECT_LE(state.best_bid, state.best_ask);
        }
    });
    
    writer.join();
    reader.join();
    
    MarketSnapshot final_state = cache->get_snapshot(0);
    EXPECT_EQ(final_state.update_count, num_updates * 2);
}

TEST_F(CacheTest, ReadLatency) {
    cache->update_bid(0, 1500.25, 1000);
    cache->update_ask(0, 1500.75, 800);
    
    const int iterations = 1000000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        volatile MarketSnapshot state = cache->get_snapshot(0);
        (void)state;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    double avg_latency = static_cast<double>(duration) / iterations;
    std::cout << "Average read latency: " << avg_latency << " ns" << std::endl;
    
    EXPECT_LT(avg_latency, 50.0) << "Read latency should be < 50ns";
}

TEST_F(CacheTest, WriteLatency) {
    const int iterations = 100000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        cache->update_bid(0, 1500.0 + i * 0.01, 1000);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    double avg_latency = static_cast<double>(duration) / iterations;
    std::cout << "Average write latency: " << avg_latency << " ns" << std::endl;
    
    double writes_per_sec = iterations / (duration / 1e9);
    std::cout << "Write throughput: " << writes_per_sec << " updates/sec" << std::endl;
    
    EXPECT_GT(writes_per_sec, 100000.0) << "Should support > 100K updates/sec";
}

TEST_F(CacheTest, NoTornReads) {
    cache->update_bid(0, 1500.25, 1000);
    cache->update_ask(0, 1500.75, 800);
    
    std::atomic<bool> stop{false};
    std::atomic<int> torn_read_count{0};
    
    std::thread writer([&]() {
        int counter = 0;
        while (!stop) {
            // Use update_quote to atomically update both bid and ask
            cache->update_quote(0, 1500.0 + counter, counter, 1500.5 + counter, counter);
            counter++;
        }
    });
    
    std::thread reader([&]() {
        while (!stop) {
            MarketSnapshot state = cache->get_snapshot(0);
            if (state.bid_quantity != state.ask_quantity) {
                torn_read_count++;
            }
        }
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;
    
    writer.join();
    reader.join();
    
    EXPECT_EQ(torn_read_count, 0) << "Should have no torn reads";
}

TEST_F(CacheTest, MultipleReaders) {
    const int num_readers = 4;
    const int num_updates = 10000;
    
    std::thread writer([&]() {
        for (int i = 0; i < num_updates; ++i) {
            cache->update_bid(0, 1500.0 + i * 0.01, 1000 + i);
        }
    });
    
    std::vector<std::thread> readers;
    for (int i = 0; i < num_readers; ++i) {
        readers.emplace_back([&]() {
            for (int j = 0; j < num_updates; ++j) {
                MarketSnapshot state = cache->get_snapshot(0);
                (void)state;
            }
        });
    }
    
    writer.join();
    for (auto& reader : readers) {
        reader.join();
    }
    
    EXPECT_EQ(cache->get_snapshot(0).update_count, num_updates);
}
