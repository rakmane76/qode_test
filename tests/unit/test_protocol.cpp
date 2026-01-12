#include <gtest/gtest.h>
#include "common/protocol.h"
#include <cstring>

using namespace mdfh;

class ProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(ProtocolTest, MessageHeaderSize) {
    EXPECT_EQ(sizeof(MessageHeader), 16);
}

TEST_F(ProtocolTest, TradeMessageSize) {
    EXPECT_EQ(sizeof(TradeMessage), 32);  // 16 header + 12 payload + 4 checksum
}

TEST_F(ProtocolTest, QuoteMessageSize) {
    EXPECT_EQ(sizeof(QuoteMessage), 44);  // 16 header + 24 payload + 4 checksum
}

TEST_F(ProtocolTest, MessageTypeValues) {
    EXPECT_EQ(static_cast<uint16_t>(MessageType::TRADE), 0x01);
    EXPECT_EQ(static_cast<uint16_t>(MessageType::QUOTE), 0x02);
    EXPECT_EQ(static_cast<uint16_t>(MessageType::HEARTBEAT), 0x03);
}

TEST_F(ProtocolTest, SubscriptionMessageFormat) {
    // Test subscription message structure
    uint8_t buffer[256];
    buffer[0] = 0xFF; // Subscribe command
    
    uint16_t count = 5;
    std::memcpy(&buffer[1], &count, sizeof(uint16_t));
    
    // Add symbol IDs
    for (uint16_t i = 0; i < count; i++) {
        uint16_t symbol_id = i * 10;
        std::memcpy(&buffer[3 + i * sizeof(uint16_t)], &symbol_id, sizeof(uint16_t));
    }
    
    EXPECT_EQ(buffer[0], 0xFF);
    
    uint16_t read_count;
    std::memcpy(&read_count, &buffer[1], sizeof(uint16_t));
    EXPECT_EQ(read_count, 5);
}

TEST_F(ProtocolTest, MessageHeaderConstruction) {
    MessageHeader header;
    header.msg_type = static_cast<uint16_t>(MessageType::TRADE);
    header.seq_num = 12345;
    header.timestamp = 1234567890123456789ULL;
    header.symbol_id = 42;
    
    EXPECT_EQ(header.msg_type, 0x01);
    EXPECT_EQ(header.seq_num, 12345);
    EXPECT_EQ(header.timestamp, 1234567890123456789ULL);
    EXPECT_EQ(header.symbol_id, 42);
}

TEST_F(ProtocolTest, TradeMessageConstruction) {
    TradeMessage trade;
    trade.payload.price = 1234.56;
    trade.payload.quantity = 100;
    
    EXPECT_DOUBLE_EQ(trade.payload.price, 1234.56);
    EXPECT_EQ(trade.payload.quantity, 100);
}

TEST_F(ProtocolTest, QuoteMessageConstruction) {
    QuoteMessage quote;
    quote.payload.bid_price = 1000.50;
    quote.payload.bid_qty = 500;
    quote.payload.ask_price = 1001.00;
    quote.payload.ask_qty = 300;
    
    EXPECT_DOUBLE_EQ(quote.payload.bid_price, 1000.50);
    EXPECT_EQ(quote.payload.bid_qty, 500);
    EXPECT_DOUBLE_EQ(quote.payload.ask_price, 1001.00);
    EXPECT_EQ(quote.payload.ask_qty, 300);
}

TEST_F(ProtocolTest, MessageAlignment) {
    // Verify that structures are packed (alignment = 1 due to __attribute__((packed)))
    EXPECT_EQ(alignof(MessageHeader), 1);
    EXPECT_EQ(alignof(TradeMessage), 1);
    EXPECT_EQ(alignof(QuoteMessage), 1);
}

TEST_F(ProtocolTest, MessagePacking) {
    // Ensure structures are packed (no padding)
    MessageHeader header;
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&header.seq_num) -
              reinterpret_cast<uintptr_t>(&header.msg_type), 2);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&header.timestamp) - 
              reinterpret_cast<uintptr_t>(&header.seq_num), 4);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&header.symbol_id) - 
              reinterpret_cast<uintptr_t>(&header.timestamp), 8);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
