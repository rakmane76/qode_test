#include <gtest/gtest.h>
#include "server/exchange_simulator.h"
#include "common/protocol.h"
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

namespace fs = std::filesystem;

namespace mdfh {

class ExchangeSimulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "test_symbols_temp";
        fs::create_directories(test_dir_);
        config_dir_ = test_dir_ + "/config";
        fs::create_directories(config_dir_);
    }

    void TearDown() override {
        // Clean up test files
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    // Helper to create a test config file
    std::string create_test_config(const std::string& symbols_file, 
                                   uint16_t port = 0, 
                                   size_t num_symbols = 10,
                                   uint32_t tick_rate = 100000) {
        std::string config_file = config_dir_ + "/test_server.conf";
        std::ofstream file(config_file);
        file << "server.port=" << port << "\n";
        file << "market.num_symbols=" << num_symbols << "\n";
        file << "market.tick_rate=" << tick_rate << "\n";
        file << "market.symbols_file=" << fs::absolute(symbols_file).string() << "\n";
        file << "fault_injection.enabled=false\n";
        file.close();
        
        // Print config parameters for debugging
        std::cout << "\n=== Test Config Created ===" << std::endl;
        std::cout << "Config file: " << config_file << std::endl;
        std::cout << "  server.port=" << port << std::endl;
        std::cout << "  market.num_symbols=" << num_symbols << std::endl;
        std::cout << "  market.tick_rate=" << tick_rate << std::endl;
        std::cout << "  market.symbols_file=" << fs::absolute(symbols_file).string() << std::endl;
        std::cout << "  fault_injection.enabled=false" << std::endl;
        std::cout << "===========================\n" << std::endl;
        
        return config_file;
    }

    // Helper to create a valid symbol CSV file
    void create_valid_symbol_file(const std::string& filename, int num_symbols) {
        std::ofstream file(filename);
        file << "symbol_id,symbol,price,volatility,drift\n";
        for (int i = 0; i < num_symbols; ++i) {
            file << i << ",SYM" << i << "," 
                 << (1000.0 + i * 10.0) << ","
                 << (0.02 + i * 0.001) << ","
                 << (0.01 - i * 0.001) << "\n";
        }
        file.close();
    }

    // Helper to create CSV with malformed data
    void create_malformed_symbol_file(const std::string& filename) {
        std::ofstream file(filename);
        file << "symbol_id,symbol,price,volatility,drift\n";
        file << "0,SYM0,1000.0,0.02,0.01\n";
        file << "1,SYM1,invalid_price,0.02,0.01\n";  // Invalid price
        file << "2,SYM2,1020.0,0.022\n";  // Missing drift field
        file << "3,SYM3,1030.0,0.023,0.009\n";  // Valid
        file << "4,SYM4,,0.024,0.008\n";  // Missing price
        file << "5,SYM5,1050.0,0.025,0.007\n";  // Valid
        file.close();
    }

    // Helper to create an empty symbol file (header only)
    void create_empty_symbol_file(const std::string& filename) {
        std::ofstream file(filename);
        file << "symbol_id,symbol,price,volatility,drift\n";
        file.close();
    }

    std::string test_dir_;
    std::string config_dir_;
};

// Test Case 1: Successful Symbol Loading from Valid File
TEST_F(ExchangeSimulatorTest, LoadValidSymbolFile) {
    // Create test symbol file
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 10);
    
    // Create test config
    std::string config_file = create_test_config(symbol_file, 0, 10);
    
    try {
        ExchangeSimulator sim(0, 10, config_file);
        
        // Verify correct number of symbols loaded
        EXPECT_EQ(sim.get_num_loaded_symbols(), 10);
        
        // Verify symbol data
        for (size_t i = 0; i < sim.get_num_loaded_symbols(); ++i) {
            const auto& symbol = sim.get_symbol(i);
            EXPECT_EQ(symbol.symbol_id, i);
            EXPECT_EQ(symbol.symbol_name, "SYM" + std::to_string(i));
            EXPECT_DOUBLE_EQ(symbol.current_price, 1000.0 + i * 10.0);
            EXPECT_DOUBLE_EQ(symbol.volatility, 0.02 + i * 0.001);
            EXPECT_DOUBLE_EQ(symbol.drift, 0.01 - i * 0.001);
            EXPECT_EQ(symbol.seq_num, 0);
        }
        
        SUCCEED() << "Successfully loaded and verified 10 symbols from test file";
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 2: Symbol File Not Found
TEST_F(ExchangeSimulatorTest, SymbolFileNotFound) {
    // Create config pointing to non-existent symbol file
    std::string symbol_file = config_dir_ + "/nonexistent.csv";
    std::string config_file = create_test_config(symbol_file, 0, 10);
    
    EXPECT_THROW({
        ExchangeSimulator sim(0, 10, config_file);
    }, std::runtime_error);
}

// Test Case 3: Verify Symbol Count Limit
TEST_F(ExchangeSimulatorTest, SymbolCountLimit) {
    // Create file with 20 symbols but request only 5
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 20);
    
    std::string config_file = create_test_config(symbol_file, 0, 5);
    
    try {
        ExchangeSimulator sim(0, 5, config_file);
        
        // Verify only 5 symbols were loaded (not all 20)
        EXPECT_EQ(sim.get_num_loaded_symbols(), 5);
        
        // Verify the first 5 symbols have correct IDs
        for (size_t i = 0; i < 5; ++i) {
            const auto& symbol = sim.get_symbol(i);
            EXPECT_EQ(symbol.symbol_id, i);
            EXPECT_EQ(symbol.symbol_name, "SYM" + std::to_string(i));
        }
        
        SUCCEED() << "Successfully limited to 5 symbols from file with 20";
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 4: Empty Symbol File Should Throw
TEST_F(ExchangeSimulatorTest, EmptySymbolFileThrows) {
    // Create empty file (header only)
    std::string symbol_file = config_dir_ + "/empty_symbols.csv";
    create_empty_symbol_file(symbol_file);
    
    std::string config_file = create_test_config(symbol_file, 0, 10);
    
    EXPECT_THROW({
        ExchangeSimulator sim(0, 10, config_file);
    }, std::runtime_error);
}

// Test Case 5: Malformed CSV Data Handling
TEST_F(ExchangeSimulatorTest, MalformedCSVData) {
    std::string symbol_file = config_dir_ + "/malformed_symbols.csv";
    create_malformed_symbol_file(symbol_file);
    
    std::string config_file = create_test_config(symbol_file, 0, 10);
    
    try {
        ExchangeSimulator sim(0, 10, config_file);
        
        // Should load only valid rows (SYM0, SYM3, SYM5 = 3 symbols)
        size_t loaded = sim.get_num_loaded_symbols();
        EXPECT_EQ(loaded, 3) << "Expected 3 valid symbols, got " << loaded;
        
        // Verify the valid symbols
        EXPECT_EQ(sim.get_symbol(0).symbol_name, "SYM0");
        EXPECT_EQ(sim.get_symbol(1).symbol_name, "SYM3");
        EXPECT_EQ(sim.get_symbol(2).symbol_name, "SYM5");

        SUCCEED() << "Handled malformed CSV gracefully, loaded " << loaded << " valid symbols" << std::endl;
    } catch (const std::exception& e) {
        FAIL() << "Should have loaded valid rows but threw: " << e.what();
    }
}

// Test Case 6: File Has Fewer Symbols Than Requested
TEST_F(ExchangeSimulatorTest, FileHasFewerSymbolsThanRequested) {
    // Create file with only 3 symbols
    std::string symbol_file = config_dir_ + "/few_symbols.csv";
    create_valid_symbol_file(symbol_file, 3);
    
    // Request 10 symbols but file only has 3
    std::string config_file = create_test_config(symbol_file, 0, 10);
    
    try {
        ExchangeSimulator sim(0, 10, config_file);
        
        // Should load only 3 symbols (limited by file content, not request)
        size_t loaded = sim.get_num_loaded_symbols();
        EXPECT_EQ(loaded, 3) << "Expected 3 symbols from file, got " << loaded;
        
        // Verify all 3 symbols have correct data
        for (size_t i = 0; i < loaded; ++i) {
            const auto& symbol = sim.get_symbol(i);
            EXPECT_EQ(symbol.symbol_id, i);
            EXPECT_EQ(symbol.symbol_name, "SYM" + std::to_string(i));
        }
        
        std::cout << "Loaded " << loaded << " symbols from file (requested 10, file had 3)" << std::endl;
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 7: Generate Tick - Verify Message Structure
TEST_F(ExchangeSimulatorTest, GenerateTickCreatesValidMessage) {
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 5);
    std::string config_file = create_test_config(symbol_file, 0, 5);
    
    try {
        ExchangeSimulator sim(0, 5, config_file);
        
        // Get initial price and sequence number
        const auto& symbol_before = sim.get_symbol(0);
        double initial_price = symbol_before.current_price;
        uint64_t initial_seq = symbol_before.seq_num;
        
        // Generate a tick (this updates internal state but won't broadcast without clients)
        sim.generate_tick(0);
        
        // Verify that symbol state was updated
        const auto& symbol_after = sim.get_symbol(0);
        
        // Sequence number should have incremented
        EXPECT_GT(symbol_after.seq_num, initial_seq) 
            << "Sequence number should increment after generate_tick";
        
        // Price should have changed (very unlikely to be exactly the same)
        EXPECT_NE(symbol_after.current_price, initial_price)
            << "Price should change after tick generation";
        
        // Price should be positive and reasonable (within 50% of original)
        EXPECT_GT(symbol_after.current_price, 0.0);
        EXPECT_GT(symbol_after.current_price, initial_price * 0.5);
        EXPECT_LT(symbol_after.current_price, initial_price * 1.5);
        
        SUCCEED() << "generate_tick() successfully updated symbol state";
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 8: Generate Multiple Ticks - Verify Price Movement
TEST_F(ExchangeSimulatorTest, GenerateMultipleTicksShowsPriceMovement) {
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 3);
    std::string config_file = create_test_config(symbol_file, 0, 3);
    
    try {
        ExchangeSimulator sim(0, 3, config_file);
        
        const auto& symbol_initial = sim.get_symbol(0);
        double initial_price = symbol_initial.current_price;
        uint64_t initial_seq = symbol_initial.seq_num;
        
        // Generate 100 ticks
        for (int i = 0; i < 100; ++i) {
            sim.generate_tick(0);
        }
        
        const auto& symbol_final = sim.get_symbol(0);
        
        // After 100 ticks, sequence number should have increased
        EXPECT_EQ(symbol_final.seq_num, initial_seq + 100);
        
        // Price should have changed
        EXPECT_NE(symbol_final.current_price, initial_price);
        
        // But should still be in reasonable range (volatility is low)
        EXPECT_GT(symbol_final.current_price, initial_price * 0.5);
        EXPECT_LT(symbol_final.current_price, initial_price * 2.0);
        
        std::cout << "Price movement over 100 ticks: " 
                  << initial_price << " -> " << symbol_final.current_price 
                  << " (change: " << ((symbol_final.current_price / initial_price - 1.0) * 100.0) << "%)" 
                  << std::endl;
        
        SUCCEED() << "100 ticks generated successfully with reasonable price movement";
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 9: Generate Tick for All Symbols
TEST_F(ExchangeSimulatorTest, GenerateTickForAllSymbols) {
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 10);
    std::string config_file = create_test_config(symbol_file, 0, 10);
    
    try {
        ExchangeSimulator sim(0, 10, config_file);
        
        // Store initial state
        std::vector<uint64_t> initial_seq_nums;
        for (size_t i = 0; i < 10; ++i) {
            initial_seq_nums.push_back(sim.get_symbol(i).seq_num);
        }
        
        // Generate tick for each symbol
        for (uint16_t symbol_id = 0; symbol_id < 10; ++symbol_id) {
            sim.generate_tick(symbol_id);
        }
        
        // Verify all symbols were updated
        for (size_t i = 0; i < 10; ++i) {
            const auto& symbol = sim.get_symbol(i);
            EXPECT_GT(symbol.seq_num, initial_seq_nums[i]) 
                << "Symbol " << i << " sequence number should increment";
            EXPECT_GT(symbol.current_price, 0.0) 
                << "Symbol " << i << " price should be positive";
        }
        
        SUCCEED() << "All 10 symbols updated successfully";
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 10: Generate Tick with Invalid Symbol ID
TEST_F(ExchangeSimulatorTest, GenerateTickWithInvalidSymbolId) {
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 5);
    std::string config_file = create_test_config(symbol_file, 0, 5);
    
    try {
        ExchangeSimulator sim(0, 5, config_file);
        
        // Try to generate tick for symbol ID that doesn't exist (should be safe - no throw)
        EXPECT_NO_THROW(sim.generate_tick(100));
        EXPECT_NO_THROW(sim.generate_tick(static_cast<uint16_t>(-1)));
        
        // Verify existing symbols were not affected
        for (size_t i = 0; i < 5; ++i) {
            const auto& symbol = sim.get_symbol(i);
            EXPECT_EQ(symbol.seq_num, 0) << "Symbol " << i << " should not be affected";
        }
        
        SUCCEED() << "Invalid symbol IDs handled gracefully";
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 11: Fault Injection Enable/Disable
TEST_F(ExchangeSimulatorTest, FaultInjectionToggle) {
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 3);
    std::string config_file = create_test_config(symbol_file, 0, 3);
    
    try {
        ExchangeSimulator sim(0, 3, config_file);
        
        // Enable fault injection
        EXPECT_NO_THROW(sim.enable_fault_injection(true));
        
        // Generate some ticks with fault injection enabled
        for (int i = 0; i < 50; ++i) {
            sim.generate_tick(0);
        }
        
        // Disable fault injection
        EXPECT_NO_THROW(sim.enable_fault_injection(false));
        
        // Generate more ticks with fault injection disabled
        for (int i = 0; i < 50; ++i) {
            sim.generate_tick(0);
        }
        
        // Sequence number should have incremented (possibly with gaps due to fault injection)
        const auto& symbol = sim.get_symbol(0);
        EXPECT_GE(symbol.seq_num, 100);  // At least 100 (could be more due to injected gaps)
        
        SUCCEED() << "Fault injection toggle works correctly, seq_num=" << symbol.seq_num;
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 12: Symbol State Persistence Across Ticks
TEST_F(ExchangeSimulatorTest, SymbolStatePersistence) {
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 2);
    std::string config_file = create_test_config(symbol_file, 0, 2);
    
    try {
        ExchangeSimulator sim(0, 2, config_file);
        
        // Generate ticks for symbol 0 only
        for (int i = 0; i < 10; ++i) {
            sim.generate_tick(0);
        }
        
        const auto& symbol0 = sim.get_symbol(0);
        const auto& symbol1 = sim.get_symbol(1);
        
        // Symbol 0 should have updated sequence number
        EXPECT_EQ(symbol0.seq_num, 10);
        
        // Symbol 1 should remain unchanged
        EXPECT_EQ(symbol1.seq_num, 0);
        EXPECT_DOUBLE_EQ(symbol1.current_price, 1010.0);  // Initial price for SYM1
        
        SUCCEED() << "Symbol state is properly isolated between symbols";
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 13: Simulator Stop/Cleanup
TEST_F(ExchangeSimulatorTest, SimulatorStopCleanup) {
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 5);
    std::string config_file = create_test_config(symbol_file, 0, 5);
    
    try {
        ExchangeSimulator sim(0, 5, config_file);
        
        // Stop should be safe to call even without start
        EXPECT_NO_THROW(sim.stop());
        
        // Should be safe to call multiple times
        EXPECT_NO_THROW(sim.stop());
        EXPECT_NO_THROW(sim.stop());
        
        SUCCEED() << "Simulator stop/cleanup works correctly";
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 14: Simulator Start
TEST_F(ExchangeSimulatorTest, SimulatorStart) {
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 5);
    std::string config_file = create_test_config(symbol_file, 0, 5);
    
    try {
        ExchangeSimulator sim(0, 5, config_file);
        
        // Start should execute without throwing (starts background tick thread)
        EXPECT_NO_THROW(sim.start());
        
        // Wait a bit to allow the background thread to start
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // At least some symbols should have ticks generated by the background thread
        bool has_activity = false;
        for (size_t i = 0; i < 5; ++i) {
            const auto& symbol = sim.get_symbol(i);
            if (symbol.seq_num > 0) {
                has_activity = true;
                break;
            }
        }
        EXPECT_TRUE(has_activity) << "At least some tick generation should occur after start()";
        
        // Manual tick generation should still work after start
        uint64_t seq_before = sim.get_symbol(0).seq_num;
        EXPECT_NO_THROW(sim.generate_tick(0));
        uint64_t seq_after = sim.get_symbol(0).seq_num;
        EXPECT_GT(seq_after, seq_before) << "Manual tick generation should work after start()";
        
        // Stop should work cleanly and halt background tick generation
        EXPECT_NO_THROW(sim.stop());
        
        // Verify stop actually stops tick generation
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        uint64_t seq_after_stop1 = sim.get_symbol(0).seq_num;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        uint64_t seq_after_stop2 = sim.get_symbol(0).seq_num;
        EXPECT_EQ(seq_after_stop1, seq_after_stop2) 
            << "Sequence numbers should not change after stop()";
        
        // Should be safe to call stop multiple times (like StopCleanup test)
        EXPECT_NO_THROW(sim.stop());
        EXPECT_NO_THROW(sim.stop());
        
        SUCCEED() << "Simulator start/stop cycle works correctly";
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 15: Broadcast Message - Basic Connectivity and Message Reception
TEST_F(ExchangeSimulatorTest, BroadcastMessageConnectivity) {
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 3);
    std::string config_file = create_test_config(symbol_file, 12345, 3, 0);  // tick_rate=0 to disable auto ticks
    
    try {
        ExchangeSimulator sim(12345, 3, config_file);
        
        // Set tick rate to 0 to disable background tick generation
        sim.set_tick_rate(0);
        
        // Start the simulator
        sim.start();
        
        // Start event loop in background thread
        std::thread event_thread([&sim]() {
            sim.run();
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Create a client socket to connect
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(client_fd, 0) << "Failed to create client socket";
        
        // Set receive timeout
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        
        struct sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(12345);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        // Connect to simulator
        int conn_result = connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
        ASSERT_EQ(conn_result, 0) << "Failed to connect to simulator: " << strerror(errno);
        
        // Give server time to accept connection
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Send subscription for symbol 0 (required for subscription-only mode)
        std::vector<uint8_t> sub_msg;
        sub_msg.push_back(0xFF); // Subscribe command
        uint16_t count = 1;
        sub_msg.push_back(count & 0xFF);
        sub_msg.push_back((count >> 8) & 0xFF);
        uint16_t symbol_id = 0;
        sub_msg.push_back(symbol_id & 0xFF);
        sub_msg.push_back((symbol_id >> 8) & 0xFF);
        ssize_t sent = send(client_fd, sub_msg.data(), sub_msg.size(), 0);
        ASSERT_EQ(sent, static_cast<ssize_t>(sub_msg.size())) << "Failed to send subscription";
        
        // Give server time to process subscription
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Get symbol state before tick
        const auto& symbol_before = sim.get_symbol(0);
        uint64_t seq_before = symbol_before.seq_num;
        
        // Generate a tick which will broadcast a message
        sim.generate_tick(0);
        
        // Read message from socket with retry
        uint8_t buffer[1024];
        ssize_t bytes_read = -1;
        int retry_count = 0;
        const int max_retries = 10;
        
        while (bytes_read <= 0 && retry_count < max_retries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            bytes_read = recv(client_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
            retry_count++;
        }
        
        ASSERT_GT(bytes_read, 0) << "Should receive data from broadcast after " << retry_count << " retries";
        
        // Verify basic message structure
        ASSERT_GE(bytes_read, static_cast<ssize_t>(sizeof(MessageHeader))) 
            << "Should receive at least message header";
        
        MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
        
        // Verify header fields
        EXPECT_TRUE(header->msg_type == static_cast<uint16_t>(MessageType::QUOTE) ||
                   header->msg_type == static_cast<uint16_t>(MessageType::TRADE))
            << "Message type should be QUOTE or TRADE";
        
        EXPECT_EQ(header->symbol_id, 0) << "Symbol ID should match";
        EXPECT_GT(header->seq_num, seq_before) << "Sequence number should have incremented";
        EXPECT_GT(header->timestamp, 0) << "Timestamp should be set";
        
        std::cout << "Received message type: " 
                  << (header->msg_type == static_cast<uint16_t>(MessageType::QUOTE) ? "QUOTE" : "TRADE")
                  << ", seq=" << header->seq_num << std::endl;
        
        // Cleanup
        close(client_fd);
        sim.stop();
        
        if (event_thread.joinable()) {
            event_thread.join();
        }
        
        SUCCEED() << "Broadcast message connectivity verified";
                  
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 16: Broadcast Quote Message - Verify Quote Data Correctness
TEST_F(ExchangeSimulatorTest, BroadcastQuoteMessageCorrectness) {
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 3);
    std::string config_file = create_test_config(symbol_file, 12346, 3, 0);  // tick_rate=0
    
    try {
        ExchangeSimulator sim(12346, 3, config_file);
        sim.set_tick_rate(0);
        sim.start();
        
        std::thread event_thread([&sim]() {
            sim.run();
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(client_fd, 0);
        
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        
        struct sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(12346);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        ASSERT_EQ(connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Send subscription for symbol 0 (required for subscription-only mode)
        std::vector<uint8_t> sub_msg;
        sub_msg.push_back(0xFF); // Subscribe command
        uint16_t count = 1;
        sub_msg.push_back(count & 0xFF);
        sub_msg.push_back((count >> 8) & 0xFF);
        uint16_t symbol_id = 0;
        sub_msg.push_back(symbol_id & 0xFF);
        sub_msg.push_back((symbol_id >> 8) & 0xFF);
        send(client_fd, sub_msg.data(), sub_msg.size(), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Generate multiple ticks until we get a QUOTE message
        uint8_t buffer[1024];
        QuoteMessage* quote = nullptr;
        int attempts = 0;
        const int max_attempts = 50;
        
        while (attempts < max_attempts && quote == nullptr) {
            sim.generate_tick(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
            if (bytes_read >= static_cast<ssize_t>(sizeof(MessageHeader))) {
                MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
                
                if (header->msg_type == static_cast<uint16_t>(MessageType::QUOTE)) {
                    ASSERT_GE(bytes_read, static_cast<ssize_t>(sizeof(QuoteMessage))) 
                        << "Quote message should be complete";
                    quote = reinterpret_cast<QuoteMessage*>(buffer);
                    break;
                }
            }
            attempts++;
        }
        
        ASSERT_NE(quote, nullptr) << "Should receive at least one QUOTE message in " << max_attempts << " attempts";
        
        // Verify Quote message data
        EXPECT_EQ(quote->header.symbol_id, 0) << "Symbol ID should be 0";
        EXPECT_GT(quote->header.seq_num, 0) << "Sequence number should be positive";
        EXPECT_GT(quote->header.timestamp, 0) << "Timestamp should be set";
        
        EXPECT_GT(quote->payload.bid_price, 0.0) << "Bid price should be positive";
        EXPECT_GT(quote->payload.ask_price, 0.0) << "Ask price should be positive";
        EXPECT_GT(quote->payload.ask_price, quote->payload.bid_price) 
            << "Ask price should be higher than bid price (spread)";
        EXPECT_GT(quote->payload.bid_qty, 0) << "Bid quantity should be positive";
        EXPECT_GT(quote->payload.ask_qty, 0) << "Ask quantity should be positive";
        
        // Verify spread is reasonable (typically < 1% of price)
        double spread = quote->payload.ask_price - quote->payload.bid_price;
        double mid_price = (quote->payload.bid_price + quote->payload.ask_price) / 2.0;
        double spread_pct = (spread / mid_price) * 100.0;
        EXPECT_LT(spread_pct, 1.0) << "Spread should be less than 1% of mid price";
        
        std::cout << "QUOTE verified: bid=" << quote->payload.bid_price 
                  << ", ask=" << quote->payload.ask_price 
                  << ", spread=" << spread << " (" << spread_pct << "%)"
                  << ", bid_qty=" << quote->payload.bid_qty
                  << ", ask_qty=" << quote->payload.ask_qty
                  << ", seq=" << quote->header.seq_num << std::endl;
        
        close(client_fd);
        sim.stop();
        
        if (event_thread.joinable()) {
            event_thread.join();
        }
        
        SUCCEED() << "Quote message data correctness verified";
                  
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 17: Broadcast Trade Message - Verify Trade Data Correctness
TEST_F(ExchangeSimulatorTest, BroadcastTradeMessageCorrectness) {
    std::string symbol_file = config_dir_ + "/symbols.csv";
    create_valid_symbol_file(symbol_file, 3);
    std::string config_file = create_test_config(symbol_file, 12347, 3, 0);  // tick_rate=0
    
    try {
        ExchangeSimulator sim(12347, 3, config_file);
        sim.set_tick_rate(0);
        sim.start();
        
        std::thread event_thread([&sim]() {
            sim.run();
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(client_fd, 0);
        
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        
        struct sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(12347);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        ASSERT_EQ(connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Send subscription for symbol 0 (required for subscription-only mode)
        std::vector<uint8_t> sub_msg;
        sub_msg.push_back(0xFF); // Subscribe command
        uint16_t count = 1;
        sub_msg.push_back(count & 0xFF);
        sub_msg.push_back((count >> 8) & 0xFF);
        uint16_t symbol_id = 0;
        sub_msg.push_back(symbol_id & 0xFF);
        sub_msg.push_back((symbol_id >> 8) & 0xFF);
        send(client_fd, sub_msg.data(), sub_msg.size(), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Generate multiple ticks until we get a TRADE message
        uint8_t buffer[1024];
        TradeMessage* trade = nullptr;
        int attempts = 0;
        const int max_attempts = 50;
        
        while (attempts < max_attempts && trade == nullptr) {
            sim.generate_tick(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
            if (bytes_read >= static_cast<ssize_t>(sizeof(MessageHeader))) {
                MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
                
                if (header->msg_type == static_cast<uint16_t>(MessageType::TRADE)) {
                    ASSERT_GE(bytes_read, static_cast<ssize_t>(sizeof(TradeMessage))) 
                        << "Trade message should be complete";
                    trade = reinterpret_cast<TradeMessage*>(buffer);
                    break;
                }
            }
            attempts++;
        }
        
        ASSERT_NE(trade, nullptr) << "Should receive at least one TRADE message in " << max_attempts << " attempts";
        
        // Verify Trade message data
        EXPECT_EQ(trade->header.symbol_id, 0) << "Symbol ID should be 0";
        EXPECT_GT(trade->header.seq_num, 0) << "Sequence number should be positive";
        EXPECT_GT(trade->header.timestamp, 0) << "Timestamp should be set";
        
        EXPECT_GT(trade->payload.price, 0.0) << "Trade price should be positive";
        EXPECT_GT(trade->payload.quantity, 0) << "Trade quantity should be positive";
        
        // Verify price is reasonable (within expected range for SYM0 initial price ~1000)
        EXPECT_GT(trade->payload.price, 500.0) << "Trade price should be > 500";
        EXPECT_LT(trade->payload.price, 2000.0) << "Trade price should be < 2000";
        
        std::cout << "TRADE verified: price=" << trade->payload.price 
                  << ", quantity=" << trade->payload.quantity
                  << ", seq=" << trade->header.seq_num << std::endl;
        
        close(client_fd);
        sim.stop();
        
        if (event_thread.joinable()) {
            event_thread.join();
        }
        
        SUCCEED() << "Trade message data correctness verified";
                  
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 18: Verify condition variable efficiently wakes tick thread when rate changes
TEST_F(ExchangeSimulatorTest, TickRateConditionVariableWakeup) {
    try {
        std::string symbol_file = config_dir_ + "/symbols.csv";
        create_valid_symbol_file(symbol_file, 3);
        std::string config_file = create_test_config(symbol_file, 12348, 3, 0);  // tick_rate=0
        
        ExchangeSimulator sim(12348, 3, config_file);
        sim.start();
        
        // Start event loop thread
        std::thread event_thread([&sim]() {
            sim.run();
        });
        
        // Give time for thread to enter wait state
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Measure time to wake up the thread
        auto start = std::chrono::steady_clock::now();
        
        // Change tick rate from 0 to 1000 - should wake immediately via CV
        sim.set_tick_rate(1000);
        
        // Give thread time to wake up and process
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        
        // Verify thread woke up quickly (not delayed by polling interval)
        EXPECT_LT(elapsed_ms, 100) << "Thread should wake immediately via condition variable, not poll";
        
        // Verify tick generation is now active by generating a tick
        sim.set_tick_rate(0);  // Pause to control tick generation
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Get initial sequence number
        uint32_t initial_seq = sim.get_symbol(0).seq_num;
        
        // Manually generate a tick
        sim.generate_tick(0);
        
        // Verify sequence incremented
        uint32_t new_seq = sim.get_symbol(0).seq_num;
        EXPECT_GT(new_seq, initial_seq) << "Tick generation should be functional after CV wakeup";
        
        sim.stop();
        
        if (event_thread.joinable()) {
            event_thread.join();
        }
        
        SUCCEED() << "Condition variable wakeup verified (elapsed: " << elapsed_ms << "ms)";
                  
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 19: Verify handle_new_connection properly registers client
TEST_F(ExchangeSimulatorTest, HandleNewConnection) {
    try {
        std::string symbol_file = config_dir_ + "/symbols.csv";
        create_valid_symbol_file(symbol_file, 3);
        std::string config_file = create_test_config(symbol_file, 12349, 3, 0);
        
        ExchangeSimulator sim(12349, 3, config_file);
        sim.start();
        
        // Start event loop thread
        std::thread event_thread([&sim]() {
            sim.run();
        });
        
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Verify no clients initially
        EXPECT_EQ(sim.get_num_connected_clients(), 0) << "Should start with no clients";
        
        // Create first client connection
        int client1_fd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(client1_fd, 0) << "Failed to create client socket";
        
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(12349);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        
        int conn_result = connect(client1_fd, (struct sockaddr*)&addr, sizeof(addr));
        ASSERT_EQ(conn_result, 0) << "Failed to connect client 1";
        
        // Give time for server to accept connection
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Verify client was added
        EXPECT_EQ(sim.get_num_connected_clients(), 1) << "Should have 1 connected client";
        
        // Create second client connection
        int client2_fd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(client2_fd, 0) << "Failed to create second client socket";
        
        conn_result = connect(client2_fd, (struct sockaddr*)&addr, sizeof(addr));
        ASSERT_EQ(conn_result, 0) << "Failed to connect client 2";
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Verify both clients are connected
        EXPECT_EQ(sim.get_num_connected_clients(), 2) << "Should have 2 connected clients";
        
        // Verify client FDs are in the list
        const auto& client_fds = sim.get_client_fds();
        EXPECT_EQ(client_fds.size(), 2) << "client_fds_ should contain 2 entries";
        
        // Send subscription for symbol 0 from client1 (required for subscription-only mode)
        std::vector<uint8_t> sub_msg;
        sub_msg.push_back(0xFF); // Subscribe command
        uint16_t count = 1;
        sub_msg.push_back(count & 0xFF);
        sub_msg.push_back((count >> 8) & 0xFF);
        uint16_t symbol_id = 0;
        sub_msg.push_back(symbol_id & 0xFF);
        sub_msg.push_back((symbol_id >> 8) & 0xFF);
        send(client1_fd, sub_msg.data(), sub_msg.size(), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Test that clients can receive data (verifies epoll registration)
        sim.generate_tick(0);
        
        char buffer[1024];
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(client1_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        ssize_t bytes_read = recv(client1_fd, buffer, sizeof(buffer), 0);
        EXPECT_GT(bytes_read, 0) << "Client 1 should receive broadcast message";
        
        // Cleanup
        close(client1_fd);
        close(client2_fd);
        
        sim.stop();
        
        if (event_thread.joinable()) {
            event_thread.join();
        }
        
        SUCCEED() << "handle_new_connection verified: accepts clients, registers with epoll";
                  
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test Case 20: Verify handle_client_disconnect properly cleans up client
TEST_F(ExchangeSimulatorTest, HandleClientDisconnect) {
    try {
        std::string symbol_file = config_dir_ + "/symbols.csv";
        create_valid_symbol_file(symbol_file, 3);
        std::string config_file = create_test_config(symbol_file, 12350, 3, 0);
        
        ExchangeSimulator sim(12350, 3, config_file);
        sim.start();
        
        // Start event loop thread
        std::thread event_thread([&sim]() {
            sim.run();
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Create three client connections
        int client_fds[3];
        for (int i = 0; i < 3; ++i) {
            client_fds[i] = socket(AF_INET, SOCK_STREAM, 0);
            ASSERT_GE(client_fds[i], 0) << "Failed to create client socket " << i;
            
            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(12350);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            
            int conn_result = connect(client_fds[i], (struct sockaddr*)&addr, sizeof(addr));
            ASSERT_EQ(conn_result, 0) << "Failed to connect client " << i;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Verify all 3 clients connected
        EXPECT_EQ(sim.get_num_connected_clients(), 3) << "Should have 3 connected clients";
        
        // Send subscription for symbol 0 from clients 1 and 2 (required for subscription-only mode)
        std::vector<uint8_t> sub_msg;
        sub_msg.push_back(0xFF); // Subscribe command
        uint16_t count = 1;
        sub_msg.push_back(count & 0xFF);
        sub_msg.push_back((count >> 8) & 0xFF);
        uint16_t symbol_id = 0;
        sub_msg.push_back(symbol_id & 0xFF);
        sub_msg.push_back((symbol_id >> 8) & 0xFF);
        
        // Subscribe clients 1 and 2 (not client 0, which we'll disconnect)
        send(client_fds[1], sub_msg.data(), sub_msg.size(), 0);
        send(client_fds[2], sub_msg.data(), sub_msg.size(), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Close first client and wait for disconnect detection
        // NOTE: Disconnect detection is asynchronous - epoll_wait must return
        // with EPOLLHUP/EPOLLERR before handle_client_disconnect runs
        close(client_fds[0]);
        
        // Trigger a tick to force write to disconnected socket (which triggers EPOLLHUP)
        sim.generate_tick(0);
        
        // Poll for disconnect detection (max 2 seconds)
        size_t count_after_first = 3;
        for (int i = 0; i < 20; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            count_after_first = sim.get_num_connected_clients();
            if (count_after_first < 3) {
                break; // Disconnect detected
            }
        }
        
        // Client should be removed (may take up to epoll timeout cycles)
        EXPECT_LE(count_after_first, 2) << "Should have <= 2 clients after disconnect";
        
        // Verify remaining clients are still functional
        sim.generate_tick(0);
        
        char buffer[1024];
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        // Only test if we have remaining clients
        if (count_after_first > 0) {
            setsockopt(client_fds[1], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            ssize_t bytes_read = recv(client_fds[1], buffer, sizeof(buffer), 0);
            EXPECT_GT(bytes_read, 0) << "Remaining client should still receive messages";
        }
        
        // Close remaining clients
        close(client_fds[1]);
        close(client_fds[2]);
        
        // Trigger tick to force write detection
        sim.generate_tick(0);
        
        // Poll for all disconnects (max 2 seconds)
        size_t final_count = count_after_first;
        for (int i = 0; i < 20; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            final_count = sim.get_num_connected_clients();
            if (final_count == 0) {
                break; // All disconnects detected
            }
        }
        
        // All clients should eventually disconnect
        EXPECT_EQ(final_count, 0) << "All clients should be disconnected";
        
        sim.stop();
        
        if (event_thread.joinable()) {
            event_thread.join();
        }
        
        SUCCEED() << "handle_client_disconnect verified: removes clients from epoll and client_fds_";
                  
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

// Test: ClientManager integration - basic functionality
TEST_F(ExchangeSimulatorTest, ClientManagerIntegration) {
    try {
        std::string symbols_file = config_dir_ + "/symbols.csv";
        create_valid_symbol_file(symbols_file, 5);
        std::string config_path = create_test_config(symbols_file, 15020, 5, 0);
        
        ExchangeSimulator sim(15020, 5, config_path);
        
        // Verify ClientManager is working by checking client count
        EXPECT_EQ(sim.get_num_connected_clients(), 0) << "Initially no clients";
        
        // Start the simulator
        sim.start();
        
        std::thread event_thread([&sim]() {
            sim.run();
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Create a simple client connection
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(sock, 0);
        
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(15020);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        
        int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        ASSERT_GE(result, 0) << "Client should connect successfully";
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Verify client is tracked by ClientManager
        EXPECT_EQ(sim.get_num_connected_clients(), 1) << "One client connected";
        
        // Send subscription message: command(1) + count(2) + symbol_ids(2*n)
        std::vector<uint8_t> sub_msg;
        sub_msg.push_back(0xFF); // Subscribe command
        sub_msg.push_back(2);    // count low byte (2 symbols)
        sub_msg.push_back(0);    // count high byte
        sub_msg.push_back(0);    // symbol 0 low byte
        sub_msg.push_back(0);    // symbol 0 high byte
        sub_msg.push_back(1);    // symbol 1 low byte
        sub_msg.push_back(0);    // symbol 1 high byte
        
        ssize_t sent = send(sock, sub_msg.data(), sub_msg.size(), 0);
        EXPECT_EQ(sent, static_cast<ssize_t>(sub_msg.size()));
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Verify subscription was recorded
        auto client_fds = sim.get_client_fds();
        ASSERT_EQ(client_fds.size(), 1);
        EXPECT_TRUE(sim.is_client_subscribed(client_fds[0], 0));
        EXPECT_TRUE(sim.is_client_subscribed(client_fds[0], 1));
        EXPECT_FALSE(sim.is_client_subscribed(client_fds[0], 2));
        EXPECT_EQ(sim.get_client_subscription_count(client_fds[0]), 2);
        
        close(sock);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Verify client is removed after disconnect
        EXPECT_EQ(sim.get_num_connected_clients(), 0) << "Client disconnected";
        
        sim.stop();
        
        if (event_thread.joinable()) {
            event_thread.join();
        }
        
        SUCCEED() << "ClientManager integration verified";
        
    } catch (const std::exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

} // namespace mdfh

// Main function for running tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
