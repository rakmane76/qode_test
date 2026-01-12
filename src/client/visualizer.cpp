#include "client/visualizer.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <unistd.h>

namespace mdfh {

// ANSI color codes
const char* COLOR_RESET = "\033[0m";
const char* COLOR_GREEN = "\033[32m";
const char* COLOR_RED = "\033[31m";
const char* COLOR_YELLOW = "\033[33m";
const char* COLOR_CYAN = "\033[36m";
const char* COLOR_BOLD = "\033[1m";

Visualizer::Visualizer(const SymbolCache& cache, size_t num_symbols)
    : cache_(cache),
      num_symbols_(num_symbols),
      running_(false),
      total_messages_(0),
      message_rate_(0),
      port_(0),
      connected_(false),
      start_time_(std::chrono::steady_clock::now()) {
}

Visualizer::~Visualizer() {
    stop();
}

void Visualizer::start() {
    running_ = true;
    start_time_ = std::chrono::steady_clock::now();
    display_thread_ = std::thread(&Visualizer::display_loop, this);
}

void Visualizer::stop() {
    running_ = false;
    
    if (display_thread_.joinable()) {
        display_thread_.join();
    }
    
    // Clear screen and show cursor
    std::cout << "\033[2J\033[H\033[?25h";
}

void Visualizer::update_stats(uint64_t messages, uint64_t msg_rate,
                               const LatencyStats& latency) {
    total_messages_ = messages;
    message_rate_ = msg_rate;
    current_latency_ = latency;
}

void Visualizer::set_connection_info(const std::string& host, uint16_t port, 
                                      bool connected) {
    host_ = host;
    port_ = port;
    connected_ = connected;
}

void Visualizer::set_symbol_names(const std::vector<std::string>& names) {
    symbol_names_ = names;
}

void Visualizer::display_loop() {
    // Hide cursor
    std::cout << "\033[?25l";
    
    while (running_) {
        clear_screen();
        draw_header();
        
        auto symbols = get_top_symbols();
        draw_symbol_table(symbols);
        
        draw_statistics();
        
        std::cout << std::flush;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_INTERVAL_MS));
    }
}

void Visualizer::clear_screen() {
    // Move cursor to home and clear screen
    std::cout << "\033[2J\033[H";
}

void Visualizer::draw_header() {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        now - start_time_);
    
    std::cout << COLOR_BOLD << COLOR_CYAN 
              << "=== NSE Market Data Feed Handler ===" 
              << COLOR_RESET << "\n";
    
    std::cout << "Connected to: " << host_ << ":" << port_;
    if (connected_) {
        std::cout << " " << COLOR_GREEN << "[CONNECTED]" << COLOR_RESET;
    } else {
        std::cout << " " << COLOR_RED << "[DISCONNECTED]" << COLOR_RESET;
    }
    std::cout << "\n";
    
    std::cout << "Uptime: " << format_duration(uptime) 
              << " | Messages: " << total_messages_.load()
              << " | Rate: " << message_rate_.load() << " msg/s\n";
    std::cout << "\n";
}

void Visualizer::draw_symbol_table(const std::vector<SymbolDisplay>& symbols) {
    std::cout << COLOR_BOLD;
    std::cout << std::left << std::setw(10) << "Symbol"
              << std::right << std::setw(12) << "Bid"
              << std::setw(12) << "Ask"
              << std::setw(12) << "LTP"
              << std::setw(15) << "Volume"
              << std::setw(10) << "Chg%"
              << std::setw(12) << "Updates"
              << COLOR_RESET << "\n";
    
    std::cout << std::string(83, '-') << "\n";
    
    for (const auto& sym : symbols) {
        std::cout << std::left << std::setw(10) << sym.symbol_name;
        std::cout << std::right << std::setw(12) << format_price(sym.bid);
        std::cout << std::setw(12) << format_price(sym.ask);
        std::cout << std::setw(12) << format_price(sym.ltp);
        std::cout << std::setw(15) << format_volume(sym.volume);
        std::cout << std::setw(10) << format_change(sym.change_pct);
        std::cout << std::setw(12) << sym.update_count;
        std::cout << "\n";
    }
    
    std::cout << "\n";
}

void Visualizer::draw_statistics() {
    std::cout << COLOR_BOLD << "Statistics:" << COLOR_RESET << "\n";
    std::cout << "Parser Throughput: " << message_rate_.load() << " msg/s\n";
    std::cout << "End-to-End Latency: "
              << "p50=" << (current_latency_.p50 / 1000) << "μs "
              << "p99=" << (current_latency_.p99 / 1000) << "μs "
              << "p999=" << (current_latency_.p999 / 1000) << "μs\n";
    std::cout << "\n";
    std::cout << COLOR_YELLOW << "Press Ctrl+C to quit" << COLOR_RESET << "\n";
}

std::vector<SymbolDisplay> Visualizer::get_top_symbols() {
    std::vector<SymbolDisplay> symbols;
    symbols.reserve(num_symbols_);
    
    for (size_t i = 0; i < num_symbols_; ++i) {
        auto snapshot = cache_.get_snapshot(i);
        
        SymbolDisplay disp;
        disp.symbol_id = i;
        // Use actual symbol name if available, otherwise fallback to generic name
        if (i < symbol_names_.size() && !symbol_names_[i].empty()) {
            disp.symbol_name = symbol_names_[i];
        } else {
            disp.symbol_name = "SYM" + std::to_string(i);
        }
        disp.bid = snapshot.best_bid;
        disp.ask = snapshot.best_ask;
        disp.ltp = snapshot.last_traded_price;
        disp.volume = snapshot.last_traded_quantity;
        disp.change_pct = 0.0; // TODO: Calculate from historical data
        disp.update_count = snapshot.update_count;
        
        symbols.push_back(disp);
    }
    
    // Sort by update count (most active first)
    std::sort(symbols.begin(), symbols.end(),
              [](const SymbolDisplay& a, const SymbolDisplay& b) {
                  return a.update_count > b.update_count;
              });
    
    // Return top N
    if (symbols.size() > TOP_N_SYMBOLS) {
        symbols.resize(TOP_N_SYMBOLS);
    }
    
    return symbols;
}

std::string Visualizer::format_price(double price) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << price;
    return oss.str();
}

std::string Visualizer::format_volume(uint64_t volume) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0) << volume;
    return oss.str();
}

std::string Visualizer::format_change(double change_pct) {
    std::ostringstream oss;
    
    const char* color = get_color_for_change(change_pct);
    oss << color;
    
    if (change_pct >= 0) {
        oss << "+";
    }
    
    oss << std::fixed << std::setprecision(2) << change_pct << "%";
    oss << COLOR_RESET;
    
    return oss.str();
}

std::string Visualizer::format_duration(std::chrono::seconds duration) {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    duration -= hours;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
    duration -= minutes;
    auto seconds = duration;
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours.count() << ":"
        << std::setw(2) << minutes.count() << ":"
        << std::setw(2) << seconds.count();
    
    return oss.str();
}

const char* Visualizer::get_color_for_change(double change_pct) {
    if (change_pct > 0) {
        return COLOR_GREEN;
    } else if (change_pct < 0) {
        return COLOR_RED;
    } else {
        return COLOR_RESET;
    }
}

} // namespace mdfh
