# Critical Thinking Questions - Answered

## 1. Exchange Simulator Questions

### Q1.1: How do you efficiently broadcast to multiple clients without blocking?

**Answer:**

Three approaches, in order of sophistication:

**Approach 1: Simple Iteration (Current Implementation)**
```cpp
void broadcast_message(const void* data, size_t len) {
    for (int fd : client_fds_) {
        send(fd, data, len, MSG_NOSIGNAL);
    }
}
```

**Pros:** Simple, works well for < 100 clients
**Cons:** Blocks if any client's send buffer is full

**Approach 2: Non-Blocking Sends with epoll**
```cpp
void broadcast_message(const void* data, size_t len) {
    for (int fd : client_fds_) {
        ssize_t sent = send(fd, data, len, MSG_DONTWAIT | MSG_NOSIGNAL);
        
        if (sent < 0 && errno == EAGAIN) {
            // Add to pending queue for this client
            pending_messages_[fd].push(message);
            
            // Register for EPOLLOUT to know when buffer is ready
            modify_epoll_for_write(fd);
        }
    }
}
```

**Pros:** Never blocks, handles slow clients gracefully
**Cons:** More complex, requires message queuing

**Approach 3: Dedicated Broadcast Thread**
```cpp
// Producer thread (tick generator)
void generate_tick() {
    Message msg = create_message();
    broadcast_queue_.push(msg);
}

// Consumer thread (broadcaster)
void broadcast_loop() {
    while (running_) {
        Message msg = broadcast_queue_.pop();
        for (int fd : client_fds_) {
            send(fd, &msg, sizeof(msg), MSG_NOSIGNAL);
        }
    }
}
```

**Pros:** Decouples tick generation from broadcasting
**Cons:** Additional thread overhead, queue synchronization

**Recommended:** Approach 2 for production (handles slow clients without blocking).

---

### Q1.2: What happens when a client's TCP send buffer fills up?

**Answer:**

**Sequence of events:**

1. **send() returns -1, errno = EAGAIN/EWOULDBLOCK**
   ```cpp
   ssize_t sent = send(fd, data, len, MSG_NOSIGNAL);
   if (sent < 0 && errno == EAGAIN) {
       // Send buffer full
   }
   ```

2. **Possible causes:**
   - Client not reading data fast enough
   - Network congestion
   - Client process suspended/killed

3. **Handling strategies:**

   **Strategy A: Drop message (lossy)**
   ```cpp
   if (sent < 0 && errno == EAGAIN) {
       dropped_messages_++;
       continue; // Skip this client
   }
   ```
   
   **Strategy B: Queue for later (lossless)**
   ```cpp
   if (sent < 0 && errno == EAGAIN) {
       pending_queue_[fd].push(message);
       epoll_mod(fd, EPOLLIN | EPOLLOUT); // Watch for writable
   }
   ```
   
   **Strategy C: Disconnect slow clients**
   ```cpp
   if (++slow_count_[fd] > THRESHOLD) {
       disconnect_client(fd);
   }
   ```

4. **Our implementation:**
   - Mark client as slow (tracking)
   - Continue best-effort delivery
   - Disconnect after persistent failures
   - Prevents one slow client from affecting others

**Buffer sizing:**
- Default TCP send buffer: ~16-64 KB
- Can increase with SO_SNDBUF: up to ~4 MB
- At 100K msgs/sec, 48-byte messages → ~5 MB/sec
- Buffer fills in < 1 second if client stops reading

---

### Q1.3: How do you ensure fair distribution when some clients are slower?

**Answer:**

**Problem:** Slow clients should not impact fast clients.

**Solutions:**

**1. Per-Client Queuing**
```cpp
struct ClientState {
    int fd;
    std::deque<Message> pending;
    size_t max_queue_size = 10000;
};

void broadcast(Message msg) {
    for (auto& client : clients_) {
        if (client.pending.size() < client.max_queue_size) {
            client.pending.push_back(msg);
        } else {
            // Queue full, drop or disconnect
            mark_slow_client(client.fd);
        }
    }
}
```

**2. Priority Levels**
```cpp
void broadcast(Message msg) {
    // Fast clients (empty queue) get immediate send
    for (auto& client : fast_clients_) {
        send_immediate(client.fd, msg);
    }
    
    // Slow clients (queued) get added to queue
    for (auto& client : slow_clients_) {
        enqueue(client.fd, msg);
    }
}
```

**3. Rate Limiting per Client**
```cpp
struct ClientState {
    RateLimiter limiter(100000); // 100K msgs/sec max
};

void broadcast(Message msg) {
    for (auto& client : clients_) {
        if (client.limiter.allow()) {
            send(client.fd, msg);
        } else {
            dropped_for_rate_limit_++;
        }
    }
}
```

**4. Sampling for Slow Clients**
```cpp
void broadcast(Message msg) {
    for (auto& client : clients_) {
        if (client.is_slow) {
            if (msg.seq_num % 10 == 0) { // Send every 10th message
                send(client.fd, msg);
            }
        } else {
            send(client.fd, msg);
        }
    }
}
```

**Recommended approach:**
- Combination of per-client queuing (bounded)
- Disconnect if queue exceeds threshold
- Protects fast clients from slow clients

---

### Q1.4: How would you handle 1000+ concurrent client connections?

**Answer:**

**Challenges at scale:**

1. **O(N) broadcast iteration**
   - 1000 clients × 2 μs per send() = 2 ms per broadcast
   - At 100K msgs/sec → 200 seconds of CPU time per second (impossible!)

2. **epoll scalability**
   - epoll scales well, but adding/removing 1000 FDs has overhead

3. **Memory usage**
   - 1000 clients × 200 KB buffers = 200 MB (manageable)

**Solutions:**

**1. Use UDP Multicast**
```cpp
// Server sends to multicast group
int sock = socket(AF_INET, SOCK_DGRAM, 0);
struct sockaddr_in addr;
addr.sin_addr.s_addr = inet_addr("239.255.0.1"); // Multicast IP
sendto(sock, data, len, 0, (struct sockaddr*)&addr, sizeof(addr));

// Clients join multicast group
struct ip_mreq mreq;
mreq.imr_multiaddr.s_addr = inet_addr("239.255.0.1");
setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
```

**Pros:** O(1) broadcast regardless of client count
**Cons:** UDP (unreliable), requires multicast-capable network

**2. Thread Pool for Broadcasting**
```cpp
// Partition clients across threads
Thread 1: clients 0-249
Thread 2: clients 250-499
Thread 3: clients 500-749
Thread 4: clients 750-999

Each thread broadcasts independently in parallel
```

**3. epoll with EPOLLOUT for Flow Control**
```cpp
// Only send to clients whose buffers are ready
for each fd with EPOLLOUT event:
    send_next_message(fd);
```

**4. Shared Memory + Client Polling**
```cpp
// Server writes to shared memory ring buffer
SharedRingBuffer* buffer = create_shm("/market_data");
buffer->write(message);

// Clients poll shared memory (no network)
while (running) {
    Message msg = buffer->read();
    process(msg);
}
```

**Recommended for 1000+ clients:**
- UDP multicast for best performance
- Or partitioned TCP with thread pool
- Add load balancing across multiple server instances

---

## 2. TCP Client Questions

### Q2.1: Why use epoll edge-triggered instead of level-triggered for feed handler?

**Answer:**

**Level-Triggered (LT):**
```
Data arrives → EPOLLIN event
recv() reads partial data
Data still available → EPOLLIN event again (next epoll_wait)
```

**Edge-Triggered (ET):**
```
Data arrives → EPOLLIN event
recv() reads partial data
Data still available → NO event (must drain buffer completely)
New data arrives → EPOLLIN event
```

**Comparison:**

| Aspect | Level-Triggered | Edge-Triggered |
|--------|----------------|----------------|
| Events generated | Many (per epoll_wait) | Few (on state change) |
| Buffer draining | Optional | Mandatory |
| Complexity | Easier | Harder |
| Performance | Good | Better |
| Latency | Higher | Lower |

**Why ET for feed handler:**

1. **Lower latency:** Fewer epoll_wait() calls
   ```
   LT: epoll_wait → recv → epoll_wait → recv → ...
   ET: epoll_wait → recv → recv → recv → ... (batch)
   ```

2. **Higher throughput:** Natural batching
   - When EPOLLIN fires, drain entire buffer
   - Process multiple messages per syscall

3. **Predictable behavior:** No spurious events
   - Only notified when NEW data arrives
   - Easier to reason about state

4. **Better for high-frequency data:**
   - Feed handler receives bursts of messages
   - ET processes entire burst efficiently
   - LT would wake up multiple times per burst

**Code pattern for ET:**
```cpp
while (true) {
    ssize_t n = recv(fd, buffer, size, 0);
    if (n > 0) {
        process(buffer, n);
    } else if (n == 0) {
        break; // Connection closed
    } else {
        if (errno == EAGAIN) {
            break; // Buffer drained, done
        } else {
            error(); // Real error
        }
    }
}
```

---

### Q2.2: How do you handle the case where recv() returns EAGAIN/EWOULDBLOCK?

**Answer:**

**EAGAIN/EWOULDBLOCK means:** "No data available right now on non-blocking socket"

**Handling depends on context:**

**1. Edge-Triggered epoll (our case):**
```cpp
void on_epollin_event() {
    while (true) {
        ssize_t n = recv(sockfd_, buffer, sizeof(buffer), 0);
        
        if (n > 0) {
            // Data received, process it
            parser_->parse(buffer, n);
        } else if (n == 0) {
            // Connection closed gracefully
            connected_ = false;
            break;
        } else { // n < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Normal: No more data available, buffer drained
                break; // Exit loop, wait for next epoll event
            } else {
                // Real error (ECONNRESET, etc.)
                handle_error();
                break;
            }
        }
    }
}
```

**Key point:** EAGAIN is **not an error** in non-blocking I/O, it's a signal to stop reading.

**2. Level-Triggered epoll:**
```cpp
void on_epollin_event() {
    ssize_t n = recv(sockfd_, buffer, sizeof(buffer), 0);
    
    if (n > 0) {
        parser_->parse(buffer, n);
    } else if (n == 0) {
        connected_ = false;
    } else if (errno == EAGAIN) {
        // No data available, but LT will notify again if data arrives
        return; // Just return, will be called again
    } else {
        handle_error();
    }
}
```

**3. Blocking socket (not recommended for latency):**
```cpp
// recv() blocks until data arrives, never returns EAGAIN
ssize_t n = recv(sockfd_, buffer, sizeof(buffer), 0);
```

**When can EAGAIN occur?**
- Non-blocking socket has no data in kernel buffer
- Previous recv() drained all available data
- Data hasn't arrived yet since last recv()

**What NOT to do:**
```cpp
// WRONG: Busy-wait
while (recv(...) == -1 && errno == EAGAIN) {
    // Burns CPU!
}

// WRONG: Retry immediately
if (errno == EAGAIN) {
    return recv(...); // Will likely return EAGAIN again
}
```

**Correct approach:** Return from receive function, let epoll notify when new data arrives.

---

### Q2.3: What happens if the kernel receive buffer fills up?

**Answer:**

**Sequence of events:**

1. **Kernel buffer fills up:**
   ```
   Sender sends data → TCP → Kernel recv buffer → Application recv()
                              ↓
                         Buffer full!
   ```

2. **TCP flow control kicks in:**
   ```
   Receiver sends TCP ACK with window size = 0 (zero window)
   Sender stops sending data (blocks or buffers)
   ```

3. **Consequences:**
   
   **At receiver:**
   - No data loss (TCP guarantees delivery)
   - recv() still works (reads from buffer)
   - Application must drain buffer to make space
   
   **At sender:**
   - send() blocks (blocking socket)
   - send() returns EAGAIN (non-blocking socket)
   - Application must retry send later

4. **Recovery:**
   ```
   Application calls recv() → buffer space freed
   Kernel sends TCP ACK with new window size > 0
   Sender resumes sending data
   ```

**Why buffer fills up:**
- Application not reading data fast enough
- Processing latency spike
- Application suspended/deadlocked

**Prevention strategies:**

**1. Increase buffer size:**
```cpp
int buffer_size = 4 * 1024 * 1024; // 4 MB
setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF, 
           &buffer_size, sizeof(buffer_size));
```

**2. Read data faster:**
```cpp
// Edge-triggered epoll: drain buffer completely
while (recv(...) > 0) {
    // Process each message
}
```

**3. Monitor buffer occupancy:**
```cpp
int bytes_available;
ioctl(sockfd_, FIONREAD, &bytes_available);

if (bytes_available > THRESHOLD) {
    log_warning("Receive buffer filling up");
}
```

**4. Backpressure to application:**
```cpp
if (parser_queue.size() > MAX_QUEUE_SIZE) {
    // Stop reading from socket temporarily
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockfd_, nullptr);
    
    // Resume when queue drains
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd_, &ev);
}
```

**For feed handler:**
- Large buffer (4MB) handles bursts
- Edge-triggered epoll ensures fast draining
- If buffer still fills → application is too slow, need optimization

---

### Q2.4: How do you detect a silent connection drop (no FIN/RST)?

**Answer:**

**Silent drop scenarios:**
- Network cable unplugged
- Router/switch failure
- Firewall drops packets
- Remote host crashes without sending FIN

**Problem:** TCP doesn't detect immediately, can take hours by default.

**Solutions:**

**1. TCP Keepalive (Kernel-Level)**
```cpp
int keepalive = 1;
setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, 
           &keepalive, sizeof(keepalive));

// Start probes after 60s of idle
int keepidle = 60;
setsockopt(sockfd_, IPPROTO_TCP, TCP_KEEPIDLE, 
           &keepidle, sizeof(keepidle));

// Send probe every 10s
int keepintvl = 10;
setsockopt(sockfd_, IPPROTO_TCP, TCP_KEEPINTVL, 
           &keepintvl, sizeof(keepintvl));

// Give up after 3 failed probes
int keepcnt = 3;
setsockopt(sockfd_, IPPROTO_TCP, TCP_KEEPCNT, 
           &keepcnt, sizeof(keepcnt));
```

**Detection time:** 60s + (10s × 3) = 90 seconds

**2. Application-Level Heartbeat (Faster)**
```cpp
// Server sends heartbeat every 5 seconds
void server_heartbeat_loop() {
    while (running) {
        HeartbeatMessage hb;
        broadcast_message(&hb, sizeof(hb));
        sleep(5s);
    }
}

// Client expects heartbeat within 15 seconds
void client_check_heartbeat() {
    auto now = steady_clock::now();
    auto elapsed = now - last_heartbeat_time_;
    
    if (elapsed > 15s) {
        // No heartbeat received, connection presumed dead
        disconnect_and_reconnect();
    }
}
```

**Detection time:** 15 seconds

**3. Timeout on recv() (Application Timeout)**
```cpp
auto last_recv = steady_clock::now();

while (running) {
    epoll_wait(epoll_fd, events, MAX_EVENTS, 1000); // 1s timeout
    
    auto now = steady_clock::now();
    if (now - last_recv > 30s) {
        // No data for 30s, assume dead
        reconnect();
    }
    
    if (events_received > 0) {
        last_recv = now;
    }
}
```

**4. TCP_USER_TIMEOUT (Linux 2.6.37+)**
```cpp
unsigned int timeout_ms = 30000; // 30 seconds
setsockopt(sockfd_, IPPROTO_TCP, TCP_USER_TIMEOUT, 
           &timeout_ms, sizeof(timeout_ms));
```

Kernel will abort connection if no ACK received within timeout.

**Recommended for feed handler:**
- Application-level heartbeat (fastest, 15s)
- TCP keepalive as backup (90s)
- Combination provides defense in depth

**Our implementation:**
- HeartbeatMessage in protocol
- Server sends every 30s
- Client expects within 45s

---

### Q2.5: Should reconnection logic be in the same thread or separate?

**Answer:**

**Option 1: Same Thread (Our Implementation)**

```cpp
void receiver_loop() {
    while (running_) {
        if (!connected_) {
            reconnect(); // Blocks this thread
            continue;
        }
        
        // Normal receive logic
        recv_and_process();
    }
}
```

**Pros:**
- Simpler code, no thread synchronization
- Natural pause during reconnection
- No race conditions on socket FD

**Cons:**
- Receiver thread blocked during reconnect
- Cannot process other tasks during reconnect

**Option 2: Separate Thread**

```cpp
// Receiver thread
void receiver_loop() {
    while (running_) {
        if (!connected_) {
            sleep_briefly();
            continue; // Don't block
        }
        
        recv_and_process();
    }
}

// Reconnection thread
void reconnector_loop() {
    while (running_) {
        if (!connected_) {
            reconnect(); // Runs independently
        }
        sleep(1s);
    }
}
```

**Pros:**
- Receiver thread never blocks
- Can handle other tasks during reconnect
- Better separation of concerns

**Cons:**
- Need synchronization on connected_ flag
- More complex state management
- Potential race on socket FD

**Decision matrix:**

| Criteria | Same Thread | Separate Thread |
|----------|-------------|-----------------|
| Simplicity | ✓✓ | ✗ |
| No blocking | ✗ | ✓✓ |
| No races | ✓✓ | ✗ |
| Resource usage | ✓ (one thread) | ✗ (two threads) |

**Recommendation:**

- **Same thread if:**
  - Single purpose application (feed handler)
  - Reconnection is rare event
  - Simplicity is priority

- **Separate thread if:**
  - Multiple concurrent tasks
  - Need to stay responsive during reconnect
  - Can handle synchronization complexity

**For our feed handler:** Same thread is sufficient.

**Alternative: Asynchronous Reconnect**
```cpp
void receiver_loop() {
    while (running_) {
        if (!connected_) {
            if (!reconnect_in_progress_) {
                reconnect_async(); // Returns immediately
                reconnect_in_progress_ = true;
            }
            continue;
        }
        
        recv_and_process();
    }
}

void reconnect_async() {
    std::thread([this]() {
        if (reconnect()) {
            reconnect_in_progress_ = false;
        }
    }).detach();
}
```

Best of both worlds, but need to handle concurrent reconnect attempts.

---

## 3. Binary Protocol Parser Questions

### Q3.1: How do you buffer incomplete messages across multiple recv() calls efficiently?

**Answer:**

**Problem:**
```
recv() call 1: [MSG1][MSG2][MS...  (partial MSG3)
recv() call 2: ...G3][MSG4]
```

**Solution: Ring Buffer**

```cpp
class BinaryParser {
private:
    std::vector<uint8_t> buffer_;  // Fixed-size buffer
    size_t buffer_pos_;            // Current fill level
    
public:
    size_t parse(const void* data, size_t len) {
        // 1. Append new data to buffer
        memcpy(&buffer_[buffer_pos_], data, len);
        buffer_pos_ += len;
        
        // 2. Try to parse complete messages
        while (try_parse_message()) {
            // Continues until no complete message
        }
        
        return len; // All data consumed
    }
    
private:
    bool try_parse_message() {
        // Need at least header
        if (buffer_pos_ < sizeof(MessageHeader))
            return false;
        
        // Read message type and determine size
        MessageHeader* hdr = (MessageHeader*)buffer_.data();
        size_t msg_size = get_message_size(hdr->msg_type);
        
        // Check if complete message available
        if (buffer_pos_ < msg_size)
            return false; // Incomplete
        
        // Process complete message
        process_message(buffer_.data(), msg_size);
        
        // Remove from buffer (shift remaining data)
        memmove(buffer_.data(), 
                &buffer_[msg_size], 
                buffer_pos_ - msg_size);
        buffer_pos_ -= msg_size;
        
        return true; // Message parsed
    }
};
```

**Key optimizations:**

**1. Avoid frequent memmove:**
```cpp
// Instead of memmove after each message,
// accumulate and compact periodically:

if (buffer_pos_ > buffer_.size() / 2) {
    compact_buffer();
}
```

**2. Circular buffer (avoid memmove entirely):**
```cpp
class CircularBuffer {
    std::vector<uint8_t> buffer_;
    size_t read_pos_;
    size_t write_pos_;
    
    void add_data(const void* data, size_t len) {
        // Wrap around if necessary
        if (write_pos_ + len > buffer_.size()) {
            size_t first_part = buffer_.size() - write_pos_;
            memcpy(&buffer_[write_pos_], data, first_part);
            memcpy(&buffer_[0], (uint8_t*)data + first_part, 
                   len - first_part);
        } else {
            memcpy(&buffer_[write_pos_], data, len);
        }
        write_pos_ = (write_pos_ + len) % buffer_.size();
    }
};
```

**3. Zero-copy for aligned messages:**
```cpp
// If message is complete and aligned in recv buffer:
ssize_t n = recv(sockfd_, buffer, sizeof(buffer), 0);

size_t offset = 0;
while (offset + sizeof(MessageHeader) <= n) {
    MessageHeader* hdr = (MessageHeader*)(buffer + offset);
    size_t msg_size = get_message_size(hdr->msg_type);
    
    if (offset + msg_size <= n) {
        // Complete message, process in-place
        process_message(buffer + offset, msg_size);
        offset += msg_size;
    } else {
        // Incomplete, copy to pending buffer
        memcpy(pending_buffer_, buffer + offset, n - offset);
        pending_size_ = n - offset;
        break;
    }
}
```

**Memory usage:**
- Buffer size: 64 KB (holds ~1000 messages)
- Worst case: 1 byte of message, 64 KB - 1 bytes wasted
- Typical: 80%+ utilization

**Performance:**
- memcpy: ~10 GB/s (negligible for our rates)
- memmove: ~8 GB/s (still fast)
- Zero-copy: Best when messages align

---

### Q3.2: What happens when you detect a sequence gap - drop it or request retransmission?

**Answer:**

**TCP guarantees in-order delivery**, so sequence gaps indicate:

1. **Message lost before TCP** (e.g., server bug, dropped before send)
2. **Our parser bug** (incorrectly advanced past message)
3. **Deliberate gap** (server injected for testing)

**Not a TCP packet loss** (TCP would retransmit automatically).

**Options:**

**1. Log and Continue (Our Approach)**
```cpp
if (seq_num != expected_seq_num) {
    sequence_gaps_++;
    log_warning("Sequence gap: expected " + expected_seq_num + 
                ", got " + seq_num);
    expected_seq_num = seq_num + 1; // Resync
}
```

**Rationale:**
- Real-time data: Old data is less valuable
- Cannot go back in time to recover
- Continue with latest data

**2. Request Retransmission**
```cpp
if (seq_num != expected_seq_num) {
    // Send NACK to server
    RetransmitRequest req;
    req.start_seq = expected_seq_num;
    req.end_seq = seq_num - 1;
    send(sockfd_, &req, sizeof(req), 0);
}
```

**Requires:**
- Server maintains history buffer
- Retransmission protocol
- Additional complexity

**Use case:** Historical data replay, compliance recording

**3. Disconnect and Reconnect**
```cpp
if (seq_num != expected_seq_num) {
    log_error("Sequence gap, assuming corruption");
    disconnect_and_reconnect();
}
```

**Rationale:** Gap might indicate corrupt stream, start fresh

**4. Ignore (Dangerous)**
```cpp
// Just accept whatever sequence number arrives
expected_seq_num = seq_num + 1;
```

**Problem:** Masks bugs, silently loses data

**For market data feed handler:**

**Recommendation:** Log and continue

**Reasoning:**
1. **Latency priority:** Cannot wait for retransmission
2. **Snapshot model:** Latest price is what matters
3. **Monitoring:** Log for diagnostics
4. **Recovery:** Periodic snapshots handle missing updates

**Alternative for critical data:**
- Primary feed: Log and continue (low latency)
- Backup feed: Request retransmission (complete data)
- Reconcile offline

---

### Q3.3: How would you handle messages arriving out of order?

**Answer:**

**First:** TCP guarantees in-order delivery, so out-of-order at application level means:

1. **Multiple TCP connections** (different sockets, no ordering guarantee)
2. **UDP protocol** (no ordering guarantee)
3. **Multicast** (can arrive out of order)

**If this occurs:**

**Option 1: Reorder Buffer**
```cpp
class MessageReorderer {
    std::map<uint32_t, Message> buffer_; // Seq num → Message
    uint32_t next_expected_;
    
    void on_message(const Message& msg) {
        if (msg.seq_num == next_expected_) {
            // In order, deliver immediately
            deliver(msg);
            next_expected_++;
            
            // Deliver any buffered messages now in order
            while (buffer_.count(next_expected_)) {
                deliver(buffer_[next_expected_]);
                buffer_.erase(next_expected_);
                next_expected_++;
            }
        } else if (msg.seq_num > next_expected_) {
            // Future message, buffer it
            buffer_[msg.seq_num] = msg;
        } else {
            // Old message, already delivered or gap
            log_warning("Duplicate or old message");
        }
    }
};
```

**Problem:** Adds latency (wait for missing messages)

**Option 2: Process Out-of-Order (Last Writer Wins)**
```cpp
void on_message(const Message& msg) {
    // Always update cache, regardless of order
    cache.update(msg.symbol_id, msg.price, msg.qty);
    
    // Track gaps for diagnostics
    if (msg.seq_num != expected_seq_num) {
        sequence_gaps_++;
    }
}
```

**Pros:** Low latency
**Cons:** Might overwrite newer data with older

**Option 3: Timestamp-Based Ordering**
```cpp
void on_message(const Message& msg) {
    auto current_ts = cache.get_timestamp(msg.symbol_id);
    
    if (msg.timestamp > current_ts) {
        // This message is newer, update
        cache.update(msg.symbol_id, msg.price, msg.qty, msg.timestamp);
    } else {
        // Old message, discard
        stale_messages_++;
    }
}
```

**Best of both worlds:** Correct ordering without buffering delay

**For our feed handler:**

**Recommendation:** Timestamp-based (Option 3)

```cpp
void SymbolCache::update_quote(...) {
    // Only update if newer
    uint64_t current_ts = state.last_update_time.load();
    if (timestamp > current_ts) {
        state.best_bid.store(bid_price);
        state.last_update_time.store(timestamp);
    }
}
```

**Why it works:**
- Messages have nanosecond timestamps
- Even if reordered, timestamp determines truth
- No buffering delay
- Correct final state

---

### Q3.4: How do you prevent buffer overflow with malicious large message lengths?

**Answer:**

**Attack scenario:**
```
Malicious message:
├─ Type: 0x02 (Quote)
├─ Length field: 0xFFFFFFFF (4GB!)
└─ Parser allocates 4GB buffer → OOM
```

**Defenses:**

**1. Maximum Message Size Check**
```cpp
bool try_parse_message() {
    MessageHeader* hdr = (MessageHeader*)buffer_.data();
    size_t msg_size = get_message_size(hdr->msg_type);
    
    if (msg_size > MAX_MESSAGE_SIZE || msg_size == 0) {
        malformed_messages_++;
        
        // Skip one byte and try to resync
        memmove(buffer_.data(), &buffer_[1], buffer_pos_ - 1);
        buffer_pos_--;
        
        return false;
    }
    
    // Proceed with bounded size
}
```

```cpp
const size_t MAX_MESSAGE_SIZE = 1024; // 1KB max
```

**2. Fixed-Size Messages (Our Protocol)**
```cpp
size_t get_message_size(MessageType type) {
    switch (type) {
        case MessageType::TRADE:     return sizeof(TradeMessage);     // 32 bytes
        case MessageType::QUOTE:     return sizeof(QuoteMessage);     // 48 bytes
        case MessageType::HEARTBEAT: return sizeof(HeartbeatMessage); // 20 bytes
        default: return 0; // Invalid
    }
}
```

**No variable-length messages** → no overflow risk!

**3. Pre-Allocated Buffer**
```cpp
std::vector<uint8_t> buffer_;  // Fixed 64KB buffer

void parse(const void* data, size_t len) {
    if (buffer_pos_ + len > buffer_.size()) {
        // Would overflow, reject
        log_error("Buffer overflow attempt");
        return;
    }
    
    memcpy(&buffer_[buffer_pos_], data, len);
    buffer_pos_ += len;
}
```

**4. Checksum Validation**
```cpp
bool validate_message(const void* data, size_t size) {
    if (!validate_checksum(data, size)) {
        // Corrupted/malicious message
        return false;
    }
    // Proceed
}
```

Corrupt size field would fail checksum.

**5. Sanity Checks**
```cpp
if (hdr->msg_type > 0x03 || hdr->msg_type == 0x00) {
    // Invalid message type
    return false;
}

if (hdr->symbol_id >= num_symbols_) {
    // Invalid symbol
    return false;
}
```

**Defense in Depth:**

| Layer | Protection |
|-------|------------|
| Protocol design | Fixed-size messages |
| Parser | MAX_MESSAGE_SIZE check |
| Buffer | Pre-allocated, fixed size |
| Validation | Checksum, sanity checks |
| Resync | Skip byte and continue |

**Result:** Parser cannot be crashed or exploited by malformed messages.

---

## 4. Lock-Free Symbol Cache Questions

### Q4.1: How do you prevent readers from seeing inconsistent state during updates?

**Answer:**

**Problem:**
```cpp
// Without atomics (WRONG):
struct MarketState {
    double bid;  // 8 bytes
    double ask;  // 8 bytes
};

// Writer:
state.bid = 100.50;
state.ask = 100.55;

// Reader (concurrent):
double bid = state.bid;  // Might read 100.50
double ask = state.ask;  // Might read old ask!
```

**Torn reads:** Reader sees partial update.

**Solutions:**

**1. Atomic Operations (Our Approach)**
```cpp
struct MarketState {
    std::atomic<double> bid;
    std::atomic<double> ask;
};

// Writer:
state.bid.store(100.50, std::memory_order_release);
state.ask.store(100.55, std::memory_order_release);

// Reader:
double bid = state.bid.load(std::memory_order_acquire);
double ask = state.ask.load(std::memory_order_acquire);
```

**Guarantees:**
- Each field update is atomic
- Memory ordering ensures visibility
- No torn reads

**Limitation:** Multi-field snapshot may be inconsistent:
```cpp
// Possible: bid from update 1, ask from update 2
```

**2. Sequence Lock (SeqLock)**
```cpp
struct MarketState {
    std::atomic<uint64_t> seq;
    double bid;  // Not atomic
    double ask;
};

// Writer:
state.seq.fetch_add(1); // Odd = writing
state.bid = 100.50;
state.ask = 100.55;
state.seq.fetch_add(1); // Even = stable

// Reader:
MarketSnapshot read() {
    MarketSnapshot snap;
    uint64_t seq1, seq2;
    
    do {
        seq1 = state.seq.load();
        
        // Read data
        snap.bid = state.bid;
        snap.ask = state.ask;
        
        seq2 = state.seq.load();
    } while (seq1 != seq2 || (seq1 & 1)); // Retry if changed or odd
    
    return snap;
}
```

**Pros:** Consistent multi-field snapshot
**Cons:** Reader may retry multiple times

**3. Double Buffering**
```cpp
struct MarketState {
    double bid;
    double ask;
};

std::atomic<int> current_buffer{0};
MarketState buffers[2];

// Writer:
int next = 1 - current_buffer.load();
buffers[next].bid = 100.50;
buffers[next].ask = 100.55;
current_buffer.store(next); // Atomic flip

// Reader:
int idx = current_buffer.load();
MarketSnapshot snap;
snap.bid = buffers[idx].bid;
snap.ask = buffers[idx].ask;
```

**Pros:** Reader always sees consistent snapshot
**Cons:** Double memory usage

**Our choice:** Atomic per-field (Option 1)

**Rationale:**
- Feed handler: single value per update (bid OR ask OR ltp)
- Visualizer: multi-field inconsistency acceptable (updates every 500ms)
- Simpler than SeqLock

**For stricter consistency:**
Add `get_snapshot()` with SeqLock pattern.

---

### Q4.2: What memory ordering do you need for atomic operations?

**Answer:**

**Memory orderings available:**

| Ordering | Guarantees | Use Case |
|----------|------------|----------|
| `relaxed` | Atomic, no ordering | Counters, statistics |
| `acquire` | Prevents reordering before | Reader synchronization |
| `release` | Prevents reordering after | Writer synchronization |
| `acq_rel` | Both acquire and release | Read-modify-write |
| `seq_cst` | Sequential consistency | Default, strongest |

**For our cache:**

**Writer (Feed Handler):**
```cpp
void SymbolCache::update_bid(uint16_t symbol_id, 
                              double price, uint32_t quantity) {
    auto& state = states_[symbol_id];
    
    state.best_bid.store(price, std::memory_order_release);
    state.bid_quantity.store(quantity, std::memory_order_release);
    state.update_count.fetch_add(1, std::memory_order_relaxed);
}
```

**Why `release` for prices?**
- Ensures all previous writes are visible
- Synchronizes with reader's `acquire`

**Why `relaxed` for counters?**
- Exact value doesn't matter
- Slightly faster (no synchronization)

**Reader (Visualizer):**
```cpp
double SymbolCache::get_bid(uint16_t symbol_id) const {
    return states_[symbol_id].best_bid.load(std::memory_order_acquire);
}
```

**Why `acquire`?**
- Ensures we see all writes before `release`
- Prevents reading stale cached data

**Happens-Before Relationship:**
```
Writer:                          Reader:
├─ bid = 100.50                  
├─ qty = 1000                    
└─ store(release) ────────────> load(acquire)
                                 ├─ sees bid = 100.50
                                 └─ sees qty = 1000
```

**Incorrect: `relaxed` for prices**
```cpp
state.best_bid.store(price, std::memory_order_relaxed);

// Reader might see stale cached value!
// Or reordered with other stores!
```

**Incorrect: `seq_cst` everywhere**
```cpp
state.best_bid.store(price, std::memory_order_seq_cst);

// Works correctly, but slower (unnecessary barriers)
```

**Benchmark:**
```
relaxed store:  ~20 ns
release store:  ~40 ns  ← Our choice for prices
seq_cst store:  ~60 ns
```

**Summary:**
- **`release` for writers** (price, qty updates)
- **`acquire` for readers** (price, qty reads)
- **`relaxed` for statistics** (update counts)
- **`acq_rel` for fetch_add on prices** (if used)

---

### Q4.3: How do you handle cache line bouncing with single writer, multiple readers?

**Answer:**

**Cache line bouncing:**
```
Core 0 (Writer):              Core 1 (Reader):
├─ Load cache line            ├─ Load cache line (shared)
├─ Modify data                │
├─ Write cache line           │
└─ Invalidate other cores     ├─ Cache miss! (line invalidated)
                              └─ Reload cache line
```

**Problem:** Frequent writes cause reader cache misses → performance loss

**Solution 1: Cache Line Alignment (Our Approach)**
```cpp
struct alignas(64) MarketState {  // 64 bytes = cache line size
    std::atomic<double> best_bid;
    std::atomic<double> best_ask;
    std::atomic<uint32_t> bid_quantity;
    std::atomic<uint32_t> ask_quantity;
    std::atomic<double> last_traded_price;
    std::atomic<uint32_t> last_traded_quantity;
    std::atomic<uint64_t> last_update_time;
    std::atomic<uint64_t> update_count;
    
    // Total: ~64 bytes, fits in one cache line
};
```

**Array of states:**
```
Symbol 0: [Cache Line 0]
Symbol 1: [Cache Line 1]
Symbol 2: [Cache Line 2]
...
```

**Benefit:** Updating symbol 0 doesn't invalidate symbol 1's cache line.

**Solution 2: Padding Between Symbols**
```cpp
struct MarketState {
    std::atomic<double> best_bid;
    std::atomic<double> best_ask;
    // ... other fields ...
    
    char padding[64 - sizeof(actual_data)]; // Pad to 64 bytes
};
```

**Solution 3: Read-Only Cache Lines**
```cpp
// Separate hot (written) and cold (read-only) data
struct MarketState {
    // Hot data (written frequently)
    alignas(64) std::atomic<double> best_bid;
    
    // Cold data (read-only or infrequent)
    alignas(64) std::string symbol_name;
    double volatility;
};
```

**Solution 4: Reader Batching**
```cpp
// Instead of reading every update:
void visualizer_loop() {
    while (running) {
        // Read all symbols once every 500ms
        for (int i = 0; i < num_symbols; ++i) {
            snapshots[i] = cache.get_snapshot(i);
        }
        
        render(snapshots);
        sleep(500ms); // Infrequent reads
    }
}
```

**Reduces:** Cache line transfers (reads only 2x per second, not 100K/sec)

**Solution 5: CPU Pinning**
```cpp
// Pin writer to Core 0
cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(0, &cpuset);
pthread_setaffinity_np(writer_thread, sizeof(cpuset), &cpuset);

// Pin reader to Core 1 (shares L3 cache)
CPU_SET(1, &cpuset);
pthread_setaffinity_np(reader_thread, sizeof(cpuset), &cpuset);
```

**Benefit:** Cores 0 and 1 often share L2/L3 cache → faster synchronization

**Measurement:**
```
Without alignment:
├─ Cache misses: ~15% of reads
├─ Read latency: ~120 ns (includes cache miss)

With alignment + batching:
├─ Cache misses: ~2% of reads
├─ Read latency: ~22 ns (cache hit)
```

**Summary:**
1. **Align to cache lines** (64 bytes)
2. **Batch reader access** (every 500ms, not every message)
3. **Pin threads to nearby cores** (optional)
4. **Separate hot and cold data** (if needed)

---

### Q4.4: Do you need read-copy-update (RCU) pattern here?

**Answer:**

**RCU Pattern:**
```cpp
// Classic RCU
struct MarketState {
    double bid;
    double ask;
};

std::atomic<MarketState*> current_state;

// Writer:
MarketState* new_state = new MarketState(*current_state.load());
new_state->bid = 100.50;
new_state->ask = 100.55;

MarketState* old_state = current_state.exchange(new_state);
// Defer deletion until no readers
rcu_defer_free(old_state);

// Reader:
MarketState* state = current_state.load();
double bid = state->bid; // No atomics needed!
```

**When RCU makes sense:**
- Complex data structures (linked lists, trees)
- Large objects that can't fit in atomics
- Multiple fields must be read atomically

**Our case:**

**Don't need RCU because:**

1. **Simple flat structure:** Array of independent states
2. **Small fields:** double (8 bytes) fits in atomic
3. **Per-field updates:** Bid/ask updated independently
4. **Acceptable inconsistency:** Visualizer can handle bid from update N, ask from update N+1

**If we needed consistent snapshots:**

**Option A: RCU**
```cpp
std::atomic<MarketSnapshot*> snapshots[NUM_SYMBOLS];

// Writer updates entire snapshot
MarketSnapshot* snap = new MarketSnapshot();
snap->bid = 100.50;
snap->ask = 100.55;
snapshots[symbol_id].store(snap);
```

**Cost:** Allocation + GC overhead

**Option B: SeqLock (lighter than RCU)**
```cpp
struct SymbolState {
    std::atomic<uint64_t> seq;
    double bid;
    double ask;
};

// No allocation, just retry on conflict
```

**Cost:** Reader may retry

**Option C: Double Buffering**
```cpp
MarketState buffers[2][NUM_SYMBOLS];
std::atomic<int> current{0};

// Writer flips buffer
int next = 1 - current.load();
buffers[next][symbol_id] = new_state;
current.store(next);
```

**Cost:** 2x memory

**Decision:**
- **Current (atomics per-field):** Simplest, good enough
- **If needed:** SeqLock (no allocation overhead)
- **Avoid RCU:** Overkill for this use case

**When RCU would be necessary:**
```cpp
// Complex structure:
struct OrderBook {
    std::map<double, Order> bids;  // Can't make atomic!
    std::map<double, Order> asks;
};

// RCU makes sense here:
std::atomic<OrderBook*> order_book;
```

**Summary:** No RCU needed for our simple symbol cache. Atomic fields are sufficient.

---

## 5. Performance Measurement (LatencyTracker) Questions

### Q5.1: Sorting is O(n log n) - how can you calculate percentiles faster?

**Answer:**

**Problem:** Naively calculating percentiles requires sorting all samples.

For 1M samples: `O(n log n) = 1M * log2(1M) ≈ 20M operations`

**Solution 1: Histogram-Based Percentiles (Our Implementation)**

**Concept:** Pre-bucket latencies into histogram bins.

```cpp
class LatencyTracker {
private:
    static constexpr size_t NUM_BUCKETS = 1000;
    static constexpr uint64_t BUCKET_SIZE_NS = 100000; // 100μs per bucket
    
    std::array<std::atomic<uint64_t>, NUM_BUCKETS> histogram_;
    
public:
    void record(uint64_t latency_ns) {
        size_t bucket = std::min(latency_ns / BUCKET_SIZE_NS, 
                                 NUM_BUCKETS - 1);
        histogram_[bucket]++;
    }
    
    uint64_t get_percentile(double p) {
        uint64_t total = get_total_count();
        uint64_t target = total * p;
        
        uint64_t cumulative = 0;
        for (size_t i = 0; i < NUM_BUCKETS; ++i) {
            cumulative += histogram_[i];
            if (cumulative >= target) {
                return i * BUCKET_SIZE_NS;  // O(NUM_BUCKETS) = O(1000)
            }
        }
        return (NUM_BUCKETS - 1) * BUCKET_SIZE_NS;
    }
};
```

**Complexity:**
- `record()`: O(1) - just increment counter
- `get_percentile()`: O(NUM_BUCKETS) = O(1000) - fixed size
- **No sorting needed!**

**Trade-offs:**
- Accuracy: Approximation within bucket size (100μs in example)
- Memory: Fixed (NUM_BUCKETS * 8 bytes = 8KB)
- Speed: 20,000x faster than sorting for 1M samples

**Solution 2: HdrHistogram (High Dynamic Range)**

```cpp
// Variable-width buckets: narrower at low latencies, wider at high
struct Bucket {
    uint64_t min_value;
    uint64_t max_value;
    std::atomic<uint64_t> count;
};

std::vector<Bucket> buckets_ = {
    {0, 1000, 0},       // 0-1μs (1ns precision)
    {1000, 10000, 0},   // 1-10μs (100ns precision)
    {10000, 100000, 0}, // 10-100μs (1μs precision)
    // ... exponentially increasing
};
```

**Benefits:**
- High precision at low latencies (where it matters)
- Lower precision at high latencies (outliers)
- Still O(NUM_BUCKETS) for percentile calculation

**Solution 3: Reservoir Sampling + Quick Select**

```cpp
// Keep fixed-size sample (e.g., 10K of 1M)
static constexpr size_t RESERVOIR_SIZE = 10000;
std::array<uint64_t, RESERVOIR_SIZE> samples_;
std::atomic<uint64_t> total_count_{0};

void record(uint64_t latency) {
    uint64_t count = total_count_++;
    
    if (count < RESERVOIR_SIZE) {
        samples_[count] = latency;
    } else {
        // Probabilistic replacement
        uint64_t idx = rand() % (count + 1);
        if (idx < RESERVOIR_SIZE) {
            samples_[idx] = latency;
        }
    }
}

uint64_t get_percentile(double p) {
    size_t k = RESERVOIR_SIZE * p;
    return quick_select(samples_, k); // O(n) average
}
```

**Complexity:** O(RESERVOIR_SIZE) instead of O(total_samples)

**Solution 4: T-Digest**

**Concept:** Adaptive clustering algorithm that maintains accuracy at extremes (p99, p999).

```cpp
// Conceptual - requires library like tdigest-cpp
TDigest digest(compression = 100);

void record(uint64_t latency) {
    digest.add(latency);  // O(log compression)
}

uint64_t get_percentile(double p) {
    return digest.quantile(p);  // O(log compression)
}
```

**Benefits:** Better accuracy at tail percentiles (p99.9, p99.99)

**Comparison Table:**

| Method | record() | percentile() | Memory | Accuracy |
|--------|----------|--------------|--------|----------|
| Full sort | O(1) | O(n log n) | O(n) | Exact |
| Histogram | O(1) | O(buckets) | O(buckets) | ±bucket_size |
| HdrHistogram | O(log buckets) | O(buckets) | O(buckets) | Variable |
| Reservoir | O(1) | O(sample_size) | O(sample_size) | Approximate |
| T-Digest | O(log c) | O(log c) | O(compression) | High at tails |

**Our choice:** **Histogram** - Simple, predictable, fast enough for real-time display.

---

### Q5.2: How do you minimize the overhead of timestamping?

**Answer:**

**Problem:** Timestamping adds latency to the critical path.

**Measurement:**
```cpp
auto t0 = std::chrono::high_resolution_clock::now();
// Cost of now() itself: ~20-30ns on modern x86
```

**Solution 1: Use Fastest Clock (Our Implementation)**

```cpp
// DON'T use system_clock (may call syscall)
auto t = std::chrono::system_clock::now();  // ~500-1000ns (syscall!)

// DO use steady_clock or high_resolution_clock
auto t = std::chrono::steady_clock::now();  // ~20-30ns (RDTSC)
```

**On x86/x64:**
- `steady_clock` typically uses RDTSC (Read Time-Stamp Counter)
- Direct CPU instruction, no syscall
- ~20-30ns overhead

**Solution 2: RDTSC Directly (Lower Level)**

```cpp
inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// Usage:
uint64_t start = rdtsc();
do_work();
uint64_t end = rdtsc();
uint64_t cycles = end - start;

// Convert cycles to nanoseconds
uint64_t ns = cycles * 1000000000ULL / cpu_freq_hz;
```

**Cost:** ~5-10ns (just CPU register read)

**Caveat:** Not monotonic if CPU frequency changes

**Solution 3: Batch Timestamping**

```cpp
// Instead of timestamping every message:
void parse_batch(const uint8_t* buffer, size_t len) {
    auto batch_start = std::chrono::steady_clock::now(); // Once!
    
    size_t offset = 0;
    while (offset < len) {
        Message* msg = parse_message(buffer + offset);
        
        // Assume all messages in batch have similar latency
        msg->receive_time = batch_start;
        
        offset += msg->size;
    }
}
```

**Benefit:** 1 timestamp for N messages (N×20ns → 20ns amortized)

**Drawback:** Less precise (assumes uniform latency within batch)

**Solution 4: Sample Timestamping**

```cpp
void record_latency(uint64_t latency) {
    static std::atomic<uint64_t> counter{0};
    
    // Only timestamp 1 in 100 messages
    if ((counter++ % 100) == 0) {
        auto t = std::chrono::steady_clock::now();
        latency_tracker_.record(t, latency);
    }
}
```

**Benefit:** 100× less overhead
**Drawback:** Only 1% sample coverage (usually fine for statistics)

**Solution 5: Lazy Timestamping**

```cpp
struct Message {
    uint64_t server_timestamp;  // Sent by server (already has it)
    // Don't timestamp on client side!
};

void process_message(const Message& msg) {
    auto now = std::chrono::steady_clock::now();
    uint64_t latency = now - msg.server_timestamp;  // Only 1 timestamp
    
    latency_tracker_.record(latency);
}
```

**Benefit:** Reuse server timestamp, only 1 client timestamp

**Solution 6: Hardware Timestamping (Advanced)**

```cpp
// Enable hardware timestamping on NIC
setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, ...);

// Kernel/NIC timestamps packet on arrival
struct msghdr msg;
recvmsg(sock, &msg, 0);

// Extract hardware timestamp from ancillary data
struct timespec hw_timestamp = extract_timestamp(&msg);
```

**Benefit:** ~100ns precision, no CPU overhead
**Drawback:** Requires kernel/driver support

**Comparison:**

| Method | Overhead | Precision | Complexity |
|--------|----------|-----------|------------|
| system_clock::now() | ~500-1000ns | 1ns | Low |
| steady_clock::now() | ~20-30ns | 1ns | Low |
| RDTSC | ~5-10ns | 1 cycle | Medium |
| Batch timestamping | ~20ns / N | N×batch_time | Low |
| Sampling (1 in 100) | ~0.2ns amort. | Statistical | Low |
| Hardware timestamp | ~100ns | <100ns | High |

**Our implementation:**
```cpp
// Use steady_clock (20-30ns is acceptable)
auto t0 = std::chrono::steady_clock::now();
process_message();
auto t1 = std::chrono::steady_clock::now();

uint64_t latency_ns = std::chrono::duration_cast<
    std::chrono::nanoseconds>(t1 - t0).count();

latency_tracker_.record(latency_ns);  // Histogram: O(1)
```

**Total overhead:** ~45ns (20ns + 20ns + 5ns for record)
**Acceptable:** < 5% of target 1μs processing time

---

### Q5.3: What granularity of histogram buckets balances accuracy vs memory?

**Answer:**

**Trade-off:**
- **Fine-grained buckets** → High accuracy, more memory
- **Coarse-grained buckets** → Low accuracy, less memory

**Analysis:**

**Example 1: Fixed Linear Buckets**

```cpp
// Option A: 1μs buckets, 0-100ms range
static constexpr size_t NUM_BUCKETS = 100000;
static constexpr uint64_t BUCKET_SIZE = 1000; // 1μs

// Memory: 100,000 * 8 bytes = 800 KB
// Accuracy: ±500ns (half bucket size)
// Range: 0-100ms
```

**Too much memory for high precision!**

```cpp
// Option B: 100μs buckets, 0-100ms range
static constexpr size_t NUM_BUCKETS = 1000;
static constexpr uint64_t BUCKET_SIZE = 100000; // 100μs

// Memory: 1,000 * 8 bytes = 8 KB
// Accuracy: ±50μs
// Range: 0-100ms
```

**Better balance!**

**Example 2: Exponential/Logarithmic Buckets**

```cpp
// Variable bucket sizes:
// [0-1μs], [1-2μs], [2-4μs], [4-8μs], ...

size_t get_bucket(uint64_t latency_ns) {
    if (latency_ns == 0) return 0;
    
    // Logarithmic bucket index
    return std::min(63 - __builtin_clzll(latency_ns), 
                    MAX_BUCKET - 1);
}

// Buckets grow exponentially:
// Bucket 0: 0-1ns
// Bucket 1: 1-2ns
// Bucket 10: 1-2μs
// Bucket 20: 1-2ms
// Bucket 30: 1-2s
```

**Benefits:**
- High precision at low latencies (critical for us)
- Low precision at high latencies (outliers)
- Only ~64 buckets needed for 0-2^64 range

**Example 3: HDR Histogram (High Dynamic Range)**

```cpp
// Hybrid approach: linear within magnitude, exponential across magnitudes
struct HdrHistogram {
    static constexpr int SIGNIFICANT_DIGITS = 2; // 1% accuracy
    
    // For each power of 10:
    // 0-10: buckets at 0, 1, 2, ..., 10     (11 buckets)
    // 10-100: buckets at 10, 11, 12, ..., 100  (91 buckets)
    // 100-1K: buckets at 100, 101, 102, ..., 1000 (901 buckets)
    // ...
};
```

**Benefits:** Configurable accuracy (e.g., 1% error), compact

**Our Requirements:**

```
Target latencies:
├─ p50:  10-20μs    (need high precision)
├─ p99:  45μs       (need high precision)
├─ p999: 120μs      (moderate precision OK)
└─ Max:  ~10ms      (coarse precision OK)
```

**Decision Matrix:**

| Bucket Size | Num Buckets | Memory | p99 Accuracy | Range |
|-------------|-------------|--------|--------------|-------|
| 1μs | 10,000 | 80 KB | ±0.5μs | 0-10ms |
| 10μs | 1,000 | 8 KB | ±5μs | 0-10ms |
| 100μs | 1,000 | 8 KB | ±50μs | 0-100ms |
| Exponential (64) | 64 | 512 B | ~2% | 0-2^64 |
| HDR (2 digits) | ~2,000 | 16 KB | ±1% | 0-hours |

**Recommended:**

**For low-latency feed handler:**
```cpp
// Linear buckets for main range, overflow bucket for outliers
static constexpr size_t NUM_LINEAR_BUCKETS = 1000;
static constexpr uint64_t BUCKET_SIZE = 10000; // 10μs

struct LatencyHistogram {
    std::array<std::atomic<uint64_t>, NUM_LINEAR_BUCKETS> buckets_;
    std::atomic<uint64_t> overflow_bucket_; // > 10ms
    
    void record(uint64_t latency_ns) {
        size_t bucket = latency_ns / BUCKET_SIZE;
        
        if (bucket < NUM_LINEAR_BUCKETS) {
            buckets_[bucket]++;
        } else {
            overflow_bucket_++;
        }
    }
};

// Memory: 1000 * 8 + 8 = ~8 KB
// Accuracy: ±5μs (good for p99 = 45μs)
// Range: 0-10ms (covers 99.99% of samples)
```

**Alternative for extreme tail tracking:**
```cpp
// Two-tier histogram
struct TieredHistogram {
    // Fine-grained for p50-p99 range (0-100μs)
    std::array<std::atomic<uint64_t>, 100> fine_buckets_;  // 1μs each
    
    // Coarse-grained for p99-p99.99 range (100μs-10ms)
    std::array<std::atomic<uint64_t>, 100> coarse_buckets_; // 100μs each
    
    // Single overflow for extreme outliers (>10ms)
    std::atomic<uint64_t> overflow_;
};

// Memory: 200 * 8 + 8 = ~1.6 KB
// Accuracy: ±0.5μs for p99, ±50μs for p99.9
```

**Final recommendation:**

```cpp
// Production configuration
static constexpr size_t NUM_BUCKETS = 500;
static constexpr uint64_t BUCKET_SIZE_NS = 20000; // 20μs

// Memory: 500 * 8 = 4 KB (negligible)
// Accuracy: ±10μs (acceptable for 45μs p99)
// Range: 0-10ms (covers normal operation)
// Max error: 10μs / 45μs = 22% (at p99 boundary)
```

**Summary:**
- **8-16 KB** total memory is sweet spot
- **10-20μs bucket size** for our latency range
- **500-1000 buckets** balances memory and accuracy
- **Exponential buckets** if range > 1000× (not our case)

---

## Summary of Answers

**Server:**
- Broadcast: Use non-blocking sends with epoll for flow control
- Slow clients: Mark, queue, eventually disconnect
- Fair distribution: Per-client queuing with bounded size
- 1000+ clients: Use UDP multicast or thread pool

**Client:**
- Edge-triggered: Lower latency, better batching
- EAGAIN: Normal for non-blocking, signals buffer empty
- Buffer full: TCP flow control, increase buffer size
- Silent drop: Heartbeat + TCP keepalive
- Reconnect thread: Same thread for simplicity

**Parser:**
- Incomplete messages: Ring buffer with memmove
- Sequence gap: Log and continue (real-time priority)
- Out-of-order: Timestamp-based ordering
- Overflow: Fixed-size messages + MAX_MESSAGE_SIZE

**Cache:**
- Consistent reads: Atomic operations with memory ordering
- Memory ordering: `release` for writes, `acquire` for reads
- Cache bouncing: Align to cache lines, batch reads
- RCU: Not needed, atomics are sufficient

**Performance Measurement:**
- Fast percentiles: Histogram-based O(buckets) instead of O(n log n) sorting
- Minimize timestamp overhead: Use steady_clock (~20-30ns), not system_clock
- Bucket granularity: 10-20μs buckets, 500-1000 total, 8-16 KB memory

All design decisions prioritize **low latency** and **simplicity** while maintaining **correctness** for a high-frequency market data feed handler.
