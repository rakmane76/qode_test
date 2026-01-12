#include "common/cache.h"
#include <stdexcept>
#include <chrono>

namespace mdfh {

SymbolCache::SymbolCache(size_t num_symbols) 
    : num_symbols_(num_symbols), states_(num_symbols) {
}

SymbolCache::~SymbolCache() {
}

void SymbolCache::update_bid(uint16_t symbol_id, double price, uint32_t quantity) {
    if (!is_valid_symbol(symbol_id)) return;
    
    auto& state = states_[symbol_id];
    
    // Seqlock write protocol: increment to odd (write in progress)
    uint64_t seq = state.sequence.load(std::memory_order_relaxed);
    state.sequence.store(seq + 1, std::memory_order_release);
    
    state.best_bid = price;
    state.bid_quantity = quantity;
    state.last_update_time = std::chrono::steady_clock::now().time_since_epoch().count();
    state.update_count++;
    
    // Increment to even (write complete) - release ensures visibility
    state.sequence.store(seq + 2, std::memory_order_release);
}

void SymbolCache::update_ask(uint16_t symbol_id, double price, uint32_t quantity) {
    if (!is_valid_symbol(symbol_id)) return;
    
    auto& state = states_[symbol_id];
    
    uint64_t seq = state.sequence.load(std::memory_order_relaxed);
    state.sequence.store(seq + 1, std::memory_order_release);
    
    state.best_ask = price;
    state.ask_quantity = quantity;
    state.last_update_time = std::chrono::steady_clock::now().time_since_epoch().count();
    state.update_count++;
    
    state.sequence.store(seq + 2, std::memory_order_release);
}

void SymbolCache::update_trade(uint16_t symbol_id, double price, uint32_t quantity) {
    if (!is_valid_symbol(symbol_id)) return;
    
    auto& state = states_[symbol_id];
    
    uint64_t seq = state.sequence.load(std::memory_order_relaxed);
    state.sequence.store(seq + 1, std::memory_order_release);
    
    state.last_traded_price = price;
    state.last_traded_quantity = quantity;
    state.last_update_time = std::chrono::steady_clock::now().time_since_epoch().count();
    state.update_count++;
    
    state.sequence.store(seq + 2, std::memory_order_release);
}

void SymbolCache::update_quote(uint16_t symbol_id, double bid_price, uint32_t bid_qty,
                                double ask_price, uint32_t ask_qty) {
    if (!is_valid_symbol(symbol_id)) return;
    
    auto& state = states_[symbol_id];
    
    uint64_t seq = state.sequence.load(std::memory_order_relaxed);
    state.sequence.store(seq + 1, std::memory_order_release);
    
    state.best_bid = bid_price;
    state.bid_quantity = bid_qty;
    state.best_ask = ask_price;
    state.ask_quantity = ask_qty;
    state.last_update_time = std::chrono::steady_clock::now().time_since_epoch().count();
    state.update_count++;
    
    state.sequence.store(seq + 2, std::memory_order_release);
}

MarketSnapshot SymbolCache::get_snapshot(uint16_t symbol_id) const {
    MarketSnapshot snapshot{};
    
    if (!is_valid_symbol(symbol_id)) return snapshot;
    
    const auto& state = states_[symbol_id];
    
    // Seqlock read protocol: retry if sequence is odd or changed
    uint64_t seq1, seq2;
    do {
        seq1 = state.sequence.load(std::memory_order_acquire);
        
        // If odd, writer is active - spin until even
        while (seq1 & 1) {
            seq1 = state.sequence.load(std::memory_order_acquire);
        }
        
        // Read all fields (no atomic operations needed - protected by seqlock)
        snapshot.best_bid = state.best_bid;
        snapshot.best_ask = state.best_ask;
        snapshot.bid_quantity = state.bid_quantity;
        snapshot.ask_quantity = state.ask_quantity;
        snapshot.last_traded_price = state.last_traded_price;
        snapshot.last_traded_quantity = state.last_traded_quantity;
        snapshot.last_update_time = state.last_update_time;
        snapshot.update_count = state.update_count;
        
        // Check if sequence changed during read
        seq2 = state.sequence.load(std::memory_order_acquire);
    } while (seq1 != seq2);
    
    return snapshot;
}

double SymbolCache::get_bid(uint16_t symbol_id) const {
    if (!is_valid_symbol(symbol_id)) return 0.0;
    
    const auto& state = states_[symbol_id];
    uint64_t seq1, seq2;
    double value;
    
    do {
        seq1 = state.sequence.load(std::memory_order_acquire);
        while (seq1 & 1) {
            seq1 = state.sequence.load(std::memory_order_acquire);
        }
        value = state.best_bid;
        seq2 = state.sequence.load(std::memory_order_acquire);
    } while (seq1 != seq2);
    
    return value;
}

double SymbolCache::get_ask(uint16_t symbol_id) const {
    if (!is_valid_symbol(symbol_id)) return 0.0;
    
    const auto& state = states_[symbol_id];
    uint64_t seq1, seq2;
    double value;
    
    do {
        seq1 = state.sequence.load(std::memory_order_acquire);
        while (seq1 & 1) {
            seq1 = state.sequence.load(std::memory_order_acquire);
        }
        value = state.best_ask;
        seq2 = state.sequence.load(std::memory_order_acquire);
    } while (seq1 != seq2);
    
    return value;
}

double SymbolCache::get_ltp(uint16_t symbol_id) const {
    if (!is_valid_symbol(symbol_id)) return 0.0;
    
    const auto& state = states_[symbol_id];
    uint64_t seq1, seq2;
    double value;
    
    do {
        seq1 = state.sequence.load(std::memory_order_acquire);
        while (seq1 & 1) {
            seq1 = state.sequence.load(std::memory_order_acquire);
        }
        value = state.last_traded_price;
        seq2 = state.sequence.load(std::memory_order_acquire);
    } while (seq1 != seq2);
    
    return value;
}

uint64_t SymbolCache::get_total_updates() const {
    uint64_t total = 0;
    for (const auto& state : states_) {
        uint64_t seq1, seq2;
        uint64_t count;
        
        do {
            seq1 = state.sequence.load(std::memory_order_acquire);
            while (seq1 & 1) {
                seq1 = state.sequence.load(std::memory_order_acquire);
            }
            count = state.update_count;
            seq2 = state.sequence.load(std::memory_order_acquire);
        } while (seq1 != seq2);
        
        total += count;
    }
    return total;
}

} // namespace mdfh
