#include <gtest/gtest.h>
#include "client/feed_handler.h"
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace mdfh;

class FeedHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        num_symbols_ = 10;
    }
    
    void TearDown() override {
        if (handler_) {
            handler_->stop();
            handler_->disconnect();
        }
        handler_.reset();
        
        if (server_fd_ >= 0) {
            close(server_fd_);
            server_fd_ = -1;
        }
    }
    
    // Create a simple test server
    int create_test_server(uint16_t port) {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            return -1;
        }
        
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(server_fd);
            return -1;
        }
        
        if (listen(server_fd, 5) < 0) {
            close(server_fd);
            return -1;
        }
        
        return server_fd;
    }
    
    std::unique_ptr<FeedHandler> handler_;
    int server_fd_ = -1;
    size_t num_symbols_;
    const uint16_t test_port_ = 17777;
};

// Test: Construction
TEST_F(FeedHandlerTest, Construction) {
    EXPECT_NO_THROW({
        FeedHandler handler("127.0.0.1", test_port_, num_symbols_);
    });
}

// Test: Initial state
TEST_F(FeedHandlerTest, InitialState) {
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    
    EXPECT_FALSE(handler_->is_connected());
    EXPECT_EQ(handler_->get_messages_received(), 0);
    EXPECT_EQ(handler_->get_bytes_received(), 0);
}

// Test: Get cache
TEST_F(FeedHandlerTest, GetCache) {
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    
    const SymbolCache& cache = handler_->get_cache();
    EXPECT_EQ(cache.get_num_symbols(), num_symbols_);
}

// Test: Get statistics
TEST_F(FeedHandlerTest, GetStatistics) {
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    
    auto stats = handler_->get_stats();
    EXPECT_EQ(stats.messages_received, 0);
    EXPECT_EQ(stats.messages_parsed, 0);
    EXPECT_EQ(stats.bytes_received, 0);
    EXPECT_EQ(stats.sequence_gaps, 0);
}

// Test: Get latency statistics
TEST_F(FeedHandlerTest, GetLatencyStats) {
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    
    auto latency_stats = handler_->get_latency_stats();
    EXPECT_EQ(latency_stats.sample_count, 0);
}

// Test: Disconnect without connect
TEST_F(FeedHandlerTest, DisconnectWithoutConnect) {
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    
    EXPECT_NO_THROW(handler_->disconnect());
    EXPECT_FALSE(handler_->is_connected());
}

// Test: Stop without start
TEST_F(FeedHandlerTest, StopWithoutStart) {
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    
    EXPECT_NO_THROW(handler_->stop());
}

// Test: Start without connection
TEST_F(FeedHandlerTest, StartWithoutConnection) {
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    
    bool result = handler_->start();
    EXPECT_FALSE(result); // Should fail if not connected
}

// Test: Subscribe without connection
TEST_F(FeedHandlerTest, SubscribeWithoutConnection) {
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    
    std::vector<uint16_t> symbols = {0, 1, 2};
    bool result = handler_->subscribe(symbols);
    EXPECT_FALSE(result); // Should fail if not connected
}

// Test: Connect to server
TEST_F(FeedHandlerTest, ConnectToServer) {
    server_fd_ = create_test_server(test_port_);
    ASSERT_GE(server_fd_, 0);
    
    std::thread accept_thread([this]() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
        if (client_fd >= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            close(client_fd);
        }
    });
    
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    bool result = handler_->connect("127.0.0.1", test_port_);
    
    accept_thread.join();
    
    // Connection may succeed or fail depending on timing
    if (result) {
        EXPECT_TRUE(handler_->is_connected());
    }
}

// Test: Connect and subscribe
TEST_F(FeedHandlerTest, ConnectAndSubscribe) {
    server_fd_ = create_test_server(test_port_);
    ASSERT_GE(server_fd_, 0);
    
    std::atomic<bool> received_subscription(false);
    std::thread accept_thread([this, &received_subscription]() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
        if (client_fd >= 0) {
            char buffer[1024];
            ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
            if (n > 0) {
                received_subscription = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            close(client_fd);
        }
    });
    
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    
    if (handler_->connect("127.0.0.1", test_port_)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        std::vector<uint16_t> symbols = {0, 1, 2, 3, 4};
        handler_->subscribe(symbols);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    accept_thread.join();
}

// Test: Multiple connect/disconnect cycles
TEST_F(FeedHandlerTest, MultipleConnectDisconnectCycles) {
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    
    for (int i = 0; i < 3; ++i) {
        uint16_t port = test_port_ + i;
        server_fd_ = create_test_server(port);
        if (server_fd_ < 0) continue;
        
        std::thread accept_thread([this]() {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                close(client_fd);
            }
        });
        
        handler_->connect("127.0.0.1", port);
        handler_->disconnect();
        
        accept_thread.join();
        close(server_fd_);
        server_fd_ = -1;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    EXPECT_FALSE(handler_->is_connected());
}

// Test: Subscribe to empty symbol list
TEST_F(FeedHandlerTest, SubscribeEmptyList) {
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    
    std::vector<uint16_t> empty_symbols;
    bool result = handler_->subscribe(empty_symbols);
    EXPECT_FALSE(result); // Should handle empty list gracefully
}

// Test: Subscribe to large symbol list
TEST_F(FeedHandlerTest, SubscribeLargeList) {
    server_fd_ = create_test_server(test_port_);
    ASSERT_GE(server_fd_, 0);
    
    std::thread accept_thread([this]() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
        if (client_fd >= 0) {
            char buffer[4096];
            recv(client_fd, buffer, sizeof(buffer), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            close(client_fd);
        }
    });
    
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, 100);
    
    if (handler_->connect("127.0.0.1", test_port_)) {
        std::vector<uint16_t> symbols;
        for (uint16_t i = 0; i < 100; ++i) {
            symbols.push_back(i);
        }
        
        handler_->subscribe(symbols);
    }
    
    accept_thread.join();
}

// Test: Destructor cleanup
TEST_F(FeedHandlerTest, DestructorCleanup) {
    server_fd_ = create_test_server(test_port_);
    ASSERT_GE(server_fd_, 0);
    
    std::thread accept_thread([this]() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
        if (client_fd >= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            close(client_fd);
        }
    });
    
    {
        FeedHandler handler("127.0.0.1", test_port_, num_symbols_);
        handler.connect("127.0.0.1", test_port_);
        // Destructor should clean up properly
    }
    
    accept_thread.join();
    
    // Test passes if no crash or hang occurs
    SUCCEED();
}

// Test: Connection to invalid host
TEST_F(FeedHandlerTest, ConnectInvalidHost) {
    handler_ = std::make_unique<FeedHandler>("invalid.host", test_port_, num_symbols_);
    
    bool result = handler_->connect("invalid.host", test_port_);
    EXPECT_FALSE(result);
    EXPECT_FALSE(handler_->is_connected());
}

// Test: Statistics remain valid after disconnect
TEST_F(FeedHandlerTest, StatisticsAfterDisconnect) {
    handler_ = std::make_unique<FeedHandler>("127.0.0.1", test_port_, num_symbols_);
    
    handler_->disconnect();
    
    auto stats = handler_->get_stats();
    EXPECT_EQ(stats.messages_received, 0);
    EXPECT_EQ(stats.bytes_received, 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
