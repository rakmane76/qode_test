#ifndef MARKET_DATA_SOCKET_H
#define MARKET_DATA_SOCKET_H

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>

namespace mdfh {

class MarketDataSocket {
public:
    MarketDataSocket();
    ~MarketDataSocket();
    
    // Connect to exchange feed
    bool connect(const std::string& host, uint16_t port, 
                 uint32_t timeout_ms = 5000);
    
    // Non-blocking receive into pre-allocated buffer
    ssize_t receive(void* buffer, size_t max_len);
    
    // Send subscription request
    bool send_subscription(const std::vector<uint16_t>& symbol_ids);
    
    // Connection management
    bool is_connected() const;
    void disconnect();
    
    // Socket options for low latency
    bool set_tcp_nodelay(bool enable);
    bool set_recv_buffer_size(size_t bytes);
    bool set_socket_priority(int priority);
    
    // Get file descriptor for epoll
    int get_fd() const { return sockfd_; }
    
private:
    int sockfd_;
    int epoll_fd_;
    std::atomic<bool> connected_;
    
    bool set_nonblocking(int fd);
    bool wait_for_connection(int fd, uint32_t timeout_ms);
};

} // namespace mdfh

#endif // MARKET_DATA_SOCKET_H
