# Benchmarks

This directory contains Google Benchmark-based performance tests for the Market Data Feed Handler components.

## Building Benchmarks

```bash
cd scripts
./build.sh --benchmarks
```

Or with CMake directly:

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
make -j$(nproc)
```

## Running Benchmarks

### Run All Benchmarks

```bash
cd scripts
./run_benchmarks.sh
```

This will:
- Run all benchmark executables
- Save results to `benchmarks/benchmark_results/` in JSON format
- Generate a summary report

### Run Individual Benchmarks

```bash
cd build

# Parser performance
./parser_benchmark

# Cache performance
./cache_benchmark

# Latency tracking performance
./latency_benchmark

# Tick generator performance
./tick_generator_benchmark

# Memory pool performance
./memory_pool_benchmark

# Socket operations performance
./socket_benchmark
```

### Benchmark Options

Google Benchmark supports various command-line options:

```bash
# Run specific benchmark
./parser_benchmark --benchmark_filter=BM_ParseTrade

# Run for specific time
./parser_benchmark --benchmark_min_time=5s

# Export to JSON
./parser_benchmark --benchmark_out=results.json --benchmark_out_format=json

# Export to CSV
./parser_benchmark --benchmark_out=results.csv --benchmark_out_format=csv

# Show counters
./parser_benchmark --benchmark_counters_tabular=true

# Verbose output
./parser_benchmark --v=1
```

## Benchmark Components

### 1. parser_benchmark.cpp
Tests message parsing performance:
- Single message parsing (Trade, Quote, Heartbeat)
- Continuous stream parsing
- Fragmented packet handling
- Message validation overhead

**Key Metrics:**
- Messages per second
- Nanoseconds per message
- Bytes processed per second

### 2. cache_benchmark.cpp
Tests lock-free symbol cache performance:
- Single update operations (bid, ask, trade)
- Batch updates (multiple symbols)
- Read operations
- Mixed read/write workloads
- Multi-threaded scenarios

**Key Metrics:**
- Operations per second
- Latency (average, p50, p99)
- Scalability with threads

### 3. latency_benchmark.cpp
Tests latency tracking infrastructure:
- Recording latency samples
- Batch recording
- Statistics calculation (percentiles)
- Concurrent recording
- Histogram export

**Key Metrics:**
- Recording overhead (nanoseconds)
- Statistics calculation time
- Multi-threaded throughput

### 4. tick_generator_benchmark.cpp
Tests market tick generation:
- Geometric Brownian Motion price generation
- Box-Muller transformation
- Quote generation (bid/ask spread)
- Batch generation (multiple symbols)
- Realistic workload (70% quotes, 30% trades)

**Key Metrics:**
- Ticks per second
- Latency per tick
- Impact of volatility

### 5. memory_pool_benchmark.cpp
Tests memory pool allocator:
- Allocation/deallocation speed
- Comparison with malloc/free
- Batch allocation
- Multi-threaded contention
- Different block sizes

**Key Metrics:**
- Operations per second
- Speedup vs standard allocator
- Scalability

### 6. socket_benchmark.cpp
Tests socket operations:
- Socket creation overhead
- Loopback latency
- Buffer size configuration
- Socket option setting
- Non-blocking mode

**Key Metrics:**
- Socket operation latency
- Round-trip time
- Configuration overhead

## Performance Targets

Based on requirements:

| Component | Target | Measured | Status |
|-----------|--------|----------|--------|
| Parse throughput | 100K msg/s | 6.8M msg/s | ✓ 68x |
| Cache read latency | <50 ns | 8 ns | ✓ 6x better |
| Cache write | - | 54 ns | ✓ |
| Latency tracking overhead | <30 ns | 26 ns | ✓ |
| Tick generation | 500K/s | 85M/s | ✓ 170x |

See [BENCHMARK_RESULTS.md](./BENCHMARK_RESULTS.md) for detailed analysis.

## Interpreting Results

### Throughput Benchmarks

```
Benchmark                      Time             CPU   Iterations
-----------------------------------------------------------------
BM_ParseTrade                117 ns          117 ns      5987234
```

- **Time/CPU**: Average time per iteration
- **Iterations**: Number of times the benchmark ran
- **Throughput**: 1 / 117ns ≈ 8.5M ops/sec

### Latency Distribution

For multi-threaded benchmarks, Google Benchmark shows per-thread statistics. Cross-reference with `latency_benchmark` for percentile data (p50, p95, p99, p999).

### Scalability

Thread count is shown in the benchmark name:

```
BM_CacheMultiThreadRead/threads:1    10 ns
BM_CacheMultiThreadRead/threads:4    12 ns  (near-linear scaling)
```

## System Configuration

For accurate benchmarks:

1. **Disable CPU frequency scaling:**
   ```bash
   sudo cpupower frequency-set -g performance
   ```

2. **Isolate CPU cores (optional):**
   ```bash
   # Edit /etc/default/grub
   GRUB_CMDLINE_LINUX="isolcpus=0-3"
   sudo update-grub && sudo reboot
   ```

3. **Set CPU affinity:**
   ```bash
   taskset -c 0-3 ./parser_benchmark
   ```

4. **Close background apps** to minimize noise

5. **Run multiple times** for statistical confidence

## Analyzing Results

### Compare Before/After

```bash
# Baseline
./parser_benchmark --benchmark_out=baseline.json --benchmark_out_format=json

# After optimization
./parser_benchmark --benchmark_out=optimized.json --benchmark_out_format=json

# Compare
python3 -m pip install scipy
python3 compare.py baseline.json optimized.json
```

### Export to Spreadsheet

```bash
./parser_benchmark --benchmark_out=results.csv --benchmark_out_format=csv
# Open results.csv in Excel/LibreOffice
```

### Flame Graphs (Advanced)

```bash
# Install perf
sudo apt-get install linux-tools-common linux-tools-generic

# Record
sudo perf record -F 999 -g ./parser_benchmark --benchmark_min_time=10s

# Generate flame graph
git clone https://github.com/brendangregg/FlameGraph
sudo perf script | FlameGraph/stackcollapse-perf.pl | FlameGraph/flamegraph.pl > flame.svg
```

## Continuous Integration

Add to CI pipeline:

```yaml
# .github/workflows/benchmark.yml
- name: Build Benchmarks
  run: |
    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
    make -j$(nproc)

- name: Run Benchmarks
  run: |
    cd build
    ./parser_benchmark --benchmark_format=json > parser_results.json
    
- name: Upload Results
  uses: actions/upload-artifact@v2
  with:
    name: benchmark-results
    path: build/*_results.json
```

## Contributing

When adding new benchmarks:

1. Follow the naming convention: `BM_ComponentName_Operation`
2. Use `benchmark::DoNotOptimize()` to prevent optimization
3. Use `benchmark::ClobberMemory()` for writes
4. Add to `CMakeLists.txt`
5. Update this README with description
6. Document expected performance in BENCHMARK_RESULTS.md

## References

- [Google Benchmark Documentation](https://github.com/google/benchmark)
- [Quick Start Guide](https://github.com/google/benchmark#getting-started)
- [User Guide](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
