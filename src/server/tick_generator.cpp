#include "server/tick_generator.h"
#include <cmath>
#include <chrono>

namespace mdfh {

TickGenerator::TickGenerator() 
    : random_num_gen_(std::random_device{}()),
      uniform_(0.0, 1.0),
      has_spare_normal_(false),
      spare_normal_(0.0) {
}

double TickGenerator::generate_next_price(double current_price, double drift,
                                           double volatility, double dt) {
    // dS = μ * S * dt + σ * S * dW
    // where dW = sqrt(dt) * N(0,1)
    
    double normal = generate_normal();
    double drift_component = drift * current_price * dt;
    double diffusion_component = volatility * current_price * std::sqrt(dt) * normal;
    
    double new_price = current_price + drift_component + diffusion_component;
    
    // Ensure price stays positive
    if (new_price < 0.0) {
        new_price = 0.1;
    }
    
    return new_price;
}

double TickGenerator::generate_spread(double price) {
    // Spread between 0.05% and 0.2% of price
    double min_spread_pct = 0.0005;
    double max_spread_pct = 0.002;
    
    double spread_pct = min_spread_pct + 
                        uniform_(random_num_gen_) * (max_spread_pct - min_spread_pct);
    
    return price * spread_pct;
}

uint32_t TickGenerator::generate_volume() {
    // Generate volume between 100 and 100,000
    double log_volume = 2.0 + uniform_(random_num_gen_) * 3.0; // log10(100) to log10(100000)
    return static_cast<uint32_t>(std::pow(10.0, log_volume));
}

bool TickGenerator::should_generate_quote() {
    // 70% chance of generating quote, 30% trade
    return uniform_(random_num_gen_) < 0.7;
}

double TickGenerator::generate_normal() {
    // Box-Muller transform
    if (has_spare_normal_) {
        has_spare_normal_ = false;
        return spare_normal_;
    }
    
    double u1, u2;
    do {
        u1 = uniform_(random_num_gen_);
    } while (u1 == 0.0); // Avoid log(0)
    
    u2 = uniform_(random_num_gen_);
    
    // Box-Muller: radius_factor = sqrt(-2*ln(u1)), then convert to Cartesian coordinates
    double radius_factor = std::sqrt(-2.0 * std::log(u1));
    double z0 = radius_factor * std::cos(2.0 * M_PI * u2);
    double z1 = radius_factor * std::sin(2.0 * M_PI * u2);
    
    spare_normal_ = z1;
    has_spare_normal_ = true;
    
    return z0;
}

} // namespace mdfh
