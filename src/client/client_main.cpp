#include "client/feed_handler.h"
#include "client/visualizer.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>

std::atomic<bool> g_running(true);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string host = "127.0.0.1";
    uint16_t port = 9876;
    size_t num_symbols = 100;
    
    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = static_cast<uint16_t>(std::atoi(argv[2]));
    }
    if (argc > 3) {
        num_symbols = static_cast<size_t>(std::atoi(argv[3]));
    }
    
    std::cout << "Starting Feed Handler..." << std::endl;
    std::cout << "Connecting to: " << host << ":" << port << std::endl;
    std::cout << "Number of symbols: " << num_symbols << std::endl;
    std::cout << std::endl;
    
    // Install signal handler
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        // Create feed handler
        mdfh::FeedHandler handler(host, port, num_symbols);
        
        // Load symbol names from CSV file
        if (!handler.load_symbols("config/symbols.csv")) {
            std::cerr << "Warning: Failed to load symbol names, using defaults" << std::endl;
        }
        
        if (!handler.start()) {
            std::cerr << "Failed to start feed handler" << std::endl;
            return 1;
        }
        
        // Subscribe to all symbols
        std::vector<uint16_t> symbol_ids;
        for (uint16_t i = 0; i < num_symbols; ++i) {
            symbol_ids.push_back(i);
        }
        
        if (!handler.subscribe(symbol_ids)) {
            std::cerr << "Failed to send subscription" << std::endl;
        }
        
        // Start visualizer
        mdfh::Visualizer viz(handler.get_cache(), num_symbols);
        viz.set_connection_info(host, port, handler.is_connected());
        
        // Pass symbol names to visualizer
        std::vector<std::string> symbol_names;
        for (uint16_t i = 0; i < num_symbols; ++i) {
            symbol_names.push_back(handler.get_symbol_name(i));
        }
        viz.set_symbol_names(symbol_names);
        
        viz.start();
        
        // Update statistics periodically
        auto last_messages = handler.get_messages_received();
        auto last_time = std::chrono::steady_clock::now();
        
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            auto now = std::chrono::steady_clock::now();
            auto current_messages = handler.get_messages_received();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_time).count();
            
            if (elapsed > 0) {
                uint64_t msg_rate = ((current_messages - last_messages) * 1000) / elapsed;
                auto latency = handler.get_latency_stats();
                
                viz.update_stats(current_messages, msg_rate, latency);
                viz.set_connection_info(host, port, handler.is_connected());
            }
            
            last_messages = current_messages;
            last_time = now;
        }
        
        std::cout << "\nShutting down..." << std::endl;
        viz.stop();
        handler.stop();
        
        // Print final statistics
        std::cout << "\nFinal Statistics:" << std::endl;
        std::cout << "Total messages received: " << handler.get_messages_received() << std::endl;
        std::cout << "Total bytes received: " << handler.get_bytes_received() << std::endl;
        
        auto stats = handler.get_latency_stats();
        std::cout << "Latency - p50: " << (stats.p50/1000) << "μs, "
                  << "p99: " << (stats.p99/1000) << "μs, "
                  << "p999: " << (stats.p999/1000) << "μs" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
