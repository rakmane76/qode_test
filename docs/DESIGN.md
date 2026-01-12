# System Design Document

## 1. System Architecture

### 1.1 High-Level Architecture

The system consists of two main components communicating over TCP:

**Server Side: Exchange Simulator**
- TCP Server (epoll-based)
  - Client Manager (handles connections)
  - Tick Generator (GBM Engine)
    - Generates realistic market data
    - Broadcasts to all connected clients

**Communication Layer**
- Binary Protocol over TCP
- Low-latency message format
- Edge-triggered epoll for efficiency

**Client Side: Feed Handler**
- TCP Client (non-blocking socket)
  - Receives binary market data stream
- Binary Parser (Zero-Copy)
  - Parses messages without allocation
  - Generic handler for low latency
- Symbol Cache (Lock-Free)
  - Atomic operations for concurrent access
  - Sub-50ns read latency
- Visualizer (Terminal)
  - Real-time market data display
  - Independent display thread

**Data Flow:**
```
Server: GBM Engine → Serialize → TCP Send
           ↓
    [Network: TCP Stream]
           ↓
Client: TCP Recv → Parse → Cache Update → Visualize
```

### 1.2 Thread Model

**Server (Exchange Simulator):**
- Main Thread: epoll event loop for accepting connections and handling client events
- Tick Generation Thread: Generates market data using GBM and broadcasts to clients

**Client (Feed Handler):**
- Main Thread: Application control and visualization updates
- Receiver Thread: Non-blocking socket receive and message parsing
- Display Thread: Terminal rendering (within Visualizer)

### 1.3 Data Flow

```
Symbol State (GBM) → Generate Message → Serialize Binary
       ↓
Broadcast to All Clients (send() loop)
       ↓
[Network]
       ↓
TCP Receive Buffer → Parser Buffer → Parse Message
       ↓
Update Symbol Cache (Lock-Free Atomic Operations)
       ↓
Visualizer Reads Cache → Render to Terminal
```

## 2. Geometric Brownian Motion Implementation

### 2.1 Mathematical Model

Stock prices follow the stochastic differential equation:

```
dS = μ * S * dt + σ * S * dW
```

Where:
- `S` = current stock price
- `μ` = drift coefficient (trend direction)
- `σ` = volatility coefficient (randomness)
- `dt` = time step
- `dW` = Wiener process increment

### 2.2 Discretization

For simulation, we discretize:

```
S(t+dt) = S(t) + μ * S(t) * dt + σ * S(t) * sqrt(dt) * Z
```

Where `Z ~ N(0,1)` is a standard normal random variable.

### 2.3 Box-Muller Transform

To generate standard normal random variables from uniform random variables:

```cpp
U1, U2 ~ Uniform(0,1)
R = sqrt(-2 * ln(U1))
θ = 2π * U2
Z0 = R * cos(θ)
Z1 = R * sin(θ)
```

Both Z0 and Z1 are independent N(0,1) random variables.

### 2.4 Parameter Selection

| Symbol Type | μ (drift) | σ (volatility) | Starting Price |
|-------------|-----------|----------------|----------------|
| Large Cap   | ±0.02     | 0.01 - 0.02    | ₹1000 - ₹5000 |
| Mid Cap     | ±0.03     | 0.02 - 0.04    | ₹500 - ₹1000  |
| Small Cap   | ±0.05     | 0.04 - 0.06    | ₹100 - ₹500   |

Time step: `dt = 0.001` (simulating ~1ms intervals)

### 2.5 Bid-Ask Spread Generation

Spread as percentage of price: 0.05% - 0.2%

```
spread = price * (0.0005 + random() * 0.0015)
bid = price - spread/2
ask = price + spread/2
```

## 3. Network Layer Design

### 3.1 Server-Side Architecture

**epoll Event Loop:**
```cpp
while (running) {
    events = epoll_wait(epoll_fd, ...)
    for each event:
        if (event.fd == server_fd):
            accept_new_client()
        else if (event.flags & EPOLLERR):
            disconnect_client(event.fd)
}
```

**Broadcasting Strategy:**
```cpp
void broadcast_message(msg, len):
    for each client_fd:
        send(client_fd, msg, len, MSG_NOSIGNAL)
        if (send_error):
            mark_slow_client(client_fd)
```

**Slow Client Detection:**
- If send() returns EAGAIN/EWOULDBLOCK → buffer full → slow client
- Option 1: Drop messages for slow clients
- Option 2: Disconnect slow clients after threshold
- Current: Mark as slow, continue sending (best-effort)

### 3.2 Client-Side Architecture

**Edge-Triggered epoll:**
```cpp
epoll_event.events = EPOLLIN | EPOLLET
```

**Benefits of Edge-Triggered:**
- Notification only on state change (reduces syscalls)
- Forces complete buffer draining (better for high-throughput)
- Lower latency for bursty traffic

**Receive Loop:**
```cpp
while (running):
    n = recv(fd, buffer, size, 0)
    if (n > 0):
        parse(buffer, n)
    else if (n == 0):
        connection_closed()
    else if (errno == EAGAIN):
        continue  // No data available
    else:
        error_occurred()
```

### 3.3 TCP Optimizations

**Socket Options:**
```cpp
TCP_NODELAY = 1          // Disable Nagle's algorithm
SO_RCVBUF = 4MB          // Large receive buffer
SO_SNDBUF = 4MB          // Large send buffer (server)
SO_PRIORITY = 6          // High priority for QoS
```

### 3.4 Connection Management

**Reconnection with Exponential Backoff:**
```
Attempt 1: wait 100ms
Attempt 2: wait 200ms
Attempt 3: wait 400ms
...
Attempt N: wait min(800ms * 2^N, 30s)
```

After 10 failed attempts, give up and report error.

## 4. Memory Management Strategy

### 4.1 Buffer Allocation

**Network Buffers:**
- Pre-allocated fixed-size buffers (64KB)
- No dynamic allocation in hot path
- Reused across receive calls

**Parser Buffer:**
- Ring buffer for handling fragmentation
- Fixed size (64KB)
- Messages copied in, processed, then removed

**Memory Pool (Optional):**
```cpp
class MemoryPool {
    vector<uint8_t> memory;      // Contiguous allocation
    vector<void*> free_list;     // Available blocks
    mutex lock;                  // For thread-safety
};
```

### 4.2 Cache Line Alignment

```cpp
struct alignas(64) MarketState {
    atomic<double> best_bid;
    atomic<double> best_ask;
    // ... other fields
};
```

Each `MarketState` aligned to 64-byte cache line to prevent false sharing.

### 4.3 Memory Ordering

**Writer (Feed Handler):**
```cpp
state.best_bid.store(price, memory_order_release);
```

**Reader (Visualizer):**
```cpp
double bid = state.best_bid.load(memory_order_acquire);
```

Ensures visibility of updates without locks.

## 5. Concurrency Model

### 5.1 Lock-Free Symbol Cache

**Single Writer, Multiple Readers (SWMR):**
- One feed handler thread updates cache
- Multiple reader threads (visualizer, query APIs) read cache
- No mutex locks in hot path

**Atomic Operations:**
```cpp
atomic<double> with memory_order_release/acquire
```

**Consistency Guarantees:**
- Individual fields may be read independently
- For consistent snapshot, read all fields with acquire barrier

### 5.2 False Sharing Prevention

**Cache Line Alignment Strategy:**

Each symbol state is aligned to 64-byte cache line boundaries to prevent false sharing:

- **Symbol 0 State**: 64 bytes (aligned to cache line boundary)
- **Symbol 1 State**: 64 bytes (aligned to cache line boundary)
- **Symbol 2 State**: 64 bytes (aligned to cache line boundary)
- ... and so on

**Why This Matters:**
- Modern CPUs have 64-byte cache lines
- Multiple threads updating different symbols won't invalidate each other's cache
- Each symbol's data occupies exactly one cache line
- Prevents performance degradation from false sharing

**Implementation:**
```cpp
struct alignas(64) SymbolState {
    std::atomic<double> bid;
    std::atomic<double> ask;
    std::atomic<double> ltp;
    std::atomic<uint64_t> volume;
    std::atomic<uint64_t> update_count;
    // ... padded to 64 bytes
};
```

Each symbol state is guaranteed to reside on a separate cache line.

### 5.3 Parser Concurrency

Parser runs in receiver thread:
- No synchronization needed (single thread)
- Callbacks update cache using atomic operations

## 6. Visualization Design

### 6.1 Update Strategy

**Polling Approach:**
```cpp
while (running):
    snapshot = read_top_symbols()
    render_screen(snapshot)
    sleep(500ms)
```

Benefits:
- Simple, no synchronization with feed handler
- Controlled update rate (avoids flicker)

### 6.2 ANSI Escape Codes

```cpp
"\033[2J"          // Clear screen
"\033[H"           // Move to home
"\033[32m"         // Green color
"\033[31m"         // Red color
"\033[0m"          // Reset color
```

Why not ncurses?
- Simpler for this use case
- Direct control over rendering
- No external dependency
- Easier to debug

### 6.3 Non-Blocking Statistics

Statistics calculated without blocking receiver:
- Atomic counters for message count
- Snapshot latency stats from tracker
- No mutex locks during rendering

## 7. Performance Optimization

### 7.1 Hot Path Identification

**Critical Paths:**
1. Socket receive → Parser → Cache update
2. Message broadcast to clients

**Optimizations:**
- Zero-copy parsing (process in-place)
- Lock-free cache updates
- Pre-allocated buffers
- Compiler optimizations (-O3, -march=native)

### 7.2 Cache Optimization

**Data Locality:**
- Symbol states stored contiguously in vector
- Hot data (price, qty) at beginning of struct
- Cold data (statistics) at end

**Prefetching:**
```cpp
__builtin_prefetch(&states[next_symbol])
```

### 7.3 System Call Minimization

- Edge-triggered epoll (fewer epoll_wait calls)
- Large buffers (fewer recv calls)
- Batched message generation

### 7.4 CPU Affinity (Optional)

```cpp
cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(2, &cpuset);  // Pin to CPU 2
pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
```

Reduces context switches and improves cache locality.

## 8. Error Handling

### 8.1 Network Errors

| Error | Handling |
|-------|----------|
| EPIPE | Client disconnected, close socket |
| ECONNRESET | Connection reset, close and cleanup |
| EAGAIN/EWOULDBLOCK | No data available, continue |
| EINTR | Interrupted syscall, retry |

### 8.2 Protocol Errors

| Error | Handling |
|-------|----------|
| Invalid message type | Skip byte, continue parsing |
| Checksum mismatch | Increment error counter, drop message |
| Sequence gap | Log gap, continue (no retransmission) |

### 8.3 Resource Exhaustion

- Accept queue full: Temporary, retry
- Memory pool empty: Allocate from heap (fallback)
- Send buffer full: Mark client as slow

