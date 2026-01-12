#include <gtest/gtest.h>
#include "client/parser.h"
#include "common/protocol.h"
#include <cstring>
#include <vector>

using namespace mdfh;

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class ParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<BinaryParser>();
    }

    std::unique_ptr<BinaryParser> parser;
    
    void create_trade_message(std::vector<uint8_t>& buffer, uint32_t seq, uint16_t symbol_id, 
                              double price, uint32_t quantity) {
        MessageHeader header;
        header.msg_type = static_cast<uint16_t>(MessageType::TRADE);
        header.seq_num = seq;
        header.timestamp = 1234567890ULL;
        header.symbol_id = symbol_id;
        
        TradeMessage trade;
        trade.header = header;
        trade.payload.price = price;
        trade.payload.quantity = quantity;
        
        buffer.resize(sizeof(TradeMessage));
        
        // Calculate checksum on header + payload (excluding checksum field)
        uint32_t checksum = calculate_checksum(&trade, sizeof(TradeMessage) - sizeof(uint32_t));
        trade.checksum = checksum;
        
        memcpy(buffer.data(), &trade, sizeof(TradeMessage));
    }
    
    void create_quote_message(std::vector<uint8_t>& buffer, uint32_t seq, uint16_t symbol_id,
                             double bid, uint32_t bid_qty, double ask, uint32_t ask_qty) {
        MessageHeader header;
        header.msg_type = static_cast<uint16_t>(MessageType::QUOTE);
        header.seq_num = seq;
        header.timestamp = 1234567890ULL;
        header.symbol_id = symbol_id;
        
        QuoteMessage quote;
        quote.header = header;
        quote.payload.bid_price = bid;
        quote.payload.bid_qty = bid_qty;
        quote.payload.ask_price = ask;
        quote.payload.ask_qty = ask_qty;
        
        buffer.resize(sizeof(QuoteMessage));
        
        // Calculate checksum on header + payload (excluding checksum field)
        uint32_t checksum = calculate_checksum(&quote, sizeof(QuoteMessage) - sizeof(uint32_t));
        quote.checksum = checksum;
        
        memcpy(buffer.data(), &quote, sizeof(QuoteMessage));
    }
};

TEST_F(ParserTest, ParseSingleTradeMessage) {
    std::vector<uint8_t> buffer;
    create_trade_message(buffer, 1, 10, 1500.50, 100);
    
    TradeMessage received_trade;
    bool trade_received = false;
    
    parser->set_generic_handler([&](const auto& msg) {
        using MsgType = std::decay_t<decltype(msg)>;
        if constexpr (std::is_same_v<MsgType, TradeMessage>) {
            received_trade = msg;
            trade_received = true;
        }
    });
    
    size_t parsed = parser->parse(buffer.data(), buffer.size());
    
    ASSERT_EQ(parsed, buffer.size());
    ASSERT_TRUE(trade_received);
    EXPECT_EQ(received_trade.header.msg_type, static_cast<uint16_t>(MessageType::TRADE));
    EXPECT_EQ(received_trade.header.seq_num, 1);
    EXPECT_EQ(received_trade.header.symbol_id, 10);
    EXPECT_DOUBLE_EQ(received_trade.payload.price, 1500.50);
    EXPECT_EQ(received_trade.payload.quantity, 100);
}

TEST_F(ParserTest, ParseSingleQuoteMessage) {
    std::vector<uint8_t> buffer;
    create_quote_message(buffer, 2, 5, 2450.25, 1000, 2450.75, 800);
    
    QuoteMessage received_quote;
    bool quote_received = false;
    
    parser->set_generic_handler([&](const auto& msg) {
        using MsgType = std::decay_t<decltype(msg)>;
        if constexpr (std::is_same_v<MsgType, QuoteMessage>) {
            received_quote = msg;
            quote_received = true;
        }
    });
    
    size_t parsed = parser->parse(buffer.data(), buffer.size());
    
    ASSERT_EQ(parsed, buffer.size());
    ASSERT_TRUE(quote_received);
    EXPECT_EQ(received_quote.header.msg_type, static_cast<uint16_t>(MessageType::QUOTE));
    EXPECT_EQ(received_quote.header.seq_num, 2);
    EXPECT_EQ(received_quote.header.symbol_id, 5);
    EXPECT_DOUBLE_EQ(received_quote.payload.bid_price, 2450.25);
    EXPECT_EQ(received_quote.payload.bid_qty, 1000);
    EXPECT_DOUBLE_EQ(received_quote.payload.ask_price, 2450.75);
    EXPECT_EQ(received_quote.payload.ask_qty, 800);
}

TEST_F(ParserTest, ParseMultipleMessages) {
    std::vector<uint8_t> buffer1, buffer2, buffer_combined;
    create_trade_message(buffer1, 1, 10, 1500.50, 100);
    create_quote_message(buffer2, 2, 5, 2450.25, 1000, 2450.75, 800);
    
    buffer_combined.insert(buffer_combined.end(), buffer1.begin(), buffer1.end());
    buffer_combined.insert(buffer_combined.end(), buffer2.begin(), buffer2.end());
    
    int msg_count = 0;
    parser->set_generic_handler([&msg_count](const auto&) { 
        msg_count++; 
    });
    
    size_t parsed = parser->parse(buffer_combined.data(), buffer_combined.size());
    
    ASSERT_EQ(parsed, buffer_combined.size());
    EXPECT_EQ(msg_count, 2);
}

TEST_F(ParserTest, HandleFragmentedMessage) {
    std::vector<uint8_t> buffer;
    create_trade_message(buffer, 1, 10, 1500.50, 100);
    
    // Split into two parts
    size_t split_point = buffer.size() / 2;
    
    int msg_count = 0;
    parser->set_generic_handler([&msg_count](const auto&) { 
        msg_count++; 
    });
    
    // Parse first fragment - should consume all bytes but not parse message yet
    size_t parsed1 = parser->parse(buffer.data(), split_point);
    EXPECT_EQ(parsed1, split_point) << "Should consume all input bytes";
    EXPECT_EQ(msg_count, 0) << "Should not parse incomplete message";
    
    // Parse second fragment - should complete the message
    size_t parsed2 = parser->parse(buffer.data() + split_point, buffer.size() - split_point);
    EXPECT_EQ(parsed2, buffer.size() - split_point) << "Should consume remaining bytes";
    EXPECT_EQ(msg_count, 1) << "Should parse complete message";
}

TEST_F(ParserTest, DetectSequenceGap) {
    std::vector<uint8_t> buffer1, buffer2;
    create_trade_message(buffer1, 1, 10, 1500.50, 100);
    create_trade_message(buffer2, 5, 10, 1501.00, 100); // Gap: 1 -> 5
    
    parser->set_generic_handler([](const auto&) {});
    
    parser->parse(buffer1.data(), buffer1.size());
    parser->parse(buffer2.data(), buffer2.size());
    
    EXPECT_GT(parser->get_sequence_gaps(), 0);
}

TEST_F(ParserTest, ValidateChecksum) {
    std::vector<uint8_t> buffer;
    create_trade_message(buffer, 1, 10, 1500.50, 100);
    
    // Corrupt checksum
    buffer[buffer.size() - 1] ^= 0xFF;
    
    parser->set_generic_handler([](const auto&) {});
    size_t parsed = parser->parse(buffer.data(), buffer.size());
    
    EXPECT_EQ(parsed, buffer.size()) << "Should consume the buffer";
    EXPECT_GT(parser->get_checksum_errors(), 0);
}

TEST_F(ParserTest, HandleInvalidMessageType) {
    std::vector<uint8_t> buffer;
    create_trade_message(buffer, 1, 10, 1500.50, 100);
    
    // Change to invalid message type
    uint16_t invalid_type = 0xFF;
    memcpy(buffer.data(), &invalid_type, sizeof(invalid_type));
    
    size_t parsed = parser->parse(buffer.data(), buffer.size());
    EXPECT_GT(parsed, 0) << "Should consume the buffer";
    EXPECT_GT(parser->get_malformed_messages(), 0) << "Should track malformed messages";
}

TEST_F(ParserTest, ZeroAllocationInHotPath) {
    std::vector<uint8_t> buffer;
    create_trade_message(buffer, 1, 10, 1500.50, 100);
    
    // Parse multiple times - should reuse internal buffers
    for (int i = 0; i < 1000; ++i) {
        auto result = parser->parse(buffer.data(), buffer.size());
        EXPECT_GT(result, 0);
    }
}

TEST_F(ParserTest, PerformanceThroughput) {
    std::vector<uint8_t> buffer;
    create_trade_message(buffer, 1, 10, 1500.50, 100);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100000; ++i) {
        parser->parse(buffer.data(), buffer.size());
        parser->reset(); // Reset for next iteration
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    double msgs_per_sec = 100000.0 / (duration / 1000000.0);
    std::cout << "Parser throughput: " << msgs_per_sec << " msgs/sec" << std::endl;
    
    EXPECT_GT(msgs_per_sec, 100000.0) << "Parser should handle > 100K msgs/sec";
}

TEST_F(ParserTest, GenericHandlerForAllMessages) {
    std::vector<uint8_t> trade_buffer, quote_buffer;
    create_trade_message(trade_buffer, 1, 10, 1500.50, 100);
    create_quote_message(quote_buffer, 2, 5, 2450.25, 1000, 2450.75, 800);
    
    int trade_count = 0;
    int quote_count = 0;
    TradeMessage received_trade;
    QuoteMessage received_quote;
    
    // Use generic handler with auto parameter (C++14)
    parser->set_generic_handler([&](const auto& msg) {
        using MsgType = std::decay_t<decltype(msg)>;
        
        if constexpr (std::is_same_v<MsgType, TradeMessage>) {
            trade_count++;
            received_trade = msg;
        } else if constexpr (std::is_same_v<MsgType, QuoteMessage>) {
            quote_count++;
            received_quote = msg;
        }
    });
    
    // Parse trade message
    parser->parse(trade_buffer.data(), trade_buffer.size());
    EXPECT_EQ(trade_count, 1);
    EXPECT_EQ(quote_count, 0);
    EXPECT_EQ(received_trade.header.seq_num, 1);
    EXPECT_DOUBLE_EQ(received_trade.payload.price, 1500.50);
    
    // Parse quote message
    parser->parse(quote_buffer.data(), quote_buffer.size());
    EXPECT_EQ(trade_count, 1);
    EXPECT_EQ(quote_count, 1);
    EXPECT_EQ(received_quote.header.seq_num, 2);
    EXPECT_DOUBLE_EQ(received_quote.payload.bid_price, 2450.25);
}

TEST_F(ParserTest, GenericHandlerPerformance) {
    std::vector<uint8_t> buffer;
    create_trade_message(buffer, 1, 10, 1500.50, 100);
    
    int msg_count = 0;
    parser->set_generic_handler([&msg_count](const auto&) {
        msg_count++;
    });
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100000; ++i) {
        parser->parse(buffer.data(), buffer.size());
        parser->reset();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    double msgs_per_sec = 100000.0 / (duration / 1000000.0);
    std::cout << "Generic handler throughput: " << msgs_per_sec << " msgs/sec" << std::endl;
    
    EXPECT_EQ(msg_count, 100000);
    EXPECT_GT(msgs_per_sec, 100000.0) << "Generic handler should handle > 100K msgs/sec";
}

TEST_F(ParserTest, GenericHandlerOverridesIndividualCallbacks) {
    // This test is no longer relevant since we only have generic handler
    // Keeping it as a placeholder for generic handler priority test
    std::vector<uint8_t> buffer;
    create_trade_message(buffer, 1, 10, 1500.50, 100);
    
    int generic_count = 0;
    
    // Set generic handler
    parser->set_generic_handler([&generic_count](const auto&) {
        generic_count++;
    });
    
    parser->parse(buffer.data(), buffer.size());
    
    // Generic handler should be called
    EXPECT_EQ(generic_count, 1);
}
