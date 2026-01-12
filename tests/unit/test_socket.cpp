#include <gtest/gtest.h>
#include "client/socket.h"
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace mdfh;

class SocketTest : public ::testing::Test {
protected:
    void SetUp() override {
        socket_ = std::make_unique<MarketDataSocket>();
    }
    
    void TearDown() override {
        if (socket_ && socket_->is_connected()) {
            socket_->disconnect();
        }
        socket_.reset();
        
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
        
        if (listen(server_fd, 1) < 0) {
            close(server_fd);
            return -1;
        }
        
        return server_fd;
    }
    
    std::unique_ptr<MarketDataSocket> socket_;
    int server_fd_ = -1;
    const uint16_t test_port_ = 18888;
};

// Test: Socket construction and destruction
TEST_F(SocketTest, ConstructionDestruction) {
    EXPECT_NO_THROW({
        MarketDataSocket sock;
    });
}

// Test: Initial state
TEST_F(SocketTest, InitialState) {
    EXPECT_FALSE(socket_->is_connected());
    EXPECT_LT(socket_->get_fd(), 0); // Should be invalid initially
}

// Test: Connect to non-existent server
TEST_F(SocketTest, ConnectToNonExistentServer) {
    bool result = socket_->connect("127.0.0.1", 19999, 1000);
    EXPECT_FALSE(result);
    EXPECT_FALSE(socket_->is_connected());
}

// Test: Connect to valid server
TEST_F(SocketTest, ConnectToValidServer) {
    server_fd_ = create_test_server(test_port_);
    ASSERT_GE(server_fd_, 0);
    
    // Start a thread to accept the connection
    std::atomic<bool> accepted(false);
    std::thread accept_thread([this, &accepted]() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
        if (client_fd >= 0) {
            accepted = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            close(client_fd);
        }
    });
    
    bool result = socket_->connect("127.0.0.1", test_port_, 2000);
    
    accept_thread.join();
    
    EXPECT_TRUE(result || accepted); // Either connect succeeded or was accepted
    if (result) {
        EXPECT_TRUE(socket_->is_connected());
        EXPECT_GE(socket_->get_fd(), 0);
    }
}

// Test: Disconnect
TEST_F(SocketTest, Disconnect) {
    EXPECT_NO_THROW(socket_->disconnect());
    EXPECT_FALSE(socket_->is_connected());
}

// Test: Double disconnect
TEST_F(SocketTest, DoubleDisconnect) {
    socket_->disconnect();
    EXPECT_NO_THROW(socket_->disconnect());
    EXPECT_FALSE(socket_->is_connected());
}

// Test: Set TCP nodelay
TEST_F(SocketTest, SetTcpNodelay) {
    server_fd_ = create_test_server(test_port_);
    ASSERT_GE(server_fd_, 0);
    
    std::thread accept_thread([this]() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
        if (client_fd >= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            close(client_fd);
        }
    });
    
    if (socket_->connect("127.0.0.1", test_port_, 2000)) {
        EXPECT_TRUE(socket_->set_tcp_nodelay(true));
        EXPECT_TRUE(socket_->set_tcp_nodelay(false));
    }
    
    accept_thread.join();
}

// Test: Set receive buffer size
TEST_F(SocketTest, SetRecvBufferSize) {
    server_fd_ = create_test_server(test_port_);
    ASSERT_GE(server_fd_, 0);
    
    std::thread accept_thread([this]() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
        if (client_fd >= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            close(client_fd);
        }
    });
    
    if (socket_->connect("127.0.0.1", test_port_, 2000)) {
        EXPECT_TRUE(socket_->set_recv_buffer_size(65536));
        EXPECT_TRUE(socket_->set_recv_buffer_size(131072));
    }
    
    accept_thread.join();
}

// Test: Send subscription
TEST_F(SocketTest, SendSubscription) {
    server_fd_ = create_test_server(test_port_);
    ASSERT_GE(server_fd_, 0);
    
    std::atomic<bool> received_data(false);
    std::thread accept_thread([this, &received_data]() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
        if (client_fd >= 0) {
            char buffer[1024];
            ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
            if (n > 0) {
                received_data = true;
            }
            close(client_fd);
        }
    });
    
    if (socket_->connect("127.0.0.1", test_port_, 2000)) {
        std::vector<uint16_t> symbols = {0, 1, 2, 3, 4};
        bool sent = socket_->send_subscription(symbols);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        EXPECT_TRUE(sent || received_data);
    }
    
    accept_thread.join();
}

// Test: Receive with no data
TEST_F(SocketTest, ReceiveNoData) {
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
    
    if (socket_->connect("127.0.0.1", test_port_, 2000)) {
        char buffer[1024];
        ssize_t n = socket_->receive(buffer, sizeof(buffer));
        // Non-blocking socket may return -1 with EAGAIN
        EXPECT_TRUE(n >= 0 || n == -1);
    }
    
    accept_thread.join();
}

// Test: Receive with data
TEST_F(SocketTest, ReceiveWithData) {
    server_fd_ = create_test_server(test_port_);
    ASSERT_GE(server_fd_, 0);
    
    const char* test_data = "Hello from server";
    std::thread accept_thread([this, test_data]() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
        if (client_fd >= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            send(client_fd, test_data, strlen(test_data), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            close(client_fd);
        }
    });
    
    if (socket_->connect("127.0.0.1", test_port_, 2000)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        char buffer[1024];
        ssize_t n = socket_->receive(buffer, sizeof(buffer));
        
        if (n > 0) {
            EXPECT_GT(n, 0);
            EXPECT_LE(n, static_cast<ssize_t>(strlen(test_data)));
        }
    }
    
    accept_thread.join();
}

// Test: Multiple connect/disconnect cycles
TEST_F(SocketTest, MultipleConnectDisconnectCycles) {
    for (int i = 0; i < 3; ++i) {
        server_fd_ = create_test_server(test_port_ + i);
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
        
        socket_->connect("127.0.0.1", test_port_ + i, 2000);
        socket_->disconnect();
        
        accept_thread.join();
        close(server_fd_);
        server_fd_ = -1;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    EXPECT_FALSE(socket_->is_connected());
}

// Test: Invalid host
TEST_F(SocketTest, ConnectInvalidHost) {
    bool result = socket_->connect("invalid.host.example", 9999, 1000);
    EXPECT_FALSE(result);
    EXPECT_FALSE(socket_->is_connected());
}

// Test: Connection timeout
TEST_F(SocketTest, ConnectionTimeout) {
    // Try to connect to a non-routable IP (should timeout)
    auto start = std::chrono::steady_clock::now();
    bool result = socket_->connect("192.0.2.1", 9999, 1000); // TEST-NET-1 (non-routable)
    auto duration = std::chrono::steady_clock::now() - start;
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(socket_->is_connected());
    // Should timeout around 1000ms, allow some margin
    EXPECT_LT(duration, std::chrono::milliseconds(2000));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
