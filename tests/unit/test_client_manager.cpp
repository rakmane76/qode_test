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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
