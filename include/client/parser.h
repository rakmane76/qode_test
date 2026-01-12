#ifndef PARSER_H
#define PARSER_H

#include "common/protocol.h"
#include <cstdint>
#include <functional>
#include <vector>
#include <type_traits>

namespace mdfh {

class BinaryParser {
public:
    BinaryParser();
    ~BinaryParser();
    
    // Generic template-based message handler (zero-copy, low-latency)
    template<typename HandlerT>
    void set_generic_handler(HandlerT&& handler);
    
    // Parse data from socket (handles fragmentation)
    size_t parse(const void* data, size_t len);
    
    // Get statistics
    uint64_t get_messages_parsed() const { return messages_parsed_; }
    uint64_t get_sequence_gaps() const { return sequence_gaps_; }
    uint64_t get_checksum_errors() const { return checksum_errors_; }
    uint64_t get_malformed_messages() const { return malformed_messages_; }
    uint64_t get_fragmented_count() const { return fragmented_messages_; }
    
    // Reset parser state
    void reset();
    
private:
    static constexpr size_t MAX_MESSAGE_SIZE = 1024;
    static constexpr size_t BUFFER_SIZE = 65536;
    
    // Fragmentation buffer
    std::vector<uint8_t> buffer_;
    size_t buffer_pos_;
    
    // Generic handler (type-erased)
    std::function<void(const void*, MessageType)> generic_handler_;
    
    // Statistics
    uint64_t messages_parsed_;
    uint64_t sequence_gaps_;
    uint64_t checksum_errors_;
    uint64_t malformed_messages_;
    uint64_t fragmented_messages_;
    uint32_t last_seq_num_;
    
    // Parse a complete message from buffer
    bool try_parse_message();
    
    // Validate and dispatch message
    bool process_message(const void* msg_data, size_t msg_size, MessageType type);
};

// Template implementations
template<typename HandlerT>
void BinaryParser::set_generic_handler(HandlerT&& handler) {
    generic_handler_ = [h = std::forward<HandlerT>(handler)](const void* data, MessageType type) {
        // Zero-copy dispatch with compile-time type resolution
        switch (type) {
            case MessageType::TRADE:
                h(*reinterpret_cast<const TradeMessage*>(data));
                break;
            case MessageType::QUOTE:
                h(*reinterpret_cast<const QuoteMessage*>(data));
                break;
            case MessageType::HEARTBEAT:
                h(*reinterpret_cast<const HeartbeatMessage*>(data));
                break;
            default:
                break;
        }
    };
}

} // namespace mdfh

#endif // PARSER_H
