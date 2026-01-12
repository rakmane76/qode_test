#include "server/exchange_simulator.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <memory>

std::atomic<bool> g_running(true);
std::unique_ptr<mdfh::ExchangeSimulator> g_simulator;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived signal, shutting down..." << std::endl;
        g_running = false;
        if (g_simulator) {
            g_simulator->stop();
        }
    }
}

int main(int argc, char* argv[]) {
    // Install signal handler
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        // Parse command line arguments: port [num_symbols]
        uint16_t port = 9876;  // default
        size_t num_symbols = 100;  // default
        
        if (argc >= 2) {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        }
        if (argc >= 3) {
            num_symbols = static_cast<size_t>(std::stoi(argv[2]));
        }
        
        std::cout << "Starting Exchange Simulator..." << std::endl;
        if (argc >= 2) {
            std::cout << "Using command line parameters" << std::endl;
        } else {
            std::cout << "Using default parameters (will be overridden by config/server.conf if present)" << std::endl;
        }
        std::cout << std::endl;
        
        g_simulator = std::make_unique<mdfh::ExchangeSimulator>(port, num_symbols);
        
        g_simulator->start();
        
        std::cout << "\nExchange Simulator running. Press Ctrl+C to stop." << std::endl;
        
        // Run event loop - simulator.run() blocks until running_ is false
        g_simulator->run();
        
        std::cout << "\nShutting down..." << std::endl;
        g_simulator->stop();
        g_simulator.reset();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
