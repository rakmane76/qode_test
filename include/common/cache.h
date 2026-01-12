#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <atomic>
#include <vector>
#include <array>

namespace mdfh {

// Aligned to cache line to prevent false sharing
struct alignas(64) MarketState {
    std::atomic<uint64_t> sequence;  // Seqlock: odd = writing, even = stable
    double best_bid;
    double best_ask;
    uint32_t bid_quantity;
    uint32_t ask_quantity;
    double last_traded_price;
    uint32_t last_traded_quantity;
    uint64_t last_update_time;
    uint64_t update_count;
    
    MarketState() 
        : sequence(0), best_bid(0.0), best_ask(0.0), 
          bid_quantity(0), ask_quantity(0),
          last_traded_price(0.0), last_traded_quantity(0),
          last_update_time(0), update_count(0) {}
};

// Snapshot for consistent reads
struct MarketSnapshot {
    double best_bid;
    double best_ask;
    uint32_t bid_quantity;
    uint32_t ask_quantity;
    double last_traded_price;
    uint32_t last_traded_quantity;
    uint64_t last_update_time;
    uint64_t update_count;
};

class SymbolCache {
public:
    explicit SymbolCache(size_t num_symbols);
    ~SymbolCache();
    
    // Writer operations (single writer thread)
    void update_bid(uint16_t symbol_id, double price, uint32_t quantity);
    void update_ask(uint16_t symbol_id, double price, uint32_t quantity);
    void update_trade(uint16_t symbol_id, double price, uint32_t quantity);
    void update_quote(uint16_t symbol_id, double bid_price, uint32_t bid_qty,
                      double ask_price, uint32_t ask_qty);
    
    // Reader operations (lock-free, multiple readers)
    MarketSnapshot get_snapshot(uint16_t symbol_id) const;
    double get_bid(uint16_t symbol_id) const;
    double get_ask(uint16_t symbol_id) const;
    double get_ltp(uint16_t symbol_id) const;
    
    // Statistics
    size_t get_num_symbols() const { return num_symbols_; }
    uint64_t get_total_updates() const;
    
private:
    size_t num_symbols_;
    std::vector<MarketState> states_;
    
    bool is_valid_symbol(uint16_t symbol_id) const {
        return symbol_id < num_symbols_;
    }
};

} // namespace mdfh

#endif // CACHE_H
