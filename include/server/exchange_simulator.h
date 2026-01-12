#ifndef EXCHANGE_SIMULATOR_H
#define EXCHANGE_SIMULATOR_H

#include <cstdint>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <sys/epoll.h>

namespace mdfh {

struct SymbolState {
    uint16_t symbol_id = 0;
    std::string symbol_name;
    double current_price = 0.0;
    double volatility = 0.0;      // σ
    double drift = 0.0;           // μ
    uint32_t seq_num = 0;
    uint32_t ticks_since_price_update = 0;  // Counter for price updates
};

class ExchangeSimulator {
public:
    // Initialize with explicit parameters
    ExchangeSimulator(uint16_t port, size_t num_symbols = 100);
    
#ifdef TESTING
    // Initialize with custom config file (for testing)
    ExchangeSimulator(uint16_t port, size_t num_symbols, const std::string& config_file);
    // Test-only accessors for verifying symbol state
    size_t get_num_loaded_symbols() const { return symbols_.size(); }
    const SymbolState &get_symbol(size_t index) const { return symbols_.at(index); }
    // Test-only accessor for client connection management
    size_t get_num_connected_clients() const { return client_fds_.size(); }
    const std::vector<int>& get_client_fds() const { return client_fds_; }
    // Test-only accessor for client subscriptions
    bool is_client_subscribed(int client_fd, uint16_t symbol_id) const;
    size_t get_client_subscription_count(int client_fd) const;
    // Expose generate_tick for testing
    void generate_tick(uint16_t symbol_id);
#endif
    
    ~ExchangeSimulator();
    
    // Start accepting connections
    void start();
    
    // Event loop
    void run();
    
    // Configuration
    void set_tick_rate(uint32_t ticks_per_second);
    void enable_fault_injection(bool enable);
    
    // Stop the simulator
    void stop();
    
private:
    // Accept new client connections
    void handle_new_connection();
    
#ifndef TESTING
    // Generate market tick using Geometric Brownian Motion (exposed in TESTING)
    void generate_tick(uint16_t symbol_id);
#endif
    
    // Broadcast message to all connected clients (or filtered by symbol)
    void broadcast_message(const void* data, size_t len, uint16_t symbol_id = 0xFFFF);
    
    // Handle client data (subscription messages)
    void handle_client_data(int client_fd);
    
    // Handle subscription message
    void handle_subscription_message(int client_fd, const uint8_t* data, size_t len);
    
    // Handle client disconnection
    void handle_client_disconnect(int client_fd);
    
    // Tick generation thread
    void tick_generation_loop();
    
    // Initialize symbols
    void initialize_symbols();
    
    // Load configuration from file (private, but used by test constructor)
    void load_config(const std::string& config_file);
    
    uint16_t port_;
    size_t num_symbols_;
    std::string symbols_file_;
    int server_fd_;
    int epoll_fd_;
    
    std::atomic<bool> running_;
    std::atomic<uint32_t> tick_rate_;
    std::atomic<bool> fault_injection_enabled_;
    
    std::vector<SymbolState> symbols_;
    std::vector<int> client_fds_;
    
    // Per-client subscription tracking: client_fd -> set of symbol_ids
    std::unordered_map<int, std::unordered_set<uint16_t>> client_subscriptions_;
    mutable std::mutex subscriptions_mutex_;
    
    std::thread tick_thread_;
    
    // Condition variable for efficient tick rate pausing
    std::mutex tick_rate_mutex_;
    std::condition_variable tick_rate_cv_;
    
    static constexpr int MAX_EVENTS = 64;
    static constexpr int MAX_CLIENTS = 1000;
};

} // namespace mdfh

#endif // EXCHANGE_SIMULATOR_H
