#include "client/socket.h"
#include "common/protocol.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>

namespace mdfh {

MarketDataSocket::MarketDataSocket()
    : sockfd_(-1), epoll_fd_(-1), connected_(false) {
    
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        throw std::runtime_error("Failed to create epoll");
    }
}

MarketDataSocket::~MarketDataSocket() {
    disconnect();
    
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
    }
}

bool MarketDataSocket::connect(const std::string& host, uint16_t port,
                                uint32_t timeout_ms) {
    // Create socket
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        return false;
    }
    
    // Set non-blocking
    if (!set_nonblocking(sockfd_)) {
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }
    
    // Connect
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }
    
    int ret = ::connect(sockfd_, (struct sockaddr*)&addr, sizeof(addr));
    
    if (ret < 0 && errno != EINPROGRESS) {
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }
    
    // Add to epoll with EPOLLOUT to wait for connection completion
    struct epoll_event ev{};
    ev.events = EPOLLOUT;
    ev.data.fd = sockfd_;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sockfd_, &ev) < 0) {
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }
    
    // Wait for connection with timeout
    if (!wait_for_connection(sockfd_, timeout_ms)) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, sockfd_, nullptr);
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }
    
    // Modify epoll to EPOLLIN for receiving data
    ev.events = EPOLLIN | EPOLLET; // Edge-triggered
    ev.data.fd = sockfd_;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, sockfd_, &ev) < 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, sockfd_, nullptr);
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }
    
    // Set socket options
    set_tcp_nodelay(true);
    set_recv_buffer_size(4 * 1024 * 1024); // 4MB
    
    connected_ = true;
    return true;
}

ssize_t MarketDataSocket::receive(void* buffer, size_t max_len) {
    if (!connected_ || sockfd_ < 0) {
        return -1;
    }
    
    ssize_t n = recv(sockfd_, buffer, max_len, 0);
    
    if (n == 0) {
        // Connection closed
        connected_ = false;
        return 0;
    }
    
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available right now
            return 0;
        }
        
        // Error
        connected_ = false;
        return -1;
    }
    
    return n;
}

bool MarketDataSocket::send_subscription(const std::vector<uint16_t>& symbol_ids) {
    if (!connected_ || sockfd_ < 0) {
        return false;
    }
    
    // Build subscription message
    std::vector<uint8_t> msg;
    msg.push_back(0xFF); // Subscribe command
    
    uint16_t count = static_cast<uint16_t>(symbol_ids.size());
    msg.push_back(count & 0xFF);
    msg.push_back((count >> 8) & 0xFF);
    
    for (uint16_t id : symbol_ids) {
        msg.push_back(id & 0xFF);
        msg.push_back((id >> 8) & 0xFF);
    }
    
    ssize_t sent = send(sockfd_, msg.data(), msg.size(), MSG_NOSIGNAL);
    return sent == static_cast<ssize_t>(msg.size());
}

bool MarketDataSocket::is_connected() const {
    return connected_;
}

void MarketDataSocket::disconnect() {
    if (sockfd_ >= 0) {
        if (epoll_fd_ >= 0) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, sockfd_, nullptr);
        }
        close(sockfd_);
        sockfd_ = -1;
    }
    connected_ = false;
}

bool MarketDataSocket::set_tcp_nodelay(bool enable) {
    if (sockfd_ < 0) return false;
    
    int flag = enable ? 1 : 0;
    return setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, 
                      &flag, sizeof(flag)) == 0;
}

bool MarketDataSocket::set_recv_buffer_size(size_t bytes) {
    if (sockfd_ < 0) return false;
    
    int size = static_cast<int>(bytes);
    return setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF, 
                      &size, sizeof(size)) == 0;
}

bool MarketDataSocket::set_socket_priority(int priority) {
    if (sockfd_ < 0) return false;
    
    return setsockopt(sockfd_, SOL_SOCKET, SO_PRIORITY, 
                      &priority, sizeof(priority)) == 0;
}

bool MarketDataSocket::set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool MarketDataSocket::wait_for_connection(int fd, uint32_t timeout_ms) {
    struct epoll_event events[1];
    
    int nfds = epoll_wait(epoll_fd_, events, 1, timeout_ms);
    
    if (nfds <= 0) {
        return false;
    }
    
    // Check if connection succeeded
    int error = 0;
    socklen_t len = sizeof(error);
    
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        return false;
    }
    
    return error == 0;
}

} // namespace mdfh
