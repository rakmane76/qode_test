#ifndef VISUALIZER_H
#define VISUALIZER_H

#include "common/cache.h"
#include "common/latency_tracker.h"
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

namespace mdfh {

struct SymbolDisplay {
    uint16_t symbol_id;
    std::string symbol_name;
    double bid;
    double ask;
    double ltp;
    uint64_t volume;
    double change_pct;
    uint64_t update_count;
};

class Visualizer {
public:
    Visualizer(const SymbolCache& cache, size_t num_symbols);
    ~Visualizer();
    
    // Start visualization
    void start();
    
    // Stop visualization
    void stop();
    
    // Update statistics
    void update_stats(uint64_t messages, uint64_t msg_rate, 
                      const LatencyStats& latency);
    
    // Set connection info
    void set_connection_info(const std::string& host, uint16_t port, bool connected);
    
    // Set symbol names
    void set_symbol_names(const std::vector<std::string>& names);
    
private:
    const SymbolCache& cache_;
    size_t num_symbols_;
    
    std::atomic<bool> running_;
    std::thread display_thread_;
    
    // Statistics
    std::atomic<uint64_t> total_messages_;
    std::atomic<uint64_t> message_rate_;
    LatencyStats current_latency_;
    
    // Connection info
    std::string host_;
    uint16_t port_;
    std::atomic<bool> connected_;
    std::chrono::steady_clock::time_point start_time_;
    
    // Symbol names
    std::vector<std::string> symbol_names_;
    
    // Display parameters
    static constexpr size_t TOP_N_SYMBOLS = 20;
    static constexpr int UPDATE_INTERVAL_MS = 500;
    
    // Display thread function
    void display_loop();
    
    // Render functions
    void clear_screen();
    void draw_header();
    void draw_symbol_table(const std::vector<SymbolDisplay>& symbols);
    void draw_statistics();
    
    // Get top N active symbols
    std::vector<SymbolDisplay> get_top_symbols();
    
    // Format helpers
    std::string format_price(double price);
    std::string format_volume(uint64_t volume);
    std::string format_change(double change_pct);
    std::string format_duration(std::chrono::seconds duration);
    
    // Color codes
    const char* get_color_for_change(double change_pct);
};

} // namespace mdfh

#endif // VISUALIZER_H
