#include <benchmark/benchmark.h>
#include "client/parser.h"
#include "common/protocol.h"
#include <vector>
#include <cstring>

using namespace mdfh;

// Helper function to create test messages
std::vector<uint8_t> create_test_message(MessageType type, uint16_t symbol_id, uint32_t seq_num) {
    std::vector<uint8_t> buffer;
    
    if (type == MessageType::TRADE) {
        buffer.resize(sizeof(TradeMessage));
        TradeMessage* msg = reinterpret_cast<TradeMessage*>(buffer.data());
        msg->header.msg_type = static_cast<uint16_t>(MessageType::TRADE);
        msg->header.seq_num = seq_num;
        msg->header.timestamp = 1234567890123456789ULL;
        msg->header.symbol_id = symbol_id;
        msg->payload.price = 2450.50;
        msg->payload.quantity = 1000;
        msg->checksum = calculate_checksum(buffer.data(), buffer.size() - 4);
    } else if (type == MessageType::QUOTE) {
        buffer.resize(sizeof(QuoteMessage));
        QuoteMessage* msg = reinterpret_cast<QuoteMessage*>(buffer.data());
        msg->header.msg_type = static_cast<uint16_t>(MessageType::QUOTE);
        msg->header.seq_num = seq_num;
        msg->header.timestamp = 1234567890123456789ULL;
        msg->header.symbol_id = symbol_id;
        msg->payload.bid_price = 2450.25;
        msg->payload.bid_qty = 500;
        msg->payload.ask_price = 2450.75;
        msg->payload.ask_qty = 600;
        msg->checksum = calculate_checksum(buffer.data(), buffer.size() - 4);
    }
    
    return buffer;
}

// Benchmark: Parse single Trade message
static void BM_ParseTrade(benchmark::State& state) {
    BinaryParser parser;
    auto msg_buffer = create_test_message(MessageType::TRADE, 1, 100);
    
    for (auto _ : state) {
        size_t parsed = parser.parse(msg_buffer.data(), msg_buffer.size());
        benchmark::DoNotOptimize(parsed);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ParseTrade);

// Benchmark: Parse single Quote message
static void BM_ParseQuote(benchmark::State& state) {
    BinaryParser parser;
    auto msg_buffer = create_test_message(MessageType::QUOTE, 1, 100);
    
    for (auto _ : state) {
        size_t parsed = parser.parse(msg_buffer.data(), msg_buffer.size());
        benchmark::DoNotOptimize(parsed);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ParseQuote);

// Benchmark: Parse stream with fragmentation
static void BM_ParseFragmentedStream(benchmark::State& state) {
    BinaryParser parser;
    std::vector<std::vector<uint8_t>> messages;
    
    // Create 100 messages
    for (int i = 0; i < 100; ++i) {
        messages.push_back(create_test_message(
            (i % 3 == 0) ? MessageType::QUOTE : MessageType::TRADE,
            i % 10,
            i
        ));
    }
    
    for (auto _ : state) {
        size_t total_parsed = 0;
        
        // Simulate fragmented reception
        for (const auto& msg : messages) {
            // Send first half
            size_t half = msg.size() / 2;
            parser.parse(msg.data(), half);
            
            // Send second half
            total_parsed += parser.parse(msg.data() + half, msg.size() - half);
        }
        
        benchmark::DoNotOptimize(total_parsed);
    }
    
    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(BM_ParseFragmentedStream);

// Benchmark: Parse continuous stream (realistic scenario)
static void BM_ParseContinuousStream(benchmark::State& state) {
    BinaryParser parser;
    
    // Create continuous buffer with multiple messages
    std::vector<uint8_t> stream_buffer;
    for (int i = 0; i < 1000; ++i) {
        auto msg = create_test_message(
            (i % 3 == 0) ? MessageType::QUOTE : MessageType::TRADE,
            i % 100,
            i
        );
        stream_buffer.insert(stream_buffer.end(), msg.begin(), msg.end());
    }
    
    for (auto _ : state) {
        parser.reset();
        size_t parsed = parser.parse(stream_buffer.data(), stream_buffer.size());
        benchmark::DoNotOptimize(parsed);
    }
    
    state.SetItemsProcessed(state.iterations() * 1000);
    state.SetBytesProcessed(state.iterations() * stream_buffer.size());
}
BENCHMARK(BM_ParseContinuousStream);

// Benchmark: Message validation overhead
static void BM_MessageValidation(benchmark::State& state) {
    auto msg_buffer = create_test_message(MessageType::QUOTE, 1, 100);
    MessageHeader* header = reinterpret_cast<MessageHeader*>(msg_buffer.data());
    
    for (auto _ : state) {
        bool valid = (header->msg_type == static_cast<uint16_t>(MessageType::QUOTE) &&
                     header->symbol_id < 500 &&
                     header->seq_num > 0);
        benchmark::DoNotOptimize(valid);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MessageValidation);

BENCHMARK_MAIN();
