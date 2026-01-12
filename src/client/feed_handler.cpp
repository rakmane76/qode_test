#include "client/feed_handler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

namespace mdfh {

FeedHandler::FeedHandler(const std::string& host, uint16_t port, size_t num_symbols)
    : host_(host),
      port_(port),
      num_symbols_(num_symbols),
      running_(false),
      messages_received_(0),
      bytes_received_(0) {
    
    socket_ = std::make_unique<MarketDataSocket>();
    parser_ = std::make_unique<BinaryParser>();
    cache_ = std::make_unique<SymbolCache>(num_symbols);
    latency_tracker_ = std::make_unique<LatencyTracker>();
    
    // Initialize symbol names with default names
    symbol_names_.resize(num_symbols);
    for (size_t i = 0; i < num_symbols; ++i) {
        symbol_names_[i] = "SYM" + std::to_string(i);
    }
    
    // Set up generic message handler (low-latency mode)
    parser_->set_generic_handler([this](const auto& msg) {
        this->handle_message(msg);
    });
}

FeedHandler::~FeedHandler() {
    stop();
}

bool FeedHandler::connect(const std::string& host, uint16_t port) {
    if (!socket_->connect(host, port)) {
        std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
        return false;
    }
    std::cout << "Connected to " << host << ":" << port << std::endl;
    return true;
}

void FeedHandler::disconnect() {
    stop();
    socket_->disconnect();
}

bool FeedHandler::start() {
    if (!socket_->is_connected()) {
        if (!socket_->connect(host_, port_)) {
            std::cerr << "Failed to connect to " << host_ << ":" << port_ << std::endl;
            return false;
        }
    }
    
    std::cout << "Connected to " << host_ << ":" << port_ << std::endl;
    
    running_ = true;
    receiver_thread_ = std::thread(&FeedHandler::receiver_loop, this);
    
    return true;
}

void FeedHandler::stop() {
    running_ = false;
    
    if (receiver_thread_.joinable()) {
        receiver_thread_.join();
    }
    
    socket_->disconnect();
}

bool FeedHandler::subscribe(const std::vector<uint16_t>& symbol_ids) {
    return socket_->send_subscription(symbol_ids);
}

bool FeedHandler::load_symbols(const std::string& symbols_file) {
    std::ifstream file(symbols_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open symbols file: " << symbols_file << std::endl;
        return false;
    }
    
    std::string line;
    // Skip header line
    if (!std::getline(file, line)) {
        std::cerr << "Empty symbols file" << std::endl;
        return false;
    }
    
    size_t loaded_count = 0;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        uint16_t symbol_id;
        std::string symbol_name;
        
        // Read: symbol_id,symbol,price,volatility,drift
        // We only need symbol_id and symbol_name
        if ((iss >> symbol_id) && iss.ignore() &&
            std::getline(iss, symbol_name, ',')) {
            
            // Validate symbol_id
            if (symbol_id >= num_symbols_) {
                std::cerr << "Warning: Symbol ID " << symbol_id 
                          << " exceeds max symbols " << num_symbols_ 
                          << ", skipping" << std::endl;
                continue;
            }
            
            symbol_names_[symbol_id] = symbol_name;
            loaded_count++;
        }
    }
    
    std::cout << "Loaded " << loaded_count << " symbol names from " << symbols_file << std::endl;
    return loaded_count > 0;
}

std::string FeedHandler::get_symbol_name(uint16_t symbol_id) const {
    if (symbol_id < symbol_names_.size()) {
        return symbol_names_[symbol_id];
    }
    return "UNKNOWN";
}

bool FeedHandler::is_connected() const {
    return socket_->is_connected();
}

void FeedHandler::receiver_loop() {
    constexpr size_t BUFFER_SIZE = 65536;
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    
    while (running_) {
        if (!socket_->is_connected()) {
            if (!reconnect()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
        }
        
        auto receive_start = std::chrono::steady_clock::now();
        
        ssize_t n = socket_->receive(buffer.data(), BUFFER_SIZE);
        
        if (n > 0) {
            auto receive_end = std::chrono::steady_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                receive_end - receive_start).count();
            
            latency_tracker_->record(latency);
            
            bytes_received_ += n;
            parser_->parse(buffer.data(), n);
        } else if (n == 0) {
            // Would block or connection closed
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        } else {
            // Error
            std::cerr << "Receive error, attempting reconnect..." << std::endl;
            socket_->disconnect();
        }
    }
}

bool FeedHandler::reconnect() {
    int backoff_ms = INITIAL_BACKOFF_MS;
    
    for (int attempt = 0; attempt < MAX_RECONNECT_ATTEMPTS; ++attempt) {
        std::cout << "Reconnection attempt " << (attempt + 1) << "..." << std::endl;
        
        if (socket_->connect(host_, port_)) {
            std::cout << "Reconnected successfully" << std::endl;
            return true;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
        
        // Exponential backoff
        backoff_ms = std::min(backoff_ms * 2, MAX_BACKOFF_MS);
    }
    
    std::cerr << "Failed to reconnect after " << MAX_RECONNECT_ATTEMPTS 
              << " attempts" << std::endl;
    return false;
}

FeedHandler::FeedHandlerStats FeedHandler::get_stats() const {
    FeedHandlerStats stats;
    stats.messages_received = messages_received_;
    stats.messages_parsed = parser_->get_messages_parsed();
    stats.bytes_received = bytes_received_;
    stats.sequence_gaps = parser_->get_sequence_gaps();
    stats.fragmented_messages = parser_->get_fragmented_count();
    stats.checksum_errors = parser_->get_checksum_errors();
    return stats;
}

} // namespace mdfh
