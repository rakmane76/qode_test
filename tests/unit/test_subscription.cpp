#include <gtest/gtest.h>
#include "server/exchange_simulator.h"
#include "common/protocol.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace mdfh {

class SubscriptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "test_subscription_temp";
        fs::create_directories(test_dir_);
        config_dir_ = test_dir_ + "/config";
        fs::create_directories(config_dir_);
    }

    void TearDown() override {
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    std::string create_test_config(uint16_t port, size_t num_symbols) {
        // Create symbol file
        std::string symbol_file = config_dir_ + "/symbols.csv";
        std::ofstream file(symbol_file);
        file << "name,initial_price,volatility,drift\n";
        for (size_t i = 0; i < num_symbols; ++i) {
            file << "SYM" << i << "," << (1000.0 + i * 10.0) << ",0.02,0.01\n";
        }
        file.close();
        
        // Create config file
        std::string config_file = config_dir_ + "/server.conf";
        std::ofstream cfg(config_file);
        cfg << "server.port=" << port << "\n";
        cfg << "market.num_symbols=" << num_symbols << "\n";
        cfg << "market.tick_rate=0\n";
        cfg << "market.symbols_file=" << fs::absolute(symbol_file).string() << "\n";
        cfg << "fault_injection.enabled=false\n";
        cfg.close();
        
        return config_file;
    }

    int create_client_socket(uint16_t port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return -1;
        
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            return -1;
        }
        
        return sock;
    }

    bool send_subscription(int sock, const std::vector<uint16_t>& symbol_ids) {
        std::vector<uint8_t> msg;
        msg.push_back(0xFF); // Subscribe command
        
        uint16_t count = static_cast<uint16_t>(symbol_ids.size());
        msg.push_back(count & 0xFF);
        msg.push_back((count >> 8) & 0xFF);
        
        for (uint16_t id : symbol_ids) {
            msg.push_back(id & 0xFF);
            msg.push_back((id >> 8) & 0xFF);
        }
        
        ssize_t sent = send(sock, msg.data(), msg.size(), 0);
        return sent == static_cast<ssize_t>(msg.size());
    }

    std::string test_dir_;
    std::string config_dir_;
};

// Test Case 1: Client subscribes to specific symbols
TEST_F(SubscriptionTest, ClientSubscribesToSymbols) {
    std::string config = create_test_config(12400, 10);
    ExchangeSimulator sim(12400, 10, config);
    sim.start();
    
    std::thread event_thread([&sim]() {
        sim.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create client and subscribe to symbols 0, 2, 5
    int client = create_client_socket(12400);
    ASSERT_GE(client, 0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    std::vector<uint16_t> symbols = {0, 2, 5};
    ASSERT_TRUE(send_subscription(client, symbols));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Get server-side client FD
    ASSERT_EQ(sim.get_num_connected_clients(), 1);
    int server_client_fd = sim.get_client_fds()[0];
    
    // Verify subscriptions registered
    EXPECT_EQ(sim.get_client_subscription_count(server_client_fd), 3);
    EXPECT_TRUE(sim.is_client_subscribed(server_client_fd, 0));
    EXPECT_TRUE(sim.is_client_subscribed(server_client_fd, 2));
    EXPECT_TRUE(sim.is_client_subscribed(server_client_fd, 5));
    EXPECT_FALSE(sim.is_client_subscribed(server_client_fd, 1));
    EXPECT_FALSE(sim.is_client_subscribed(server_client_fd, 3));
    
    close(client);
    sim.stop();
    
    if (event_thread.joinable()) {
        event_thread.join();
    }
}

// Test Case 2: Client receives only subscribed symbols
TEST_F(SubscriptionTest, ClientReceivesOnlySubscribedSymbols) {
    std::string config = create_test_config(12401, 5);
    ExchangeSimulator sim(12401, 5, config);
    sim.start();
    
    std::thread event_thread([&sim]() {
        sim.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create client and subscribe to symbol 0 only
    int client = create_client_socket(12401);
    ASSERT_GE(client, 0);
    
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    std::vector<uint16_t> symbols = {0};
    ASSERT_TRUE(send_subscription(client, symbols));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Generate tick for symbol 0 (subscribed)
    sim.generate_tick(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Should receive message
    uint8_t buffer[1024];
    ssize_t bytes = recv(client, buffer, sizeof(buffer), 0);
    EXPECT_GT(bytes, 0) << "Should receive message for subscribed symbol 0";
    
    if (bytes > 0) {
        MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
        EXPECT_EQ(header->symbol_id, 0);
    }
    
    // Generate tick for symbol 1 (NOT subscribed)
    sim.generate_tick(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Should NOT receive message
    bytes = recv(client, buffer, sizeof(buffer), MSG_DONTWAIT);
    EXPECT_LE(bytes, 0) << "Should NOT receive message for unsubscribed symbol 1";
    
    close(client);
    sim.stop();
    
    if (event_thread.joinable()) {
        event_thread.join();
    }
}

// Test Case 3: Multiple clients with different subscriptions
TEST_F(SubscriptionTest, MultipleClientsDifferentSubscriptions) {
    std::string config = create_test_config(12402, 5);
    ExchangeSimulator sim(12402, 5, config);
    sim.start();
    
    std::thread event_thread([&sim]() {
        sim.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Client 1 subscribes to symbols 0, 1
    int client1 = create_client_socket(12402);
    ASSERT_GE(client1, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(send_subscription(client1, {0, 1}));
    
    // Client 2 subscribes to symbols 1, 2
    int client2 = create_client_socket(12402);
    ASSERT_GE(client2, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(send_subscription(client2, {1, 2}));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Get server-side client FDs
    ASSERT_EQ(sim.get_num_connected_clients(), 2);
    const auto& server_fds = sim.get_client_fds();
    int server_client1_fd = server_fds[0];
    int server_client2_fd = server_fds[1];
    
    // Verify subscriptions
    EXPECT_TRUE(sim.is_client_subscribed(server_client1_fd, 0));
    EXPECT_TRUE(sim.is_client_subscribed(server_client1_fd, 1));
    EXPECT_FALSE(sim.is_client_subscribed(server_client1_fd, 2));
    
    EXPECT_FALSE(sim.is_client_subscribed(server_client2_fd, 0));
    EXPECT_TRUE(sim.is_client_subscribed(server_client2_fd, 1));
    EXPECT_TRUE(sim.is_client_subscribed(server_client2_fd, 2));
    
    close(client1);
    close(client2);
    sim.stop();
    
    if (event_thread.joinable()) {
        event_thread.join();
    }
}

// Test Case 4: Subscription update (re-subscribe)
TEST_F(SubscriptionTest, SubscriptionUpdate) {
    std::string config = create_test_config(12403, 5);
    ExchangeSimulator sim(12403, 5, config);
    sim.start();
    
    std::thread event_thread([&sim]() {
        sim.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    int client = create_client_socket(12403);
    ASSERT_GE(client, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Initial subscription: symbols 0, 1
    ASSERT_TRUE(send_subscription(client, {0, 1}));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Get server-side client FD
    ASSERT_EQ(sim.get_num_connected_clients(), 1);
    int server_client_fd = sim.get_client_fds()[0];
    
    EXPECT_EQ(sim.get_client_subscription_count(server_client_fd), 2);
    EXPECT_TRUE(sim.is_client_subscribed(server_client_fd, 0));
    EXPECT_TRUE(sim.is_client_subscribed(server_client_fd, 1));
    
    // Update subscription: symbols 2, 3, 4
    ASSERT_TRUE(send_subscription(client, {2, 3, 4}));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(sim.get_client_subscription_count(server_client_fd), 3);
    EXPECT_FALSE(sim.is_client_subscribed(server_client_fd, 0));
    EXPECT_FALSE(sim.is_client_subscribed(server_client_fd, 1));
    EXPECT_TRUE(sim.is_client_subscribed(server_client_fd, 2));
    EXPECT_TRUE(sim.is_client_subscribed(server_client_fd, 3));
    EXPECT_TRUE(sim.is_client_subscribed(server_client_fd, 4));
    
    close(client);
    sim.stop();
    
    if (event_thread.joinable()) {
        event_thread.join();
    }
}

// Test Case 5: Invalid subscription message handling
TEST_F(SubscriptionTest, InvalidSubscriptionMessage) {
    std::string config = create_test_config(12404, 5);
    ExchangeSimulator sim(12404, 5, config);
    sim.start();
    
    std::thread event_thread([&sim]() {
        sim.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    int client = create_client_socket(12404);
    ASSERT_GE(client, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Get server-side client FD
    ASSERT_EQ(sim.get_num_connected_clients(), 1);
    int server_client_fd = sim.get_client_fds()[0];
    
    // Send invalid message (wrong command)
    uint8_t invalid_msg[] = {0xFE, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00};
    send(client, invalid_msg, sizeof(invalid_msg), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Should have 0 subscriptions
    EXPECT_EQ(sim.get_client_subscription_count(server_client_fd), 0);
    
    // Send valid subscription
    ASSERT_TRUE(send_subscription(client, {0, 1}));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(sim.get_client_subscription_count(server_client_fd), 2);
    
    close(client);
    sim.stop();
    
    if (event_thread.joinable()) {
        event_thread.join();
    }
}

// Test Case 6: Client disconnect removes subscriptions
TEST_F(SubscriptionTest, ClientDisconnectRemovesSubscriptions) {
    std::string config = create_test_config(12405, 5);
    ExchangeSimulator sim(12405, 5, config);
    sim.start();
    
    std::thread event_thread([&sim]() {
        sim.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    int client = create_client_socket(12405);
    ASSERT_GE(client, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    ASSERT_TRUE(send_subscription(client, {0, 1, 2}));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Get server-side client FD
    ASSERT_EQ(sim.get_num_connected_clients(), 1);
    int server_client_fd = sim.get_client_fds()[0];
    
    EXPECT_EQ(sim.get_client_subscription_count(server_client_fd), 3);
    
    // Disconnect client
    close(client);
    
    // Trigger disconnect detection
    sim.generate_tick(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Subscriptions should be removed (count returns 0 for non-existent client)
    EXPECT_EQ(sim.get_client_subscription_count(server_client_fd), 0);
    
    sim.stop();
    
    if (event_thread.joinable()) {
        event_thread.join();
    }
}

// Test Case 7: Empty subscription (unsubscribe from all)
TEST_F(SubscriptionTest, EmptySubscription) {
    std::string config = create_test_config(12406, 5);
    ExchangeSimulator sim(12406, 5, config);
    sim.start();
    
    std::thread event_thread([&sim]() {
        sim.run();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    int client = create_client_socket(12406);
    ASSERT_GE(client, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Subscribe to some symbols
    ASSERT_TRUE(send_subscription(client, {0, 1, 2}));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Get server-side client FD
    ASSERT_EQ(sim.get_num_connected_clients(), 1);
    int server_client_fd = sim.get_client_fds()[0];
    
    EXPECT_EQ(sim.get_client_subscription_count(server_client_fd), 3);
    
    // Send empty subscription
    ASSERT_TRUE(send_subscription(client, {}));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Should have 0 subscriptions
    EXPECT_EQ(sim.get_client_subscription_count(server_client_fd), 0);
    
    close(client);
    sim.stop();
    
    if (event_thread.joinable()) {
        event_thread.join();
    }
}

} // namespace mdfh

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
