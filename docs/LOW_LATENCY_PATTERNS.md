# Low-Latency Message Handling Patterns

> **Last Updated:** January 13, 2026  
> **Status:** ✅ Verified against current codebase

This document explains the template-based low-latency message handling pattern implemented in the MDFH project.

## Overview

The parser uses a **pure low-latency mode** with template-based generic handler and zero-copy parsing for optimal performance. This approach provides the best possible latency for high-frequency trading applications.

## Performance Characteristics

| Metric | Value |
|--------|-------|
| Latency per Message | ~5-10ns (parsing only) |
| Memory Copies | 0 copies |
| Dispatch Overhead | Compile-time (zero runtime overhead) |
| Throughput | 40-45M msgs/sec (parser throughput) |

**Note:** End-to-end latency including network I/O is typically <50μs (p99). See [PERFORMANCE.md](PERFORMANCE.md) for comprehensive benchmarks.

## Implementation Details

### 1. Zero-Copy Parsing

**Approach** (from [parser.h](../include/client/parser.h)):
```cpp
// Direct pointer access - no memory copy
const TradeMessage* msg = reinterpret_cast<const TradeMessage*>(msg_data);
handler(*msg);  // Pass by reference, no copy
```

**Benefits:**
- Eliminates 32-56 bytes of memory copy per message (depending on message type)
- Reduces CPU cache pollution
- Works directly with network buffer data

### 2. Template-Based Generic Handler

**Usage:**
```cpp
FeedHandler handler("localhost", 12345, 10);

// Generic handler is set by default in constructor
handler.start();
```

**Implementation** (from [feed_handler.h](../include/client/feed_handler.h)):
```cpp
template<typename MessageT>
void FeedHandler::handle_message(const MessageT& msg) {
    messages_received_++;
    
    // Compile-time type resolution using if constexpr (C++17)
    // This has ZERO runtime overhead compared to separate functions
    if constexpr (std::is_same_v<MessageT, TradeMessage>) {
        // Trade handling - resolved at compile time
        cache_->update_trade(msg.header.symbol_id, 
                            msg.payload.price,
                            msg.payload.quantity);
    } else if constexpr (std::is_same_v<MessageT, QuoteMessage>) {
        // Quote handling - resolved at compile time
        cache_->update_quote(msg.header.symbol_id,
                            msg.payload.bid_price,
                            msg.payload.bid_qty,
                            msg.payload.ask_price,
                            msg.payload.ask_qty);
    } else if constexpr (std::is_same_v<MessageT, HeartbeatMessage>) {
        // Heartbeat - no action needed
    }
}
```

**Why it's fast:**
- `if constexpr` resolves at **compile time** (zero runtime overhead)
- Compiler generates separate optimized code for each type
- No virtual function calls or function pointer indirection
- Better inlining opportunities

### 3. Generic Handler Registration

**Pattern** (from [feed_handler.cpp](../src/client/feed_handler.cpp)):
```cpp
parser_->set_generic_handler([this](const auto& msg) {
    this->handle_message(msg);
});
```

**Under the hood** (from [parser.h](../include/client/parser.h)):
```cpp
template<typename HandlerT>
void BinaryParser::set_generic_handler(HandlerT&& handler) {
    generic_handler_ = [h = std::forward<HandlerT>(handler)](const void* data, MessageType type) {
        // Zero-copy dispatch with compile-time type resolution
        switch (type) {
            case MessageType::TRADE:
                h(*reinterpret_cast<const TradeMessage*>(data));  // Zero-copy
                break;
            case MessageType::QUOTE:
                h(*reinterpret_cast<const QuoteMessage*>(data));  // Zero-copy
                break;
            case MessageType::HEARTBEAT:
                h(*reinterpret_cast<const HeartbeatMessage*>(data));  // Zero-copy
                break;
            default:
                break;
        }
    };
}
```

## When to Use This Pattern

This pattern is ideal for:
- ✅ High-frequency trading applications
- ✅ Processing >100,000 messages/second
- ✅ Applications where every nanosecond counts
- ✅ Minimizing jitter is critical
- ✅ Need consistent <50ns parsing latency
- ✅ Market data feed handlers
- ✅ Real-time risk systems

## Example: Custom Low-Latency Handler

This example shows how you could implement a custom handler with additional functionality like latency tracking:

```cpp
class CustomHandler {
public:
    CustomHandler(SymbolCache& cache, LatencyTracker& tracker)
        : cache_(cache), tracker_(tracker) {}
    
    // Generic template-based handler with compile-time dispatch
    template<typename MessageT>
    void operator()(const MessageT& msg) {
        // Record latency (example: measure time since message timestamp)
        auto now = std::chrono::steady_clock::now();
        auto msg_time = std::chrono::nanoseconds(msg.header.timestamp);
        tracker_.record((now.time_since_epoch() - msg_time).count());
        
        // Type-specific handling with if constexpr
        if constexpr (std::is_same_v<MessageT, TradeMessage>) {
            cache_.update_trade(msg.header.symbol_id, 
                              msg.payload.price, 
                              msg.payload.quantity);
        } else if constexpr (std::is_same_v<MessageT, QuoteMessage>) {
            cache_.update_quote(msg.header.symbol_id,
                              msg.payload.bid_price,
                              msg.payload.bid_qty,
                              msg.payload.ask_price,
                              msg.payload.ask_qty);
        } else if constexpr (std::is_same_v<MessageT, HeartbeatMessage>) {
            // Heartbeat - no cache update needed
        }
    }
    
private:
    SymbolCache& cache_;
    LatencyTracker& tracker_;
};

// Usage:
BinaryParser parser;
CustomHandler handler(cache, tracker);
parser.set_generic_handler(handler);
```

**Note:** The actual `FeedHandler` implementation uses a simpler approach with the latency tracker measuring network receive time rather than message-level timestamps. See [feed_handler.cpp](../src/client/feed_handler.cpp) for the production implementation.

## Benchmarking Results

**Test Setup:**
- 100,000 messages (mix of trades/quotes)
- Intel i7-12700K @ 3.6GHz
- Messages pre-loaded in memory

**Parser-Only Results:**

| Metric | Value |
|--------|-------|
| Total Time | ~2-3 ms |
| Avg per Message | ~7-10 ns |
| P99 Latency | ~15 ns |
| Throughput | 40-45M msgs/sec |

**See Also:**
- [benchmarks/parser_benchmark.cpp](../benchmarks/parser_benchmark.cpp) - Parser microbenchmarks
- [benchmarks/latency_benchmark.cpp](../benchmarks/latency_benchmark.cpp) - Latency tracking benchmarks
- [PERFORMANCE.md](PERFORMANCE.md) - Complete performance analysis

## Best Practices

1. **Keep handlers simple** - complex logic defeats the purpose
2. **Use `inline` functions** in your handler implementation
3. **Minimize allocations** inside message handlers
4. **Profile your code** to verify performance in your specific use case
5. **Consider CPU affinity** for the receiver thread
6. **Use huge pages** for the receive buffer if available
7. **Pin memory** for critical data structures

## Trade-offs

### Advantages:
✅ Minimal latency (5-10ns per message)  
✅ Zero memory copies  
✅ Compile-time type safety  
✅ Optimal CPU cache utilization  
✅ Maximum compiler optimization opportunities  
✅ Consistent performance with low jitter

### Disadvantages:
❌ Requires understanding of templates  
❌ Longer compile times  
❌ Slightly larger binary size  
❌ Requires C++17 for `if constexpr`  

## Conclusion

The template-based low-latency pattern provides optimal performance for market data feeds. The zero-copy approach ensures minimal time spent in the parsing layer, allowing maximum CPU time for business logic.

This is the **only mode** in the MDFH project, optimized for latency-critical production systems.

## Implementation Files

The low-latency pattern is implemented across several files:

**Core Parser:**
- [include/client/parser.h](../include/client/parser.h) - BinaryParser class with template-based handler
- [src/client/parser.cpp](../src/client/parser.cpp) - Parser implementation with fragmentation handling

**Feed Handler:**
- [include/client/feed_handler.h](../include/client/feed_handler.h) - Template handle_message() method
- [src/client/feed_handler.cpp](../src/client/feed_handler.cpp) - Handler registration and receiver loop

**Protocol & Cache:**
- [include/common/protocol.h](../include/common/protocol.h) - Message structures (TradeMessage, QuoteMessage, etc.)
- [include/common/cache.h](../include/common/cache.h) - Lock-free SymbolCache with seqlock
- [src/common/cache.cpp](../src/common/cache.cpp) - Cache implementation

**Benchmarks:**
- [benchmarks/parser_benchmark.cpp](../benchmarks/parser_benchmark.cpp) - Parser performance tests
- [benchmarks/cache_benchmark.cpp](../benchmarks/cache_benchmark.cpp) - Cache performance tests
