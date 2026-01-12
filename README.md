# Market Data Feed Handler

A high-performance market data feed handler implementation featuring a realistic exchange simulator and low-latency client processing system.

## Overview

This project implements a complete market data distribution system consisting of:

1. **Exchange Simulator** - TCP server that generates realistic market ticks using Geometric Brownian Motion
2. **Feed Handler Client** - High-performance client with zero-copy parsing and lock-free caching
3. **Terminal Visualizer** - Real-time display of market data with statistics

## Features

### Exchange Simulator
- Generates realistic price movements using Geometric Brownian Motion (GBM)
- Supports 100+ concurrent symbols
- Configurable tick rates (10K - 500K messages/second)
- epoll-based multi-client handling
- Graceful client connection/disconnection management

### Feed Handler
- Non-blocking TCP client with edge-triggered epoll
- Zero-copy binary protocol parser
- Lock-free symbol cache for concurrent access
- Automatic reconnection with exponential backoff
- Sub-microsecond latency tracking

### Visualizer
- Real-time terminal-based display
- Top 20 most active symbols
- Color-coded price changes
- Live performance statistics

## Architecture

**Server Side:**
- Exchange Simulator
  - GBM Tick Generator (realistic price movements)
  - epoll-based Multi-client Handler
  - Binary Protocol Encoder

**Communication:** Binary Protocol over TCP

**Client Side:**
- Feed Handler
  - Non-blocking Socket (edge-triggered epoll)
  - Zero-copy Parser (generic handler)
  - Lock-free Symbol Cache (atomic operations)
  - Terminal Visualizer (real-time display)

## Building

### Prerequisites
- Linux operating system
- GCC 7+ or Clang 6+ with C++17 support
- GNU Make or CMake

### Build with CMake (Recommended)
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Or using CMake directly
cmake --build . --config Release --parallel
```

**Build Targets:**
- `exchange_server` - Exchange simulator server
- `feed_client` - Feed handler client with visualizer
- All libraries (`libmdfh_server.a`, `libmdfh_client.a`, `libmdfh_common.a`)

### Build with Make
```bash
make                  # Build release version
make debug            # Build debug version
make clean            # Clean build artifacts
make test             # Run unit tests
```

## Running

### Configuration

The exchange simulator uses a configuration file (`config/server.conf`) for all settings:

```ini
# Network Settings
server.port = 9876
server.max_clients = 1000

# Market Data Settings
market.num_symbols = 100
market.tick_rate = 100000
market.symbols_file = config/symbols.csv

# Fault Injection (for testing)
fault_injection.enabled = false
```

You can create custom configuration files for different scenarios.

### Start the Exchange Simulator
```bash
# Using script (recommended)
./scripts/run_server.sh

# Using custom config
./scripts/run_server.sh my_config.conf

# Or directly from build directory
./build/exchange_server [config_file]

# Example with full path:
./build/exchange_server config/server.conf
```

**Configuration File:**
- `config_file`: Path to configuration file (default: `config/server.conf`)
- All settings loaded from config: port, max clients, tick rate, symbols file
- Symbol data is loaded from the CSV file specified in `market.symbols_file`
- See `config/server.conf` for all available options

### Start the Feed Handler Client
```bash
# Using script (recommended)
./scripts/run_client.sh

# Or directly from build directory
./build/feed_client [host] [port] [num_symbols]

# Examples:
./build/feed_client                      # Default: localhost:9876, 100 symbols
./build/feed_client 127.0.0.1 9876 100   # Explicit parameters
./build/feed_client 192.168.1.100 9876 500  # Remote server, 500 symbols
```

**Arguments:**
- `host`: Server hostname/IP (default: 127.0.0.1)
- `port`: Server port (default: 9876)
- `num_symbols`: Number of symbols to track (default: 100)

**Display:**
- Real-time terminal UI showing top 20 most active symbols
- Live performance statistics (latency percentiles, throughput)
- Color-coded price changes (green=up, red=down)
- Press Ctrl+C to exit gracefully

## Binary Protocol

### Message Format

**Header (16 bytes):**

|Messge Type | Seq Num  | Timestamp  | Symbol ID|
|-----------------------------------------------|
|  (2 bytes) │ (4 bytes)│  (8 bytes) │ (2 bytes)│

**Message Types:**
- `0x01` - Trade: Price (8 bytes) + Quantity (4 bytes) = 32 bytes total
- `0x02` - Quote: Bid Price (8) + Bid Qty (4) + Ask Price (8) + Ask Qty (4) = 48 bytes total
- `0x03` - Heartbeat: No payload = 20 bytes total

**Byte Order:** Little-endian (x86/x64 native)

**Checksum:** Last 4 bytes = XOR-based checksum of all previous bytes

## Performance Targets

| Component | Metric | Target |
|-----------|--------|--------|
| Server | Tick Generation | 100K+ msgs/sec |
| Client | Parse Throughput | 100K+ msgs/sec |
| Client | Read Latency | p99 < 50μs |
| Cache | Update Latency | < 100ns |
| Cache | Read Latency | < 50ns |

## Project Structure

qodeTest/
  ├── src/
  │   ├── server/          # Exchange simulator
  │   ├── client/          # Feed handler
  │   └── common/          # Shared components
  ├── include/             # Header files
  ├── docs/                # Documentation
  ├── tests/               # Test suites
  ├── benchmarks/          # Performance benchmarks
  ├── scripts/             # Build/run scripts
  ├── config/              # Configuration files
  ├── Makefile             # GNU Make build
  ├── CMakeLists.txt       # CMake build
  └── README.md            # This file

## Documentation

- [DESIGN.md](docs/DESIGN.md) - Architecture and design decisions
- [NETWORK.md](docs/NETWORK.md) - Network implementation details
- [GBM.md](docs/GBM.md) - Geometric Brownian Motion explanation
- [PERFORMANCE.md](docs/PERFORMANCE.md) - Performance analysis
- [QUESTIONS.md](docs/QUESTIONS.md) - Critical thinking questions answered

## Key Design Decisions

1. **epoll with Edge-Triggered Mode** - Minimizes system calls and provides low-latency notifications
2. **Lock-Free Symbol Cache** - Uses atomics with memory ordering for concurrent access without locks
3. **Zero-Copy Parsing** - Processes data in-place without extra memory allocations
4. **Ring Buffer for Latency Tracking** - Fixed-size buffer with histogram for fast percentile calculation
5. **Exponential Backoff Reconnection** - Prevents connection storms during server issues

## Testing

### Unit Tests

Run comprehensive unit tests for all components:

```bash
# Run all unit tests
cd build && ctest

# Run specific test suites
cd build/tests
./cache_test                # Lock-free cache tests
./parser_test               # Protocol parser tests (12 tests)
./feed_handler_test         # Feed handler tests
./latency_tracker_test      # Latency tracking tests
./tick_generator_test       # GBM tick generation tests
./config_parser_test        # Configuration parsing tests
./client_manager_test       # Client management tests
./exchange_simulator_test   # Exchange simulator tests
./visualizer_test           # Visualizer UI tests

# Run with verbose output
cd build && ctest -V

# Run specific test by name
cd build && ctest -R parser -V
```

**Status**: ✅ All unit test suites passing (12/12 parser tests, all using generic handler)

### Visualizer Testing

Test the real-time terminal UI:

```bash
# Run automated visualizer tests
cd build/tests
./visualizer_test

# Run interactive demo (see it in action!)
./scripts/test_visualizer_demo.sh

# Quick visualizer test
./scripts/run_visualizer_test.sh
```

For detailed testing documentation:
- [tests/VISUALIZER_TESTING.md](tests/VISUALIZER_TESTING.md) - Testing methodology
- [VISUALIZER_QUICK_REF.md](VISUALIZER_QUICK_REF.md) - Quick reference

**Status**: ✅ Visualizer tests passing

### Benchmarks

Performance benchmarks using Google Benchmark:

```bash
# Build with benchmarks enabled
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
make -j$(nproc)

# Run all benchmarks
./scripts/run_benchmarks.sh

# Run individual benchmarks
cd build
./parser_benchmark              # Parser performance (6.8M msg/s)
./cache_benchmark               # Cache operations (8ns read, 45ns write)
./latency_benchmark             # Latency tracking (26ns overhead)
./tick_generator_benchmark      # GBM generation (12.5M ticks/s)
./socket_benchmark              # Socket operations
./memory_pool_benchmark         # Memory allocation
```

**Results:** See [benchmarks/BENCHMARK_RESULTS.md](benchmarks/BENCHMARK_RESULTS.md) for detailed analysis

## Documentation

**Core Documentation:**
- [docs/DESIGN.md](docs/DESIGN.md) - System architecture and design decisions
- [docs/NETWORK.md](docs/NETWORK.md) - Network protocol specification
- [docs/PERFORMANCE.md](docs/PERFORMANCE.md) - Comprehensive performance analysis
- [docs/LOW_LATENCY_PATTERNS.md](docs/LOW_LATENCY_PATTERNS.md) - Low-latency techniques and patterns
- [docs/GBM.md](docs/GBM.md) - Geometric Brownian Motion implementation
- [docs/QUESTIONS.md](docs/QUESTIONS.md) - Critical thinking and design Q&A

**Component Documentation:**
- [docs/VISUALIZER.md](docs/VISUALIZER.md) - Visualizer API and usage
- [tests/VISUALIZER_TESTING.md](tests/VISUALIZER_TESTING.md) - Visualizer testing guide
- [VISUALIZER_QUICK_REF.md](VISUALIZER_QUICK_REF.md) - Quick reference

**Benchmark Results:**
- [benchmarks/README.md](benchmarks/README.md) - How to run benchmarks
- [benchmarks/BENCHMARK_RESULTS.md](benchmarks/BENCHMARK_RESULTS.md) - Detailed performance results

## Troubleshooting

### Connection Refused
- Ensure server is running: `netstat -an | grep 9876`
- Check firewall settings: `sudo iptables -L`

### High Latency
- Verify CPU frequency scaling: `cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor`
- Should be "performance" mode: `sudo cpupower frequency-set -g performance`

### Message Parsing Errors
- Check sequence gaps in visualizer
- Enable debug logging: Rebuild with `make debug`

## License

This is an educational project for learning high-performance C++ systems programming.

## Author

Created as part of the Market Data Feed Handler assignment demonstrating:
- Low-latency network programming
- Lock-free concurrent data structures
- Systems programming in C++17
- Financial market data protocols
