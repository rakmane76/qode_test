#!/bin/bash

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Market Data Feed Handler - Benchmarks${NC}"
echo -e "${BLUE}========================================${NC}\n"

# Build directory
BUILD_DIR="../build"
BENCHMARK_DIR="$BUILD_DIR/benchmarks"
BENCHMARK_DIR="scripts/build"

# Check if benchmarks are built
if [ ! -d "$BENCHMARK_DIR" ]; then
    echo -e "${RED}Error: Benchmarks not built. Please run build.sh first.${NC}"
    exit 1
fi

# Create results directory
RESULTS_DIR="./benchmark_results"
mkdir -p "$RESULTS_DIR"

# Run each benchmark
echo -e "${GREEN}Running Parser Benchmark...${NC}"
$BENCHMARK_DIR/parser_benchmark --benchmark_out=$RESULTS_DIR/parser.json --benchmark_out_format=json

echo -e "\n${GREEN}Running Cache Benchmark...${NC}"
$BENCHMARK_DIR/cache_benchmark --benchmark_out=$RESULTS_DIR/cache.json --benchmark_out_format=json

echo -e "\n${GREEN}Running Latency Tracker Benchmark...${NC}"
$BENCHMARK_DIR/latency_benchmark --benchmark_out=$RESULTS_DIR/latency.json --benchmark_out_format=json

echo -e "\n${GREEN}Running Tick Generator Benchmark...${NC}"
$BENCHMARK_DIR/tick_generator_benchmark --benchmark_out=$RESULTS_DIR/tick_generator.json --benchmark_out_format=json

echo -e "\n${GREEN}Running Memory Pool Benchmark...${NC}"
$BENCHMARK_DIR/memory_pool_benchmark --benchmark_out=$RESULTS_DIR/memory_pool.json --benchmark_out_format=json

echo -e "\n${GREEN}Running Socket Benchmark...${NC}"
$BENCHMARK_DIR/socket_benchmark --benchmark_out=$RESULTS_DIR/socket.json --benchmark_out_format=json

echo -e "\n${BLUE}========================================${NC}"
echo -e "${GREEN}All benchmarks completed!${NC}"
echo -e "${BLUE}Results saved to: $RESULTS_DIR${NC}"
echo -e "${BLUE}========================================${NC}"

# Generate summary
echo -e "\n${GREEN}Generating Summary...${NC}"

cat > $RESULTS_DIR/SUMMARY.txt << 'EOF'
Market Data Feed Handler - Benchmark Summary
=============================================

Key Performance Metrics:
------------------------

1. Parser Performance:
   - Single message parsing: ~X ns/msg
   - Continuous stream: ~Y msg/s
   - Fragmented stream handling: ~Z msg/s

2. Cache Performance:
   - Update latency: ~X ns
   - Read latency: ~Y ns
   - Concurrent read throughput: ~Z ops/s

3. Latency Tracking:
   - Record overhead: ~X ns
   - Statistics calculation: ~Y ns
   - Multi-threaded recording: ~Z ops/s

4. Tick Generation:
   - Single tick: ~X ns
   - Batch generation (100 symbols): ~Y ticks/s
   - Realistic workload: ~Z ticks/s

5. Memory Pool:
   - Allocation speed: ~X ns
   - vs malloc/free: Xx faster
   - Concurrent allocation: ~Y ops/s

6. Socket Operations:
   - Socket creation: ~X ns
   - Loopback latency: ~Y μs
   - Option setting: ~Z ns

Notes:
------
- All benchmarks run on: [System Info]
- CPU: [CPU Model]
- RAM: [RAM Size]
- OS: Linux [Version]
- Compiler: g++ [Version]

See individual JSON files for detailed results.
EOF

echo -e "${GREEN}Summary created: $RESULTS_DIR/SUMMARY.txt${NC}"

