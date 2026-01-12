#include <benchmark/benchmark.h>
#include "server/tick_generator.h"
#include <random>

using namespace mdfh;

// Benchmark: Generate single tick
static void BM_GenerateTick(benchmark::State& state) {
    TickGenerator generator;
    double current_price = 2450.0;
    double drift = 0.0;
    double volatility = 0.03;
    double dt = 0.001;
    
    for (auto _ : state) {
        double price = generator.generate_next_price(current_price, drift, volatility, dt);
        benchmark::DoNotOptimize(price);
        current_price = price;
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GenerateTick);

// Benchmark: Box-Muller transformation
static void BM_BoxMuller(benchmark::State& state) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    for (auto _ : state) {
        double u1 = dist(rng);
        double u2 = dist(rng);
        double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
        benchmark::DoNotOptimize(z);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_BoxMuller);

// Benchmark: Generate spread
static void BM_GenerateSpread(benchmark::State& state) {
    TickGenerator generator;
    double price = 2450.0;
    
    for (auto _ : state) {
        double spread = generator.generate_spread(price);
        benchmark::DoNotOptimize(spread);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GenerateSpread);

// Benchmark: Batch tick generation (multiple symbols)
static void BM_GenerateBatchTicks(benchmark::State& state) {
    int num_symbols = state.range(0);
    TickGenerator generator;
    std::vector<double> prices;
    std::vector<double> volatilities;
    
    for (int i = 0; i < num_symbols; ++i) {
        prices.push_back(1000.0 + i * 50.0);
        volatilities.push_back(0.02 + (i % 3) * 0.01);
    }
    
    for (auto _ : state) {
        for (int i = 0; i < num_symbols; ++i) {
            prices[i] = generator.generate_next_price(prices[i], 0.0, volatilities[i], 0.001);
            benchmark::DoNotOptimize(prices[i]);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * num_symbols);
}
BENCHMARK(BM_GenerateBatchTicks)->Range(10, 500);

// Benchmark: Price with different volatility levels
static void BM_VolatilityImpact(benchmark::State& state) {
    double volatility = state.range(0) / 100.0;  // Convert to decimal
    TickGenerator generator;
    double current_price = 2450.0;
    
    for (auto _ : state) {
        current_price = generator.generate_next_price(current_price, 0.0, volatility, 0.001);
        benchmark::DoNotOptimize(current_price);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.SetLabel("σ=" + std::to_string(volatility));
}
BENCHMARK(BM_VolatilityImpact)->Arg(1)->Arg(3)->Arg(5)->Arg(10);

// Benchmark: Trade generation with volume
static void BM_GenerateTrade(benchmark::State& state) {
    TickGenerator generator;
    double current_price = 2450.0;
    
    for (auto _ : state) {
        current_price = generator.generate_next_price(current_price, 0.0, 0.03, 0.001);
        uint32_t volume = generator.generate_volume();
        benchmark::DoNotOptimize(current_price);
        benchmark::DoNotOptimize(volume);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GenerateTrade);

// Benchmark: Realistic market tick rate (100K ticks/sec)
static void BM_RealisticTickRate(benchmark::State& state) {
    TickGenerator generator;
    std::vector<double> prices;
    std::vector<double> volatilities;
    
    // 100 symbols
    for (int i = 0; i < 100; ++i) {
        prices.push_back(1000.0 + i * 50.0);
        volatilities.push_back(0.02 + (i % 5) * 0.01);
    }
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> symbol_dist(0, 99);
    
    for (auto _ : state) {
        int symbol_id = symbol_dist(rng);
        
        if (generator.should_generate_quote()) {
            // Generate quote
            double mid_price = generator.generate_next_price(prices[symbol_id], 0.0, volatilities[symbol_id], 0.001);
            double spread = generator.generate_spread(mid_price);
            double bid = mid_price - spread / 2.0;
            double ask = mid_price + spread / 2.0;
            prices[symbol_id] = mid_price;
            benchmark::DoNotOptimize(bid);
            benchmark::DoNotOptimize(ask);
        } else {
            // Generate trade
            double price = generator.generate_next_price(prices[symbol_id], 0.0, volatilities[symbol_id], 0.001);
            uint32_t volume = generator.generate_volume();
            prices[symbol_id] = price;
            benchmark::DoNotOptimize(price);
            benchmark::DoNotOptimize(volume);
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RealisticTickRate);

BENCHMARK_MAIN();
