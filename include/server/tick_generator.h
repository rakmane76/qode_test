#ifndef TICK_GENERATOR_H
#define TICK_GENERATOR_H

#include <cstdint>
#include <random>

namespace mdfh {

class TickGenerator {
public:
    TickGenerator();
    
    // Generate next price using Geometric Brownian Motion
    // dS = μ * S * dt + σ * S * dW
    double generate_next_price(double current_price, double drift, 
                               double volatility, double dt);
    
    // Generate bid-ask spread (0.05% - 0.2% of price)
    double generate_spread(double price);
    
    // Generate random volume
    uint32_t generate_volume();
    
    // Decide if next message should be quote or trade (70% quote, 30% trade)
    bool should_generate_quote();
    
private:
    // Box-Muller transform for normal distribution
    double generate_normal();
    
    std::mt19937_64 random_num_gen_;
    std::uniform_real_distribution<double> uniform_;
    bool has_spare_normal_;
    double spare_normal_;
};

} // namespace mdfh

#endif // TICK_GENERATOR_H
