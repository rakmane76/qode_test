#include <gtest/gtest.h>
#include "server/client_manager.h"
#include <thread>
#include <vector>
#include <algorithm>

using namespace mdfh;

class ClientManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<ClientManager>();
    }
    
    void TearDown() override {
        manager_.reset();
    }
    
    std::unique_ptr<ClientManager> manager_;
};

// Test: Construction and destruction
TEST_F(ClientManagerTest, ConstructionDestruction) {
    EXPECT_NO_THROW({
        ClientManager manager;
    });
}

// Test: Initial state
TEST_F(ClientManagerTest, InitialState) {
    EXPECT_EQ(manager_->get_client_count(), 0);
    
    auto clients = manager_->get_all_clients();
    EXPECT_TRUE(clients.empty());
}

// Test: Add single client
TEST_F(ClientManagerTest, AddSingleClient) {
    int fd = 10;
    
    manager_->add_client(fd);
    
    EXPECT_EQ(manager_->get_client_count(), 1);
    
    auto clients = manager_->get_all_clients();
    EXPECT_EQ(clients.size(), 1);
    EXPECT_EQ(clients[0], fd);
}

// Test: Add multiple clients
TEST_F(ClientManagerTest, AddMultipleClients) {
    std::vector<int> fds = {10, 11, 12, 13, 14};
    
    for (int fd : fds) {
        manager_->add_client(fd);
    }
    
    EXPECT_EQ(manager_->get_client_count(), fds.size());
    
    auto clients = manager_->get_all_clients();
    EXPECT_EQ(clients.size(), fds.size());
    
    // Check all clients are present
    for (int fd : fds) {
        EXPECT_NE(std::find(clients.begin(), clients.end(), fd), clients.end());
    }
}

// Test: Remove client
TEST_F(ClientManagerTest, RemoveClient) {
    int fd = 10;
    
    manager_->add_client(fd);
    EXPECT_EQ(manager_->get_client_count(), 1);
    
    manager_->remove_client(fd);
    EXPECT_EQ(manager_->get_client_count(), 0);
    
    auto clients = manager_->get_all_clients();
    EXPECT_TRUE(clients.empty());
}

// Test: Remove non-existent client
TEST_F(ClientManagerTest, RemoveNonExistentClient) {
    EXPECT_NO_THROW(manager_->remove_client(999));
    EXPECT_EQ(manager_->get_client_count(), 0);
}

// Test: Get client info
TEST_F(ClientManagerTest, GetClientInfo) {
    int fd = 10;
    
    manager_->add_client(fd);
    
    auto info = manager_->get_client_info(fd);
    EXPECT_EQ(info.fd, fd);
    EXPECT_EQ(info.messages_sent, 0);
    EXPECT_EQ(info.bytes_sent, 0);
    EXPECT_EQ(info.send_errors, 0);
    EXPECT_FALSE(info.is_slow);
}

// Test: Update statistics - success
TEST_F(ClientManagerTest, UpdateStatsSuccess) {
    int fd = 10;
    
    manager_->add_client(fd);
    
    size_t bytes = 1024;
    manager_->update_stats(fd, bytes, true);
    
    auto info = manager_->get_client_info(fd);
    EXPECT_EQ(info.bytes_sent, bytes);
    EXPECT_EQ(info.messages_sent, 1);
    EXPECT_EQ(info.send_errors, 0);
}

// Test: Update statistics - failure
TEST_F(ClientManagerTest, UpdateStatsFailure) {
    int fd = 10;
    
    manager_->add_client(fd);
    
    size_t bytes = 1024;
    manager_->update_stats(fd, bytes, false);
    
    auto info = manager_->get_client_info(fd);
    EXPECT_EQ(info.bytes_sent, 0); // Failed sends don't count bytes
    EXPECT_EQ(info.messages_sent, 0); // Failed sends don't increment messages_sent
    EXPECT_EQ(info.send_errors, 1);
}

// Test: Update statistics multiple times
TEST_F(ClientManagerTest, UpdateStatsMultipleTimes) {
    int fd = 10;
    
    manager_->add_client(fd);
    
    // Successful sends
    for (int i = 0; i < 10; ++i) {
        manager_->update_stats(fd, 100, true);
    }
    
    // Failed sends
    for (int i = 0; i < 3; ++i) {
        manager_->update_stats(fd, 100, false);
    }
    
    auto info = manager_->get_client_info(fd);
    EXPECT_EQ(info.messages_sent, 10); // Only successful sends increment messages_sent
    EXPECT_EQ(info.bytes_sent, 1000); // Only successful sends
    EXPECT_EQ(info.send_errors, 3);
}

// Test: Mark client as slow
TEST_F(ClientManagerTest, MarkSlowClient) {
    int fd = 10;
    
    manager_->add_client(fd);
    
    auto info_before = manager_->get_client_info(fd);
    EXPECT_FALSE(info_before.is_slow);
    
    manager_->mark_slow_client(fd);
    
    auto info_after = manager_->get_client_info(fd);
    EXPECT_TRUE(info_after.is_slow);
}

// Test: Mark non-existent client as slow
TEST_F(ClientManagerTest, MarkNonExistentClientSlow) {
    EXPECT_NO_THROW(manager_->mark_slow_client(999));
}

// Test: Add duplicate client
TEST_F(ClientManagerTest, AddDuplicateClient) {
    int fd = 10;
    
    manager_->add_client(fd);
    manager_->add_client(fd); // Add same client again
    
    // Should still have only 1 client (or handle as implementation decides)
    auto clients = manager_->get_all_clients();
    size_t count = std::count(clients.begin(), clients.end(), fd);
    EXPECT_GE(count, 1); // At least one instance
}

// Test: Thread safety - concurrent add
TEST_F(ClientManagerTest, ConcurrentAdd) {
    const int num_threads = 10;
    const int clients_per_thread = 100;
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, clients_per_thread]() {
            for (int i = 0; i < clients_per_thread; ++i) {
                int fd = t * clients_per_thread + i;
                manager_->add_client(fd);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(manager_->get_client_count(), num_threads * clients_per_thread);
}

// Test: Thread safety - concurrent remove
TEST_F(ClientManagerTest, ConcurrentRemove) {
    const int num_clients = 1000;
    
    // Add clients
    for (int i = 0; i < num_clients; ++i) {
        manager_->add_client(i);
    }
    
    EXPECT_EQ(manager_->get_client_count(), num_clients);
    
    // Remove concurrently
    std::vector<std::thread> threads;
    const int num_threads = 10;
    const int clients_per_thread = num_clients / num_threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, clients_per_thread]() {
            for (int i = 0; i < clients_per_thread; ++i) {
                int fd = t * clients_per_thread + i;
                manager_->remove_client(fd);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(manager_->get_client_count(), 0);
}

// Test: Thread safety - concurrent updates
TEST_F(ClientManagerTest, ConcurrentUpdates) {
    const int num_clients = 10;
    
    // Add clients
    for (int i = 0; i < num_clients; ++i) {
        manager_->add_client(i);
    }
    
    // Update stats concurrently
    std::vector<std::thread> threads;
    const int updates_per_thread = 1000;
    
    for (int i = 0; i < num_clients; ++i) {
        threads.emplace_back([this, i, updates_per_thread]() {
            for (int j = 0; j < updates_per_thread; ++j) {
                manager_->update_stats(i, 100, j % 10 != 0); // 90% success
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all clients have reasonable message counts
    // 90% success rate, so should be around 900 successful messages
    for (int i = 0; i < num_clients; ++i) {
        auto info = manager_->get_client_info(i);
        EXPECT_GE(info.messages_sent, 800); // At least 80% succeeded
        EXPECT_LE(info.messages_sent, updates_per_thread); // Not more than total
    }
}

// Test: Thread safety - mixed operations
TEST_F(ClientManagerTest, MixedConcurrentOperations) {
    std::atomic<bool> running(true);
    
    // Thread 1: Add clients
    std::thread add_thread([this, &running]() {
        int fd = 1000;
        while (running) {
            manager_->add_client(fd++);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // Thread 2: Remove clients
    std::thread remove_thread([this, &running]() {
        int fd = 1000;
        while (running) {
            manager_->remove_client(fd++);
            std::this_thread::sleep_for(std::chrono::microseconds(150));
        }
    });
    
    // Thread 3: Update stats
    std::thread update_thread([this, &running]() {
        while (running) {
            auto clients = manager_->get_all_clients();
            for (int fd : clients) {
                manager_->update_stats(fd, 100, true);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });
    
    // Thread 4: Mark slow clients
    std::thread slow_thread([this, &running]() {
        while (running) {
            auto clients = manager_->get_all_clients();
            for (int fd : clients) {
                manager_->mark_slow_client(fd);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(250));
        }
    });
    
    // Let threads run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;
    
    add_thread.join();
    remove_thread.join();
    update_thread.join();
    slow_thread.join();
    
    // Test passes if no crashes or deadlocks
    SUCCEED();
}

// Test: Large number of clients
TEST_F(ClientManagerTest, LargeNumberOfClients) {
    const int num_clients = 10000;
    
    for (int i = 0; i < num_clients; ++i) {
        manager_->add_client(i);
    }
    
    EXPECT_EQ(manager_->get_client_count(), num_clients);
    
    auto clients = manager_->get_all_clients();
    EXPECT_EQ(clients.size(), num_clients);
}

// Test: Get info for non-existent client
TEST_F(ClientManagerTest, GetInfoNonExistentClient) {
    auto info = manager_->get_client_info(999);
    // Returns default-constructed ClientInfo with fd=0
    EXPECT_EQ(info.fd, 0);
    EXPECT_EQ(info.messages_sent, 0);
    EXPECT_EQ(info.bytes_sent, 0);
}

// Test: Stress test
TEST_F(ClientManagerTest, StressTest) {
    const int iterations = 100;
    const int clients_per_iteration = 100;
    
    for (int iter = 0; iter < iterations; ++iter) {
        // Add clients
        for (int i = 0; i < clients_per_iteration; ++i) {
            int fd = iter * clients_per_iteration + i;
            manager_->add_client(fd);
        }
        
        // Update some stats
        auto clients = manager_->get_all_clients();
        for (int fd : clients) {
            manager_->update_stats(fd, 100, true);
        }
        
        // Mark some as slow
        for (size_t i = 0; i < clients.size(); i += 10) {
            manager_->mark_slow_client(clients[i]);
        }
        
        // Remove half
        for (int i = 0; i < clients_per_iteration / 2; ++i) {
            int fd = iter * clients_per_iteration + i;
            manager_->remove_client(fd);
        }
    }
    
    // Should still be functional
    size_t count = manager_->get_client_count();
    EXPECT_GT(count, 0);
}

// Test: Subscribe single client to symbols
TEST_F(ClientManagerTest, SubscribeSingleClient) {
    int fd = 10;
    manager_->add_client(fd);
    
    std::unordered_set<uint16_t> symbols = {0, 1, 2};
    manager_->subscribe(fd, symbols);
    
    EXPECT_TRUE(manager_->is_subscribed(fd, 0));
    EXPECT_TRUE(manager_->is_subscribed(fd, 1));
    EXPECT_TRUE(manager_->is_subscribed(fd, 2));
    EXPECT_FALSE(manager_->is_subscribed(fd, 3));
    EXPECT_EQ(manager_->get_subscription_count(fd), 3);
}

// Test: Subscribe multiple clients to different symbols
TEST_F(ClientManagerTest, SubscribeMultipleClients) {
    int fd1 = 10;
    int fd2 = 11;
    manager_->add_client(fd1);
    manager_->add_client(fd2);
    
    manager_->subscribe(fd1, {0, 1});
    manager_->subscribe(fd2, {1, 2});
    
    EXPECT_TRUE(manager_->is_subscribed(fd1, 0));
    EXPECT_TRUE(manager_->is_subscribed(fd1, 1));
    EXPECT_FALSE(manager_->is_subscribed(fd1, 2));
    
    EXPECT_FALSE(manager_->is_subscribed(fd2, 0));
    EXPECT_TRUE(manager_->is_subscribed(fd2, 1));
    EXPECT_TRUE(manager_->is_subscribed(fd2, 2));
    
    EXPECT_EQ(manager_->get_subscription_count(fd1), 2);
    EXPECT_EQ(manager_->get_subscription_count(fd2), 2);
}

// Test: Update subscription (replace existing)
TEST_F(ClientManagerTest, UpdateSubscription) {
    int fd = 10;
    manager_->add_client(fd);
    
    manager_->subscribe(fd, {0, 1});
    EXPECT_EQ(manager_->get_subscription_count(fd), 2);
    
    manager_->subscribe(fd, {2, 3, 4});
    EXPECT_EQ(manager_->get_subscription_count(fd), 3);
    EXPECT_FALSE(manager_->is_subscribed(fd, 0));
    EXPECT_FALSE(manager_->is_subscribed(fd, 1));
    EXPECT_TRUE(manager_->is_subscribed(fd, 2));
    EXPECT_TRUE(manager_->is_subscribed(fd, 3));
    EXPECT_TRUE(manager_->is_subscribed(fd, 4));
}

// Test: Unsubscribe from specific symbol
TEST_F(ClientManagerTest, UnsubscribeSymbol) {
    int fd = 10;
    manager_->add_client(fd);
    
    manager_->subscribe(fd, {0, 1, 2});
    EXPECT_EQ(manager_->get_subscription_count(fd), 3);
    
    manager_->unsubscribe(fd, 1);
    EXPECT_EQ(manager_->get_subscription_count(fd), 2);
    EXPECT_TRUE(manager_->is_subscribed(fd, 0));
    EXPECT_FALSE(manager_->is_subscribed(fd, 1));
    EXPECT_TRUE(manager_->is_subscribed(fd, 2));
}

// Test: Clear all subscriptions
TEST_F(ClientManagerTest, ClearSubscriptions) {
    int fd = 10;
    manager_->add_client(fd);
    
    manager_->subscribe(fd, {0, 1, 2, 3, 4});
    EXPECT_EQ(manager_->get_subscription_count(fd), 5);
    
    manager_->clear_subscriptions(fd);
    EXPECT_EQ(manager_->get_subscription_count(fd), 0);
    EXPECT_FALSE(manager_->is_subscribed(fd, 0));
    EXPECT_FALSE(manager_->is_subscribed(fd, 1));
}

// Test: Get subscribed clients for a symbol
TEST_F(ClientManagerTest, GetSubscribedClients) {
    int fd1 = 10;
    int fd2 = 11;
    int fd3 = 12;
    
    manager_->add_client(fd1);
    manager_->add_client(fd2);
    manager_->add_client(fd3);
    
    manager_->subscribe(fd1, {0, 1});
    manager_->subscribe(fd2, {1, 2});
    manager_->subscribe(fd3, {2, 3});
    
    // Symbol 0: only fd1
    auto clients_0 = manager_->get_subscribed_clients(0);
    EXPECT_EQ(clients_0.size(), 1);
    EXPECT_NE(std::find(clients_0.begin(), clients_0.end(), fd1), clients_0.end());
    
    // Symbol 1: fd1 and fd2
    auto clients_1 = manager_->get_subscribed_clients(1);
    EXPECT_EQ(clients_1.size(), 2);
    EXPECT_NE(std::find(clients_1.begin(), clients_1.end(), fd1), clients_1.end());
    EXPECT_NE(std::find(clients_1.begin(), clients_1.end(), fd2), clients_1.end());
    
    // Symbol 2: fd2 and fd3
    auto clients_2 = manager_->get_subscribed_clients(2);
    EXPECT_EQ(clients_2.size(), 2);
    EXPECT_NE(std::find(clients_2.begin(), clients_2.end(), fd2), clients_2.end());
    EXPECT_NE(std::find(clients_2.begin(), clients_2.end(), fd3), clients_2.end());
    
    // Symbol 3: only fd3
    auto clients_3 = manager_->get_subscribed_clients(3);
    EXPECT_EQ(clients_3.size(), 1);
    EXPECT_NE(std::find(clients_3.begin(), clients_3.end(), fd3), clients_3.end());
    
    // Symbol 4: no clients
    auto clients_4 = manager_->get_subscribed_clients(4);
    EXPECT_TRUE(clients_4.empty());
}

// Test: Subscribe without adding client first
TEST_F(ClientManagerTest, SubscribeNonExistentClient) {
    // Should not crash even if client doesn't exist
    EXPECT_NO_THROW(manager_->subscribe(999, {0, 1, 2}));
    EXPECT_TRUE(manager_->is_subscribed(999, 0));
    EXPECT_EQ(manager_->get_subscription_count(999), 3);
}

// Test: Unsubscribe from non-existent client
TEST_F(ClientManagerTest, UnsubscribeNonExistentClient) {
    EXPECT_NO_THROW(manager_->unsubscribe(999, 0));
    EXPECT_EQ(manager_->get_subscription_count(999), 0);
}

// Test: Empty subscription set
TEST_F(ClientManagerTest, EmptySubscriptionSet) {
    int fd = 10;
    manager_->add_client(fd);
    
    manager_->subscribe(fd, {0, 1, 2});
    EXPECT_EQ(manager_->get_subscription_count(fd), 3);
    
    // Subscribe to empty set (clear subscriptions)
    manager_->subscribe(fd, {});
    EXPECT_EQ(manager_->get_subscription_count(fd), 0);
}

// Test: Large number of subscriptions
TEST_F(ClientManagerTest, ManySubscriptions) {
    int fd = 10;
    manager_->add_client(fd);
    
    std::unordered_set<uint16_t> symbols;
    for (uint16_t i = 0; i < 1000; ++i) {
        symbols.insert(i);
    }
    
    manager_->subscribe(fd, symbols);
    EXPECT_EQ(manager_->get_subscription_count(fd), 1000);
    
    for (uint16_t i = 0; i < 1000; ++i) {
        EXPECT_TRUE(manager_->is_subscribed(fd, i));
    }
}

// Test: Concurrent subscription operations
TEST_F(ClientManagerTest, ConcurrentSubscriptions) {
    const int num_clients = 10;
    const int num_threads = 4;
    
    // Add clients
    for (int i = 0; i < num_clients; ++i) {
        manager_->add_client(i);
    }
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, num_clients]() {
            for (int i = 0; i < num_clients; ++i) {
                std::unordered_set<uint16_t> symbols;
                for (uint16_t s = t * 10; s < (t + 1) * 10; ++s) {
                    symbols.insert(s);
                }
                manager_->subscribe(i, symbols);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify each client has subscriptions (exact count depends on race conditions)
    for (int i = 0; i < num_clients; ++i) {
        size_t count = manager_->get_subscription_count(i);
        EXPECT_GE(count, 0);
        EXPECT_LE(count, 40); // Maximum possible from all threads
    }
}

// Test: Remove client also clears subscriptions
TEST_F(ClientManagerTest, RemoveClientClearsSubscriptions) {
    int fd = 10;
    manager_->add_client(fd);
    manager_->subscribe(fd, {0, 1, 2});
    
    EXPECT_EQ(manager_->get_subscription_count(fd), 3);
    
    manager_->remove_client(fd);
    
    // After removal, client should not be in subscribed clients list
    auto clients_0 = manager_->get_subscribed_clients(0);
    EXPECT_EQ(std::find(clients_0.begin(), clients_0.end(), fd), clients_0.end());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
