#include "server/exchange_simulator.h"
#include "server/tick_generator.h"
#include "server/client_manager.h"
#include "common/protocol.h"
#include "common/config_parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <chrono>
#include <random>
#include <algorithm>

namespace mdfh {

// Global config file path with default value
static constexpr const char* DEFAULT_CONFIG_FILE = "config/server.conf";

ExchangeSimulator::ExchangeSimulator(uint16_t port, size_t num_symbols)
    : port_(port),
      num_symbols_(num_symbols),
      server_fd_(-1),
      epoll_fd_(-1),
      running_(false),
      tick_rate_(100000),
      fault_injection_enabled_(false) {
    
    load_config(DEFAULT_CONFIG_FILE);
    initialize_symbols();
}

#ifdef TESTING
ExchangeSimulator::ExchangeSimulator(uint16_t port, size_t num_symbols, const std::string& config_file)
    : port_(port),
      num_symbols_(num_symbols),
      server_fd_(-1),
      epoll_fd_(-1),
      running_(false),
      tick_rate_(100000),
      fault_injection_enabled_(false) {
    
    load_config(config_file);
    initialize_symbols();
}
#endif

void ExchangeSimulator::load_config(const std::string& config_file) {
    // Try to load configuration from file
    ConfigParser config;
    if (config.load(config_file)) {
        // Use config values, but allow constructor params to override
        if (port_ == 0) {
            port_ = config.get_int("server.port", 9876);
        }
        if (num_symbols_ == 100) {
            num_symbols_ = config.get_int("market.num_symbols", 100);
        }
        tick_rate_ = config.get_int("market.tick_rate", 100000);
        symbols_file_ = config.get_string("market.symbols_file", "config/symbols.csv");
        fault_injection_enabled_ = config.get_bool("fault_injection.enabled", false);
    } else {
        // Use default values
        symbols_file_ = "config/symbols.csv";
        std::cout << "Warning: Config file not found, using defaults" << std::endl;
    }
    
    std::cout << "Exchange Simulator Configuration:" << std::endl;
    std::cout << "  Port: " << port_ << std::endl;
    std::cout << "  Symbols: " << num_symbols_ << std::endl;
    std::cout << "  Tick Rate: " << tick_rate_ << " msgs/sec" << std::endl;
    std::cout << "  Symbols File: " << symbols_file_ << std::endl;
    std::cout << "  Fault Injection: " << (fault_injection_enabled_ ? "enabled" : "disabled") << std::endl;
}

ExchangeSimulator::~ExchangeSimulator() {
    stop();
}

void ExchangeSimulator::initialize_symbols() {
    std::ifstream file(symbols_file_);
    if (!file.is_open()) {
        throw std::runtime_error("Symbol file not found: " + symbols_file_);
    }
    
    // Skip header row
    std::string header;
    std::getline(file, header);
    
    symbols_.clear();
    symbols_.resize(num_symbols_); // Pre-allocate to ensure correct indexing
    std::string line;
    size_t loaded_count = 0;
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        uint16_t symbol_id;
        std::string symbol_name;
        double price, volatility, drift;
        
        // Read: symbol_id,symbol,price,volatility,drift
        if ((iss >> symbol_id) && iss.ignore() &&
            std::getline(iss, symbol_name, ',') &&
            (iss >> price) && iss.ignore() &&
            (iss >> volatility) && iss.ignore() &&
            (iss >> drift)) {
            
            // Validate symbol_id
            if (symbol_id >= num_symbols_) {
                std::cerr << "Warning: Symbol ID " << symbol_id 
                          << " exceeds max symbols " << num_symbols_ 
                          << ", skipping" << std::endl;
                continue;
            }
            
            SymbolState sym;
            sym.symbol_id = symbol_id;
            sym.symbol_name = symbol_name;
            sym.current_price = price;
            sym.volatility = volatility;
            sym.drift = drift;
            sym.seq_num = 0;
            sym.ticks_since_price_update = 0;
            
            symbols_[symbol_id] = sym;
            loaded_count++;
        }
    }
    
    if (loaded_count == 0) {
        throw std::runtime_error("No symbols loaded from file: " + symbols_file_);
    }
    
    std::cout << "Loaded " << loaded_count << " symbols from " << symbols_file_ << std::endl;
}

void ExchangeSimulator::start() {
    // Create socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to bind socket");
    }
    
    // Listen
    if (listen(server_fd_, MAX_CLIENTS) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to listen on socket");
    }
    
    // Create epoll
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to create epoll");
    }
    
    // Add server fd to epoll
    struct epoll_event ev{};
    ev.events = EPOLLIN; // Monitor for incoming connections
    ev.data.fd = server_fd_; // Associate with server socket
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev); // Add socket to the epoll interest

    running_ = true;
    
    // Start tick generation thread
    tick_thread_ = std::thread(&ExchangeSimulator::tick_generation_loop, this);
    
    std::cout << "Exchange Simulator started on port " << port_ << std::endl;
}

void ExchangeSimulator::run() {
    struct epoll_event events[MAX_EVENTS]; // Buffer for returned events

    while (running_) {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100); //// BLOCKS until events arrive or 100ms timeout

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == server_fd_)
            { // Server socket is readable → new client connection
                handle_new_connection();
            }
            else
            {
                // Handle client data or disconnection
                if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                    handle_client_disconnect(events[i].data.fd);
                }
                else if (events[i].events & EPOLLIN) {
                    // Client has data to read (subscription message)
                    handle_client_data(events[i].data.fd);
                }
            }
        }
    }
}

void ExchangeSimulator::handle_new_connection() {
    struct sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    // Accept the new client connection
    int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        return;
    }
    
    // Set client socket non-blocking
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    // Set TCP_NODELAY (disable Nagle's algorithm for low latency)
    int nodelay = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    
    // Add client socket to epoll
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET; // Edge-triggered mode (notifies only on state changes, not continuous readiness)
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);
    
    client_fds_.push_back(client_fd);
    
    std::cout << "New client connected: " << client_fd << std::endl;
}

void ExchangeSimulator::handle_client_disconnect(int client_fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
    
    auto it = std::find(client_fds_.begin(), client_fds_.end(), client_fd);
    if (it != client_fds_.end()) {
        client_fds_.erase(it);
    }
    
    // Remove client subscriptions
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        client_subscriptions_.erase(client_fd);
    }
    
    std::cout << "Client disconnected: " << client_fd << std::endl;
}

void ExchangeSimulator::generate_tick(uint16_t symbol_id) {
    if (symbol_id >= num_symbols_) return;
    
    TickGenerator gen;
    auto& symbol = symbols_[symbol_id];
    
    // Update underlying price only every 100 ticks
    // This keeps price evolution realistic while maintaining high message throughput
    constexpr uint32_t PRICE_UPDATE_INTERVAL = 100;
    symbol.ticks_since_price_update++;
    if (symbol.ticks_since_price_update >= PRICE_UPDATE_INTERVAL) {
        // Calculate dt (time in seconds between price updates)
        // dt = ticks_between_updates / ticks_per_second_per_symbol
        // dt = PRICE_UPDATE_INTERVAL / (tick_rate / num_symbols)
        // dt = PRICE_UPDATE_INTERVAL * num_symbols / tick_rate
        uint32_t rate = tick_rate_.load();
        double dt = (rate > 0) ? (static_cast<double>(PRICE_UPDATE_INTERVAL) * num_symbols_) / rate : 0.1;
        
        symbol.current_price = gen.generate_next_price(
            symbol.current_price, symbol.drift, symbol.volatility, dt);
        symbol.ticks_since_price_update = 0;
    }
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    
    // Fault injection: 1% sequence gaps (skip sequence number)
    static std::random_device rd;
    static std::mt19937 gen_fault(rd());
    static std::uniform_int_distribution<> dis_gap(1, 100);
    
    if (fault_injection_enabled_ && dis_gap(gen_fault) <= 1) {
        symbol.seq_num += 2; // Skip one sequence number to create a gap
    }
    
    if (gen.should_generate_quote()) {
        // Generate quote
        QuoteMessage msg{};
        msg.header.msg_type = static_cast<uint16_t>(MessageType::QUOTE);
        msg.header.seq_num = ++symbol.seq_num;
        msg.header.timestamp = timestamp;
        msg.header.symbol_id = symbol_id;
        
        double spread = gen.generate_spread(symbol.current_price);
        msg.payload.bid_price = symbol.current_price - spread / 2.0;
        msg.payload.ask_price = symbol.current_price + spread / 2.0;
        msg.payload.bid_qty = gen.generate_volume();
        msg.payload.ask_qty = gen.generate_volume();
        
        msg.checksum = calculate_checksum(&msg, sizeof(msg) - 4);
        
        broadcast_message(&msg, sizeof(msg), symbol_id);
    } else {
        // Generate trade
        TradeMessage msg{};
        msg.header.msg_type = static_cast<uint16_t>(MessageType::TRADE);
        msg.header.seq_num = ++symbol.seq_num;
        msg.header.timestamp = timestamp;
        msg.header.symbol_id = symbol_id;
        
        msg.payload.price = symbol.current_price;
        msg.payload.quantity = gen.generate_volume();
        
        msg.checksum = calculate_checksum(&msg, sizeof(msg) - 4);
        
        broadcast_message(&msg, sizeof(msg), symbol_id);
    }
}

void ExchangeSimulator::broadcast_message(const void* data, size_t len, uint16_t symbol_id) {
    auto clients = client_fds_;
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1, 100);
    
    for (int fd : clients) {
        // Subscription-only mode: clients must subscribe to receive messages
        if (symbol_id != 0xFFFF) {
            std::lock_guard<std::mutex> lock(subscriptions_mutex_);
            auto it = client_subscriptions_.find(fd);
            
            // Client has no subscriptions or empty subscription list - skip
            if (it == client_subscriptions_.end() || it->second.empty()) {
                continue;
            }
            
            // Client has subscriptions - check if subscribed to this specific symbol
            if (it->second.find(symbol_id) == it->second.end()) {
                continue; // Not subscribed to this symbol, skip
            }
        }
        
        const uint8_t* send_ptr = static_cast<const uint8_t*>(data);
        
        // Fault injection: 5% packet fragmentation (send in two parts)
        if (fault_injection_enabled_ && dis(gen) <= 5) {
            size_t first_part = len / 2;
            ssize_t sent1 = send(fd, send_ptr, first_part, MSG_NOSIGNAL | MSG_DONTWAIT);
            if (sent1 > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                send(fd, send_ptr + first_part, len - first_part, MSG_NOSIGNAL | MSG_DONTWAIT);
            }
            continue;
        }
        
        ssize_t sent = send(fd, data, len, MSG_NOSIGNAL | MSG_DONTWAIT);
        
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Send buffer full - slow consumer detected
                std::cerr << "Slow consumer detected on fd " << fd << std::endl;
                // Skip this client to avoid blocking others
                continue;
            } else if (errno == EPIPE || errno == ECONNRESET) {
                // Client disconnected
                handle_client_disconnect(fd);
            }
        }
    }
}

void ExchangeSimulator::tick_generation_loop() {
    using namespace std::chrono;
    
    while (running_) {
        auto start = steady_clock::now();
        
        uint32_t rate = tick_rate_.load();
        if (rate == 0) {
            // Efficient wait using condition variable instead of polling
            std::unique_lock<std::mutex> lock(tick_rate_mutex_);
            tick_rate_cv_.wait(lock, [this] { 
                return !running_ || tick_rate_.load() > 0; 
            });
            continue;
        }
        
        // Generate ticks for all symbols
        // Message rate: 100000 / 100 symbols = 1000 messages per symbol per second
        // Price update rate: Every 100 ticks = 10 price updates per second
        // This separates high-frequency messaging from realistic price evolution
        size_t ticks_per_symbol = rate / num_symbols_;
        if (ticks_per_symbol == 0) ticks_per_symbol = 1;
        
        for (uint16_t i = 0; i < num_symbols_; ++i) {
            for (size_t j = 0; j < ticks_per_symbol; ++j) {
                generate_tick(i);
            }
        }
        
        // Sleep to maintain tick rate
        auto elapsed = steady_clock::now() - start;
        auto sleep_time = milliseconds(1000) - 
                         duration_cast<milliseconds>(elapsed);
        
        if (sleep_time.count() > 0) {
            std::this_thread::sleep_for(sleep_time);
        }
    }
}

void ExchangeSimulator::set_tick_rate(uint32_t ticks_per_second) {
    uint32_t old_rate = tick_rate_.exchange(ticks_per_second);
    
    // If changing from 0 to non-zero, wake up the tick thread
    if (old_rate == 0 && ticks_per_second > 0) {
        tick_rate_cv_.notify_one();
    }
}

void ExchangeSimulator::enable_fault_injection(bool enable) {
    fault_injection_enabled_ = enable;
}

void ExchangeSimulator::stop() {
    running_ = false;
    
    // Wake up tick thread if it's waiting on condition variable
    tick_rate_cv_.notify_one();
    
    if (tick_thread_.joinable()) {
        tick_thread_.join();
    }
    
    for (int fd : client_fds_) {
        close(fd);
    }
    client_fds_.clear();
    
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
    
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
}

void ExchangeSimulator::handle_client_data(int client_fd) {
    uint8_t buffer[1024];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
    
    if (bytes_read <= 0) {
        if (bytes_read == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            // Connection closed or error
            handle_client_disconnect(client_fd);
        }
        return;
    }
    
    // Check if this is a subscription message (starts with 0xFF)
    if (bytes_read >= 3 && buffer[0] == 0xFF) {
        handle_subscription_message(client_fd, buffer, bytes_read);
    }
}

void ExchangeSimulator::handle_subscription_message(int client_fd, const uint8_t* data, size_t len) {
    if (len < 3) {
        std::cerr << "Invalid subscription message: too short" << std::endl;
        return;
    }
    
    // Parse subscription header
    uint8_t command = data[0];
    uint16_t count = data[1] | (data[2] << 8);
    
    if (command != 0xFF) {
        std::cerr << "Invalid subscription command: " << (int)command << std::endl;
        return;
    }
    
    size_t expected_len = 3 + (count * 2);
    if (len < expected_len) {
        std::cerr << "Invalid subscription message: expected " << expected_len 
                  << " bytes, got " << len << std::endl;
        return;
    }
    
    // Parse symbol IDs
    std::unordered_set<uint16_t> symbol_ids;
    for (uint16_t i = 0; i < count; ++i) {
        size_t offset = 3 + (i * 2);
        uint16_t symbol_id = data[offset] | (data[offset + 1] << 8);
        
        // Validate symbol ID
        if (symbol_id < num_symbols_) {
            symbol_ids.insert(symbol_id);
        } else {
            std::cerr << "Invalid symbol ID in subscription: " << symbol_id 
                      << " (max=" << num_symbols_ << ")" << std::endl;
        }
    }
    
    std::cout << "Client " << client_fd << " subscribed to " << symbol_ids.size() 
              << " symbols" << std::endl;
    
    // Update client subscriptions
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        client_subscriptions_[client_fd] = std::move(symbol_ids);
    }
}

#ifdef TESTING
bool ExchangeSimulator::is_client_subscribed(int client_fd, uint16_t symbol_id) const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    auto it = client_subscriptions_.find(client_fd);
    if (it == client_subscriptions_.end()) {
        return false;
    }
    return it->second.find(symbol_id) != it->second.end();
}

size_t ExchangeSimulator::get_client_subscription_count(int client_fd) const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    auto it = client_subscriptions_.find(client_fd);
    if (it == client_subscriptions_.end()) {
        return 0;
    }
    return it->second.size();
}
#endif

} // namespace mdfh
