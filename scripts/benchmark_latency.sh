#!/bin/bash

# Benchmark Latency

set -e

echo "=== Latency Benchmark ==="
echo ""

# Ensure binaries are built
if [ ! -f "build/release/exchange_server" ] || [ ! -f "build/release/feed_client" ]; then
    echo "Building project..."
    ./scripts/build.sh
fi

# Create logs directory
mkdir -p logs benchmarks/results

# Create results file with timestamp
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_FILE="benchmarks/results/latency_${TIMESTAMP}.txt"

echo "Latency Benchmark Results - $(date)" > "$RESULTS_FILE"
echo "======================================" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"

echo "Running latency benchmarks..."
echo "Results will be saved to: $RESULTS_FILE"
echo ""

# Test configurations
CONFIGS=(
    "10000:10:100"      # 10K msgs/sec, 10 symbols
    "50000:50:100"      # 50K msgs/sec, 50 symbols
    "100000:100:100"    # 100K msgs/sec, 100 symbols
    "200000:100:200"    # 200K msgs/sec, 100 symbols
)

for config in "${CONFIGS[@]}"; do
    IFS=':' read -r tick_rate symbols duration <<< "$config"

    echo "Configuration: $tick_rate msgs/sec, $symbols symbols, ${duration}s duration"
    echo "" >> "$RESULTS_FILE"
    echo "Configuration: $tick_rate msgs/sec, $symbols symbols, ${duration}s duration" >> "$RESULTS_FILE"
    echo "-----------------------------------" >> "$RESULTS_FILE"

    # Start server
    ./build/release/exchange_server 9876 $symbols $tick_rate > logs/bench_server.log 2>&1 &
    SERVER_PID=$!

    sleep 1

    # Run client for specified duration
    timeout $duration ./build/feed_client 127.0.0.1 9876 $symbols > logs/bench_client.log 2>&1 || true

    # Stop server
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true

    # Extract final statistics from client log
    if [ -f logs/bench_client.log ]; then
        echo "Results:" >> "$RESULTS_FILE"
        grep -A 3 "Final Statistics:" logs/bench_client.log >> "$RESULTS_FILE" 2>/dev/null || echo "  No statistics found" >> "$RESULTS_FILE"
        
        # Extract parser throughput
        THROUGHPUT=$(grep "Parser Throughput:" logs/bench_client.log | tail -1 | awk '{print $3, $4}')
        if [ -n "$THROUGHPUT" ]; then
            echo "Parser Throughput: $THROUGHPUT" | tee -a "$RESULTS_FILE"
        fi
        
        # Extract latency
        LATENCY=$(grep "Latency - p50:" logs/bench_client.log | tail -1)
        if [ -n "$LATENCY" ]; then
            echo "$LATENCY" | tee -a "$RESULTS_FILE"
        fi
    fi

    echo "" >> "$RESULTS_FILE"
    echo "  Completed"
    echo ""

    sleep 1
done

echo "======================================" >> "$RESULTS_FILE"
echo "Benchmark complete at $(date)" >> "$RESULTS_FILE"

echo ""
echo "Benchmark complete!"
echo "Results saved to: $RESULTS_FILE"
echo "Raw logs in: logs/bench_*.log"
