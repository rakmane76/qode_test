# Network Implementation Details

## 1. Server-Side Design

### 1.1 epoll-Based Multi-Client Handling

**Why epoll?**
- Scalable to thousands of connections (O(1) per event)
- More efficient than select/poll (O(n) overhead)
- Available on Linux, our target platform

**Event Loop Structure:**
```cpp
int epoll_fd = epoll_create1(0);

// Add server socket
epoll_event ev;
ev.events = EPOLLIN;
ev.data.fd = server_fd;
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

while (running) {
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout_ms);
    
    for (int i = 0; i < nfds; ++i) {
        if (events[i].data.fd == server_fd) {
            // New connection
            accept_new_client();
        } else if (events[i].events & (EPOLLHUP | EPOLLERR)) {
            // Client error/disconnect
            disconnect_client(events[i].data.fd);
        }
    }
}
```

### 1.2 Accepting Connections

```cpp
void ExchangeSimulator::handle_new_connection() {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(server_fd_, 
                           (struct sockaddr*)&client_addr, 
                           &addr_len);
    
    if (client_fd < 0) return;
    
    // Set non-blocking mode
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    // Disable Nagle's algorithm
    int nodelay = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, 
               &nodelay, sizeof(nodelay));
    
    // Add to epoll (optional: we don't read from clients)
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);
    
    // Track client
    client_fds_.push_back(client_fd);
}
```

### 1.3 Broadcasting Messages

**Simple Iteration:**
```cpp
void ExchangeSimulator::broadcast_message(const void* data, size_t len) {
    for (int fd : client_fds_) {
        ssize_t sent = send(fd, data, len, MSG_NOSIGNAL);
        
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Send buffer full - slow client
                mark_slow_client(fd);
            } else if (errno == EPIPE || errno == ECONNRESET) {
                // Client disconnected
                schedule_disconnect(fd);
            }
        }
    }
}
```

**MSG_NOSIGNAL:**  
Prevents SIGPIPE when client disconnects. Instead, send() returns -1 with errno=EPIPE.

### 1.4 Slow Client Detection

**Problem:** Slow clients can't keep up with tick rate → TCP send buffer fills up.

**Detection:**
```cpp
void ClientManager::mark_slow_client(int fd) {
    clients_[fd].is_slow = true;
    clients_[fd].slow_count++;
    
    if (clients_[fd].slow_count > SLOW_THRESHOLD) {
        // Disconnect persistently slow clients
        disconnect_client(fd);
    }
}
```

**Strategies:**
1. **Best-effort**: Continue sending, drop messages if buffer full
2. **Disconnect**: Close connection to slow clients
3. **Throttle**: Reduce tick rate for slow clients
4. **Sample**: Send every Nth message to slow clients

Our implementation: Best-effort with eventual disconnect.

### 1.5 Connection State Management

```cpp
struct ClientInfo {
    int fd;
    time_t connected_at;
    uint64_t messages_sent;
    uint64_t bytes_sent;
    uint64_t send_errors;
    bool is_slow;
};

std::unordered_map<int, ClientInfo> clients_;
```

## 2. Client-Side Design

### 2.1 Non-Blocking Socket Creation

```cpp
bool MarketDataSocket::connect(const std::string& host, uint16_t port, 
                                uint32_t timeout_ms) {
    // Create socket
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    
    // Set non-blocking BEFORE connect
    int flags = fcntl(sockfd_, F_GETFL, 0);
    fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK);
    
    // Initiate connection
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    
    int ret = ::connect(sockfd_, (struct sockaddr*)&addr, sizeof(addr));
    
    if (ret < 0 && errno != EINPROGRESS) {
        // Immediate failure
        return false;
    }
    
    // Wait for connection with timeout
    return wait_for_connection(sockfd_, timeout_ms);
}
```

### 2.2 epoll: Edge-Triggered vs Level-Triggered

**Level-Triggered (LT):**
- Notification whenever data is available
- Can read partial data, will be notified again
- Easier to program, more forgiving

**Edge-Triggered (ET):**
- Notification only on state change (arrival of new data)
- Must read ALL available data before next notification
- More efficient, lower latency

**Our choice: Edge-Triggered**

```cpp
epoll_event ev;
ev.events = EPOLLIN | EPOLLET;  // Edge-triggered
ev.data.fd = sockfd_;
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd_, &ev);
```

**Receive loop for ET:**
```cpp
// When epoll notifies EPOLLIN:
while (true) {
    ssize_t n = recv(sockfd_, buffer, buffer_size, 0);
    
    if (n > 0) {
        process_data(buffer, n);
    } else if (n == 0) {
        // Connection closed
        break;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No more data available - done
            break;
        } else {
            // Real error
            handle_error();
            break;
        }
    }
}
```

**Why Edge-Triggered for Feed Handler:**
1. Lower latency (fewer epoll_wait calls)
2. Forces complete buffer draining (good for high throughput)
3. Natural batching of messages
4. Better for bursty traffic patterns

### 2.3 Socket Options for Low Latency

**TCP_NODELAY:**
```cpp
int nodelay = 1;
setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, 
           &nodelay, sizeof(nodelay));
```
- Disables Nagle's algorithm
- Sends small packets immediately
- Critical for low-latency applications

**SO_RCVBUF:**
```cpp
int buffer_size = 4 * 1024 * 1024;  // 4MB
setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF, 
           &buffer_size, sizeof(buffer_size));
```
- Increases kernel receive buffer
- Reduces dropped packets during bursts
- Allows client to fall behind temporarily

**SO_PRIORITY:**
```cpp
int priority = 6;
setsockopt(sockfd_, SOL_SOCKET, SO_PRIORITY, 
           &priority, sizeof(priority));
```
- Higher priority in OS network stack
- May require CAP_NET_ADMIN capability

### 2.4 Connection Drop Detection

**Three scenarios:**
1. **Graceful close (FIN)**: recv() returns 0
2. **Reset (RST)**: recv() returns -1, errno=ECONNRESET
3. **Silent drop (network partition)**: No immediate indication

**Detecting silent drops:**

**Option 1: TCP Keepalive**
```cpp
int keepalive = 1;
int keepidle = 60;   // Start after 60s idle
int keepintvl = 10;  // Probe every 10s
int keepcnt = 3;     // 3 failed probes = dead

setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
setsockopt(sockfd_, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
setsockopt(sockfd_, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
setsockopt(sockfd_, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
```

**Option 2: Application-level heartbeat**
```cpp
// Server sends heartbeat every 30s
// Client expects heartbeat within 45s
if (time_since_last_message() > 45s) {
    // Connection presumed dead
    disconnect_and_reconnect();
}
```

Our implementation: Heartbeat messages in protocol (MessageType::HEARTBEAT).

## 3. TCP Stream Handling

### 3.1 Message Boundary Problem

TCP is a **stream protocol** - no message boundaries:

```
Sender: [MSG1][MSG2][MSG3]
         ↓ TCP Stream
Receiver: [MSG1][MS  ← recv() returns here
           G2][MSG3] ← next recv() gets rest
```

### 3.2 Buffering Strategy

**Ring buffer approach:**
```cpp
class BinaryParser {
    std::vector<uint8_t> buffer_;  // Fixed-size buffer
    size_t buffer_pos_;            // Current fill level
    
    void parse(const void* data, size_t len) {
        // Copy new data to buffer
        memcpy(&buffer_[buffer_pos_], data, len);
        buffer_pos_ += len;
        
        // Try to parse complete messages
        while (try_parse_message()) {
            // Continue until no complete message
        }
    }
    
    bool try_parse_message() {
        if (buffer_pos_ < sizeof(MessageHeader))
            return false;
        
        MessageHeader* hdr = (MessageHeader*)buffer_.data();
        size_t msg_size = get_message_size(hdr->msg_type);
        
        if (buffer_pos_ < msg_size)
            return false;  // Incomplete message
        
        // Process complete message
        process_message(buffer_.data(), msg_size);
        
        // Remove from buffer
        memmove(buffer_.data(), &buffer_[msg_size], 
                buffer_pos_ - msg_size);
        buffer_pos_ -= msg_size;
        
        return true;
    }
};
```

### 3.3 Buffer Sizing

**Trade-offs:**
- Small buffer: Less memory, more frequent allocations
- Large buffer: More memory, handles burst better

**Our choice: 64KB**
- Can hold ~1000 quote messages
- Can hold ~2000 trade messages
- Handles typical burst sizes
- Modest memory footprint

### 3.4 Preventing Buffer Overflow

**Malformed message with huge length:**
```cpp
if (msg_size > MAX_MESSAGE_SIZE || msg_size == 0) {
    // Invalid size - skip one byte and resync
    memmove(buffer_.data(), &buffer_[1], buffer_pos_ - 1);
    buffer_pos_--;
    malformed_messages_++;
    return false;
}
```

## 4. Connection Management

### 4.1 Reconnection Logic

**Exponential Backoff:**
```cpp
bool FeedHandler::reconnect() {
    int backoff_ms = INITIAL_BACKOFF_MS;  // 100ms
    
    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        if (socket_->connect(host_, port_)) {
            return true;
        }
        
        std::this_thread::sleep_for(
            std::chrono::milliseconds(backoff_ms));
        
        backoff_ms = std::min(backoff_ms * 2, MAX_BACKOFF_MS);
    }
    
    return false;  // Give up
}
```

**Backoff sequence:**
- Attempt 1: 100ms
- Attempt 2: 200ms
- Attempt 3: 400ms
- Attempt 4: 800ms
- Attempt 5: 1600ms
- Attempt 6: 3200ms
- Attempt 7-10: 30000ms (capped)

### 4.2 Reconnection Thread Strategy

**Option 1: Same thread (our choice)**
```cpp
void receiver_loop() {
    while (running_) {
        if (!connected_) {
            reconnect();
            continue;
        }
        
        // Normal receive logic
    }
}
```

**Pros:**
- Simple, no thread coordination
- Automatic pause during reconnect

**Cons:**
- Blocks receiver thread during reconnect

**Option 2: Separate reconnection thread**
```cpp
void receiver_loop() {
    while (running_) {
        if (!connected_) {
            sleep_briefly();
            continue;
        }
        // Receive logic
    }
}

void reconnector_loop() {
    while (running_) {
        if (!connected_) {
            reconnect();
        }
        sleep(1s);
    }
}
```

**Pros:**
- Receiver thread never blocks
- Can handle other tasks during reconnect

**Cons:**
- More complex synchronization
- Need to coordinate state

For our use case, same-thread reconnection is simpler and sufficient.

### 4.3 Subscription Management

After reconnection, must resubscribe:

```cpp
bool FeedHandler::reconnect() {
    if (!socket_->connect(host_, port_)) {
        return false;
    }
    
    // Resubscribe to symbols
    if (!subscribed_symbols_.empty()) {
        socket_->send_subscription(subscribed_symbols_);
    }
    
    return true;
}
```

## 5. Error Handling

### 5.1 Common Network Errors

| Error | Meaning | Handling |
|-------|---------|----------|
| EPIPE | Write to closed socket | Disconnect, reconnect |
| ECONNRESET | Connection reset by peer | Disconnect, reconnect |
| ECONNREFUSED | Server not accepting | Reconnect with backoff |
| ETIMEDOUT | Connection timeout | Reconnect with backoff |
| EAGAIN/EWOULDBLOCK | No data available (non-blocking) | Continue, normal |
| EINTR | Interrupted by signal | Retry operation |
| EMSGSIZE | Message too large | Protocol error |
| ENOTCONN | Socket not connected | Reconnect |

### 5.2 Error Recovery

```cpp
ssize_t n = recv(sockfd_, buffer, size, 0);

if (n < 0) {
    switch (errno) {
        case EAGAIN:
        case EWOULDBLOCK:
            // Normal for non-blocking socket
            return 0;
        
        case EINTR:
            // Interrupted by signal, retry
            return receive(buffer, max_len);
        
        case ECONNRESET:
        case ENOTCONN:
        case ETIMEDOUT:
            // Connection problem
            connected_ = false;
            return -1;
        
        default:
            // Unknown error
            log_error("recv failed: ", strerror(errno));
            connected_ = false;
            return -1;
    }
}
```

### 5.3 Graceful Shutdown

```cpp
void MarketDataSocket::disconnect() {
    if (sockfd_ >= 0) {
        // Shutdown write side (send FIN)
        shutdown(sockfd_, SHUT_WR);
        
        // Brief delay for FIN to be sent
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Close socket
        close(sockfd_);
        sockfd_ = -1;
    }
    
    connected_ = false;
}
```

## 6. Performance Considerations

### 6.1 System Call Overhead

**Minimizing recv() calls:**
- Use large buffers (64KB)
- Edge-triggered epoll (read until EAGAIN)
- Batch processing of messages

**Typical syscall latency:**
- recv(): ~1-2 μs
- epoll_wait(): ~1-2 μs
- send(): ~1-2 μs

### 6.2 Zero-Copy Techniques

**Avoiding copies:**
```cpp
// Bad: Extra copy
std::vector<uint8_t> temp_buffer(size);
recv(sockfd_, temp_buffer.data(), size, 0);
parser_.parse(temp_buffer.data(), size);

// Good: Direct parsing
uint8_t buffer[65536];
ssize_t n = recv(sockfd_, buffer, sizeof(buffer), 0);
parser_.parse(buffer, n);  // Parses in-place
```

### 6.3 CPU Affinity

Pin threads to specific CPUs:
```cpp
cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(2, &cpuset);

pthread_setaffinity_np(pthread_self(), 
                       sizeof(cpuset), &cpuset);
```

Reduces cache misses and context switches.

