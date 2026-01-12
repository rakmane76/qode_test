#include "client/parser.h"
#include <cstring>
#include <iostream>

namespace mdfh {

BinaryParser::BinaryParser()
    : buffer_(BUFFER_SIZE),
      buffer_pos_(0),
      messages_parsed_(0),
      sequence_gaps_(0),
      checksum_errors_(0),
      malformed_messages_(0),
      fragmented_messages_(0),
      last_seq_num_(0) {
}

BinaryParser::~BinaryParser() {
}

size_t BinaryParser::parse(const void* data, size_t len) {
    if (len == 0) return 0;
    
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    size_t consumed = 0;
    
    while (consumed < len) {
        // Calculate space available in buffer
        size_t space = BUFFER_SIZE - buffer_pos_;
        size_t to_copy = std::min(space, len - consumed);
        
        // Copy data to buffer
        std::memcpy(&buffer_[buffer_pos_], bytes + consumed, to_copy);
        buffer_pos_ += to_copy;
        consumed += to_copy;
        
        // Try to parse complete messages
        while (try_parse_message()) {
            // Continue parsing
        }
        
        // If buffer is full but no complete message, it's malformed
        if (buffer_pos_ >= BUFFER_SIZE && buffer_pos_ < sizeof(MessageHeader)) {
            malformed_messages_++;
            buffer_pos_ = 0;
        }
    }
    
    return consumed;
}

bool BinaryParser::try_parse_message() {
    // Need at least header
    if (buffer_pos_ < sizeof(MessageHeader)) {
        return false;
    }
    
    // Read header
    MessageHeader header;
    std::memcpy(&header, buffer_.data(), sizeof(MessageHeader));
    
    MessageType msg_type = static_cast<MessageType>(header.msg_type);
    size_t msg_size = get_message_size(msg_type);
    
    if (msg_size == 0 || msg_size > MAX_MESSAGE_SIZE) {
        // Unknown or invalid message type
        malformed_messages_++;
        // Skip this byte and try again
        std::memmove(buffer_.data(), &buffer_[1], buffer_pos_ - 1);
        buffer_pos_--;
        return false;
    }
    
    // Check if we have complete message
    if (buffer_pos_ < msg_size) {
        fragmented_messages_++;
        return false;
    }
    
    // Process message
    bool success = process_message(buffer_.data(), msg_size, msg_type);
    
    if (success) {
        messages_parsed_++;
    }
    
    // Remove message from buffer
    std::memmove(buffer_.data(), &buffer_[msg_size], buffer_pos_ - msg_size);
    buffer_pos_ -= msg_size;
    
    return true;
}

bool BinaryParser::process_message(const void* msg_data, size_t msg_size, 
                                    MessageType type) {
    // Validate checksum
    if (!validate_checksum(msg_data, msg_size)) {
        checksum_errors_++;
        return false;
    }
    
    // Zero-copy: use reinterpret_cast instead of memcpy for header access
    const MessageHeader* header = reinterpret_cast<const MessageHeader*>(msg_data);
    
    // Check sequence number
    if (last_seq_num_ != 0 && header->seq_num != last_seq_num_ + 1) {
        sequence_gaps_++;
    }
    last_seq_num_ = header->seq_num;
    
    // Use generic handler (required)
    if (!generic_handler_) {
        // No handler set - cannot process message
        return false;
    }
    
    generic_handler_(msg_data, type);
    return true;
}

void BinaryParser::reset() {
    buffer_pos_ = 0;
    messages_parsed_ = 0;
    sequence_gaps_ = 0;
    checksum_errors_ = 0;
    malformed_messages_ = 0;
    last_seq_num_ = 0;
}

} // namespace mdfh
