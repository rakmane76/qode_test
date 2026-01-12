# Visualizer Documentation

## Overview

The Visualizer is a terminal-based real-time display component for the Market Data Feed Handler. It provides live monitoring of market data, connection status, and performance metrics.

## Features

### 1. **Real-time Market Data Display**
- Top 20 most active symbols
- Current bid, ask, and last traded price (LTP)
- Volume and price change percentage
- Update frequency tracking

### 2. **Connection Monitoring**
- Server connection status (CONNECTED/DISCONNECTED)
- Host and port information
- Uptime tracking

### 3. **Performance Metrics**
- Total messages received
- Message throughput (messages/second)
- Latency statistics:
  - Minimum, Maximum, Average
  - 50th, 95th, 99th percentiles

### 4. **Visual Indicators**
- Color-coded price changes:
  - **Green**: Price increase (>0.1%)
  - **Red**: Price decrease (<-0.1%)
  - **Yellow**: Minimal change
- Bold headers and section separators
- Formatted numbers with appropriate units

## Architecture

### Class: `Visualizer`

```cpp
class Visualizer {
public:
    Visualizer(const SymbolCache& cache, size_t num_symbols);
    void start();
    void stop();
    void update_stats(uint64_t messages, uint64_t msg_rate, const LatencyStats& latency);
    void set_connection_info(const std::string& host, uint16_t port, bool connected);
};
```

### Key Components

1. **Display Thread**: Runs continuously at 500ms intervals, updating the terminal display
2. **Cache Reader**: Reads symbol data from the shared SymbolCache
3. **Statistics Tracker**: Maintains message counts, rates, and latency metrics
4. **ANSI Formatter**: Uses ANSI escape codes for colors and screen control

## Usage

### Basic Usage

```cpp
#include "client/visualizer.h"

// Create visualizer with cache reference
mdfh::Visualizer viz(cache, num_symbols);

// Set connection information
viz.set_connection_info("127.0.0.1", 9999, true);

// Start display
viz.start();

// Update statistics periodically
LatencyStats stats = tracker.get_stats();
viz.update_stats(total_messages, msg_rate, stats);

// Stop display
viz.stop();
```

### Integration with Feed Handler

The visualizer integrates seamlessly with the feed handler:

```cpp
// In client_main.cpp
mdfh::Visualizer viz(handler.get_cache(), num_symbols);
viz.set_connection_info(server_host, server_port, handler.is_connected());
viz.start();

// In the main loop
while (running) {
    auto stats = handler.get_latency_stats();
    viz.update_stats(handler.get_message_count(), handler.get_msg_rate(), stats);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

viz.stop();
```

## Display Layout

```
=== NSE Market Data Feed Handler ===
Connected to: 127.0.0.1:9999 [CONNECTED]
Uptime: 0h 2m 35s

Top 20 Symbols:
┌─────────────┬────────┬────────┬────────┬──────────┬─────────┬─────────┐
│ Symbol      │ Bid    │ Ask    │ LTP    │ Volume   │ Change% │ Updates │
├─────────────┼────────┼────────┼────────┼──────────┼─────────┼─────────┤
│ RELIANCE    │ 2545.10│ 2545.65│ 2545.35│ 1.25M    │  +0.45% │   12543 │
│ TCS         │ 3456.50│ 3457.20│ 3456.90│ 987.5K   │  -0.12% │   11234 │
...

Statistics:
  Messages Received: 125,430
  Message Rate: 10,250 msg/s
  Latency: min=1.2μs avg=8.5μs p50=7.5μs p95=15.2μs p99=25.8μs max=45.3μs
```

## Testing

### Unit Tests

The visualizer has comprehensive unit tests covering:

1. **Basic Operations**
   - Construction/destruction
   - Start/stop cycles
   - Multiple start/stop cycles

2. **Data Updates**
   - Statistics updates
   - Connection info updates
   - Live data simulation

3. **Edge Cases**
   - Empty cache
   - High-frequency updates
   - Large symbol sets
   - Stop without start
   - Double stop

4. **Concurrency**
   - Thread safety
   - Concurrent cache updates
   - Concurrent stats updates

### Running Tests

```bash
# Run unit tests
./scripts/run_visualizer_test.sh

# Or directly
./build/tests/visualizer_test
```

### Demo Mode

Test the visualizer with live data:

```bash
# Run interactive demo
./scripts/test_visualizer_demo.sh
```

This starts:
1. Exchange simulator server (generates market data)
2. Feed client with visualizer (displays real-time data)

Press Ctrl+C to stop the demo.

## Configuration

### Display Parameters

```cpp
static constexpr size_t TOP_N_SYMBOLS = 20;     // Number of symbols to display
static constexpr int UPDATE_INTERVAL_MS = 500;   // Refresh rate in milliseconds
```

### Color Thresholds

Price change color coding:
- **Green**: change > +0.1%
- **Red**: change < -0.1%
- **Yellow**: -0.1% ≤ change ≤ +0.1%

## Performance Considerations

1. **Update Frequency**: 500ms refresh rate balances responsiveness and CPU usage
2. **Top N Symbols**: Displays only top 20 symbols to avoid screen clutter
3. **Lock-free Reads**: Uses seqlock protocol for non-blocking cache reads
4. **Minimal Overhead**: Display thread runs independently, doesn't block data processing

## Terminal Requirements

- **ANSI Support**: Terminal must support ANSI escape codes
- **Minimum Size**: 80x24 (standard terminal size)
- **Recommended**: 120x40 for comfortable viewing

### Tested Terminals
- ✅ Linux: gnome-terminal, xterm, konsole
- ✅ macOS: Terminal.app, iTerm2
- ✅ Windows: Windows Terminal, WSL2
- ⚠️ Windows CMD: Limited ANSI support

## Troubleshooting

### Display Issues

**Problem**: Garbled output or missing colors
- **Solution**: Ensure terminal supports ANSI escape codes
- **Workaround**: Run in a modern terminal emulator

**Problem**: Screen flickers excessively
- **Solution**: Increase UPDATE_INTERVAL_MS to reduce refresh rate

**Problem**: Display doesn't update
- **Solution**: Check that visualizer.start() was called

### Performance Issues

**Problem**: High CPU usage
- **Solution**: Reduce update frequency or display fewer symbols
- **Check**: Ensure display thread isn't running without being started

**Problem**: Delayed updates
- **Solution**: System may be overloaded; reduce tick rate on server

## Future Enhancements

Potential improvements:

1. **Interactive Mode**
   - Keyboard controls for pausing/resuming
   - Symbol filtering
   - Custom sorting options

2. **Multiple Views**
   - Orderbook view
   - Trade history
   - Performance graphs

3. **Export Capabilities**
   - Save snapshots to file
   - CSV export for analysis
   - Screen recording

4. **Advanced Metrics**
   - Bid-ask spread analysis
   - Volume-weighted average price (VWAP)
   - Market momentum indicators

## API Reference

### Constructor
```cpp
Visualizer(const SymbolCache& cache, size_t num_symbols)
```
Creates a visualizer instance.
- **Parameters**:
  - `cache`: Reference to symbol cache for reading market data
  - `num_symbols`: Total number of symbols in the system

### Methods

#### `void start()`
Starts the display thread. Should be called once after construction.

#### `void stop()`
Stops the display thread and cleans up. Safe to call multiple times.

#### `void update_stats(uint64_t messages, uint64_t msg_rate, const LatencyStats& latency)`
Updates performance statistics displayed.
- **Parameters**:
  - `messages`: Total messages received
  - `msg_rate`: Current message throughput (msg/s)
  - `latency`: Latency statistics structure

#### `void set_connection_info(const std::string& host, uint16_t port, bool connected)`
Updates connection status information.
- **Parameters**:
  - `host`: Server hostname or IP address
  - `port`: Server port number
  - `connected`: Connection status flag

## Examples

### Example 1: Basic Visualization

```cpp
#include "client/visualizer.h"
#include "common/cache.h"

int main() {
    const size_t num_symbols = 50;
    
    // Create cache
    mdfh::SymbolCache cache(num_symbols);
    
    // Create and start visualizer
    mdfh::Visualizer viz(cache, num_symbols);
    viz.set_connection_info("127.0.0.1", 9999, true);
    viz.start();
    
    // Run for 60 seconds
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    // Stop and cleanup
    viz.stop();
    
    return 0;
}
```

### Example 2: With Statistics Updates

```cpp
mdfh::SymbolCache cache(100);
mdfh::Visualizer viz(cache, 100);
mdfh::LatencyTracker tracker(10000);

viz.set_connection_info("exchange.example.com", 8888, true);
viz.start();

while (running) {
    // Process market data...
    // Update cache...
    
    // Update visualizer every second
    auto stats = tracker.get_stats();
    viz.update_stats(total_messages, message_rate, stats);
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

viz.stop();
```

### Example 3: Connection State Handling

```cpp
mdfh::Visualizer viz(cache, num_symbols);
viz.start();

// Initially disconnected
viz.set_connection_info(host, port, false);

// Attempt connection
if (connect_to_server(host, port)) {
    viz.set_connection_info(host, port, true);
} else {
    std::cerr << "Failed to connect" << std::endl;
}

// Handle reconnection
while (running) {
    if (!is_connected()) {
        viz.set_connection_info(host, port, false);
        if (reconnect()) {
            viz.set_connection_info(host, port, true);
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

viz.stop();
```

## License

Part of the Market Data Feed Handler project.
