#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <cstring>

namespace mdfh {

// Message Types
enum class MessageType : uint16_t {
    TRADE = 0x01,
    QUOTE = 0x02,
    HEARTBEAT = 0x03,
    SUBSCRIBE = 0xFF
};

// Message Header (16 bytes)
struct __attribute__((packed)) MessageHeader {
    uint16_t msg_type;      // Message type
    uint32_t seq_num;       // Sequence number
    uint64_t timestamp;     // Nanoseconds since epoch
    uint16_t symbol_id;     // Symbol identifier
};

// Trade Message Payload (12 bytes)
struct __attribute__((packed)) TradePayload {
    double price;           // Trade price
    uint32_t quantity;      // Trade quantity
};

// Quote Message Payload (24 bytes)
struct __attribute__((packed)) QuotePayload {
    double bid_price;       // Best bid price
    uint32_t bid_qty;       // Bid quantity
    double ask_price;       // Best ask price
    uint32_t ask_qty;       // Ask quantity
};

// Complete Trade Message (16 + 12 + 4 = 32 bytes)
struct __attribute__((packed)) TradeMessage {
    MessageHeader header;
    TradePayload payload;
    uint32_t checksum;      // XOR of all previous bytes
};

// Complete Quote Message (16 + 24 + 4 = 44 bytes)
struct __attribute__((packed)) QuoteMessage {
    MessageHeader header;
    QuotePayload payload;
    uint32_t checksum;      // XOR of all previous bytes
};

// Heartbeat Message (16 + 4 = 20 bytes)
struct __attribute__((packed)) HeartbeatMessage {
    MessageHeader header;
    uint32_t checksum;      // XOR of all previous bytes
};

// Subscription Message
struct __attribute__((packed)) SubscriptionHeader {
    uint8_t command;        // 0xFF for subscribe
    uint16_t count;         // Number of symbols
};

// Helper functions
inline uint32_t calculate_checksum(const void* data, size_t len) {
    uint32_t checksum = 0;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; i++) {
        checksum ^= bytes[i];
    }
    return checksum;
}

inline bool validate_checksum(const void* data, size_t total_len) {
    if (total_len < 4) return false;
    
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t stored_checksum;
    std::memcpy(&stored_checksum, bytes + total_len - 4, 4);
    
    uint32_t calculated = calculate_checksum(bytes, total_len - 4);
    return calculated == stored_checksum;
}

inline size_t get_message_size(MessageType type) {
    switch (type) {
        case MessageType::TRADE: return sizeof(TradeMessage);
        case MessageType::QUOTE: return sizeof(QuoteMessage);
        case MessageType::HEARTBEAT: return sizeof(HeartbeatMessage);
        default: return 0;
    }
}

} // namespace mdfh

#endif // PROTOCOL_H
