#ifndef FEED_HANDLER_H
#define FEED_HANDLER_H

#include "client/socket.h"
#include "client/parser.h"
#include "common/cache.h"
#include "common/latency_tracker.h"
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <type_traits>

namespace mdfh {

class FeedHandler {
public:
    FeedHandler(const std::string& host, uint16_t port, size_t num_symbols);
    ~FeedHandler();
    
    // Connect to server
    bool connect(const std::string& host, uint16_t port);
    
    // Disconnect from server
    void disconnect();
    
    // Start the feed handler
    bool start();
    
    // Stop the feed handler
    void stop();
    
    // Subscribe to symbols
    bool subscribe(const std::vector<uint16_t>& symbol_ids);
    
    // Load symbol names from CSV file
    bool load_symbols(const std::string& symbols_file);
    
    // Get symbol name by ID
    std::string get_symbol_name(uint16_t symbol_id) const;
    
    // Get symbol cache for reading
    const SymbolCache& get_cache() const { return *cache_; }
    
    struct FeedHandlerStats {
        uint64_t messages_received;
        uint64_t messages_parsed;
        uint64_t bytes_received;
        uint64_t sequence_gaps;
        uint64_t fragmented_messages;
        uint64_t checksum_errors;
    };
    
    // Get statistics
    uint64_t get_messages_received() const { return messages_received_; }
    uint64_t get_bytes_received() const { return bytes_received_; }
    LatencyStats get_latency_stats() const { return latency_tracker_->get_stats(); }
    FeedHandlerStats get_stats() const;
    
    // Connection status
    bool is_connected() const;
    
private:
    std::string host_;
    uint16_t port_;
    size_t num_symbols_;
    
    std::unique_ptr<MarketDataSocket> socket_;
    std::unique_ptr<BinaryParser> parser_;
    std::unique_ptr<SymbolCache> cache_;
    std::unique_ptr<LatencyTracker> latency_tracker_;
    
    std::atomic<bool> running_;
    std::atomic<uint64_t> messages_received_;
    std::atomic<uint64_t> bytes_received_;
    
    // Symbol names loaded from CSV file
    std::vector<std::string> symbol_names_;
    
    std::thread receiver_thread_;
    
    // Reconnection parameters
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr int INITIAL_BACKOFF_MS = 100;
    static constexpr int MAX_BACKOFF_MS = 30000;
    
    // Receiver loop
    void receiver_loop();
    
    // Reconnection with exponential backoff
    bool reconnect();
    
    // Template-based generic message handler (compile-time dispatch)
    template<typename MessageT>
    void handle_message(const MessageT& msg);
};

// Template implementation for generic low-latency message handler
template<typename MessageT>
void FeedHandler::handle_message(const MessageT& msg) {
    messages_received_++;
    
    // Compile-time type dispatch using if constexpr (C++17)
    // This has ZERO runtime overhead compared to separate functions
    if constexpr (std::is_same_v<MessageT, TradeMessage>) {
        // Trade-specific handling
        cache_->update_trade(msg.header.symbol_id, 
                            msg.payload.price,
                            msg.payload.quantity);
    } else if constexpr (std::is_same_v<MessageT, QuoteMessage>) {
        // Quote-specific handling
        cache_->update_quote(msg.header.symbol_id,
                            msg.payload.bid_price,
                            msg.payload.bid_qty,
                            msg.payload.ask_price,
                            msg.payload.ask_qty);
    } else if constexpr (std::is_same_v<MessageT, HeartbeatMessage>) {
        // Heartbeat - no action needed
    }
}

} // namespace mdfh

#endif // FEED_HANDLER_H
