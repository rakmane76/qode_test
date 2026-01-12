#include <gtest/gtest.h>
#include "server/tick_generator.h"
#include <cmath>
#include <vector>

using namespace mdfh;

class TickGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        generator = std::make_unique<TickGenerator>();
    }

    std::unique_ptr<TickGenerator> generator;
};

// ==================== Price Generation Tests ====================

TEST_F(TickGeneratorTest, GenerateNextPrice_BasicBounds) {
    double initial_price = 1000.0;
    double drift = 0.0;
    double volatility = 0.03;
    double dt = 0.001;
    
    double new_price = generator->generate_next_price(initial_price, drift, volatility, dt);
    
    // Price should be positive and within reasonable bounds
    EXPECT_GT(new_price, 0.0);
    EXPECT_GT(new_price, initial_price * 0.8); // Not too far down
    EXPECT_LT(new_price, initial_price * 1.2); // Not too far up
}

TEST_F(TickGeneratorTest, GenerateNextPrice_PositiveDrift) {
    double initial_price = 1000.0;
    double drift = 0.05;  // Positive drift
    double volatility = 0.01;
    double dt = 0.001;
    
    // Generate many prices to see trend
    double sum = 0;
    double current_price = initial_price;
    int iterations = 1000;
    
    for (int i = 0; i < iterations; i++) {
        current_price = generator->generate_next_price(current_price, drift, volatility, dt);
        sum += current_price;
    }
    
    double avg_price = sum / iterations;
    // With positive drift, average should trend upward from initial
    EXPECT_GT(avg_price, initial_price * 0.95);
}

TEST_F(TickGeneratorTest, GenerateNextPrice_NegativeDrift) {
    double initial_price = 1000.0;
    double drift = -0.05;  // Negative drift
    double volatility = 0.01;
    double dt = 0.001;
    
    // Generate many prices to see trend
    double sum = 0;
    double current_price = initial_price;
    int iterations = 1000;
    
    for (int i = 0; i < iterations; i++) {
        current_price = generator->generate_next_price(current_price, drift, volatility, dt);
        sum += current_price;
    }
    
    double avg_price = sum / iterations;
    // With negative drift, average should trend downward from initial
    EXPECT_LT(avg_price, initial_price * 1.05);
}

TEST_F(TickGeneratorTest, GenerateNextPrice_StaysPositive) {
    double current_price = 100.0;
    double drift = -0.05;
    double volatility = 0.06;  // High volatility
    double dt = 0.001;
    
    // Generate many prices
    for (int i = 0; i < 10000; i++) {
        current_price = generator->generate_next_price(current_price, drift, volatility, dt);
        EXPECT_GT(current_price, 0.0) << "Price became negative at iteration " << i;
    }
}

TEST_F(TickGeneratorTest, GenerateNextPrice_VolatilityAffectsVariance) {
    // Low volatility
    TickGenerator gen_low;
    double low_initial = 1000.0;
    double low_drift = 0.0;
    double low_vol = 0.01;
    
    // High volatility
    TickGenerator gen_high;
    double high_initial = 1000.0;
    double high_drift = 0.0;
    double high_vol = 0.06;
    
    double dt = 0.001;
    int iterations = 1000;
    
    std::vector<double> prices_low, prices_high;
    double current_low = low_initial;
    double current_high = high_initial;
    
    // Generate prices
    for (int i = 0; i < iterations; i++) {
        current_low = gen_low.generate_next_price(current_low, low_drift, low_vol, dt);
        current_high = gen_high.generate_next_price(current_high, high_drift, high_vol, dt);
        
        prices_low.push_back(current_low);
        prices_high.push_back(current_high);
    }
    
    // Calculate means
    double sum_low = 0, sum_high = 0;
    for (int i = 0; i < iterations; i++) {
        sum_low += prices_low[i];
        sum_high += prices_high[i];
    }
    double mean_low = sum_low / iterations;
    double mean_high = sum_high / iterations;
    
    // Calculate variances
    double variance_low = 0, variance_high = 0;
    for (int i = 0; i < iterations; i++) {
        variance_low += std::pow(prices_low[i] - mean_low, 2);
        variance_high += std::pow(prices_high[i] - mean_high, 2);
    }
    variance_low /= iterations;
    variance_high /= iterations;
    
    // High volatility should have higher variance
    EXPECT_GT(variance_high, variance_low);
}

TEST_F(TickGeneratorTest, PriceStability_SingleStepChange) {
    double initial_price = 1500.0;
    double drift = 0.0;
    double volatility = 0.02;
    double dt = 0.001;
    
    // Run many iterations
    double current_price = initial_price;
    for (int i = 0; i < 1000; ++i) {
        double new_price = generator->generate_next_price(current_price, drift, volatility, dt);
        
        // Single step change should be small
        double change_ratio = std::abs(new_price - current_price) / current_price;
        EXPECT_LT(change_ratio, 0.05) << "Price changed too much in single step at iteration " << i;
        
        current_price = new_price;
    }
}

// ==================== Spread Generation Tests ====================

TEST_F(TickGeneratorTest, GenerateSpread_BasicBounds) {
    double price = 2450.0;
    double spread = generator->generate_spread(price);
    
    // Spread should be 0.05% - 0.2% of price
    EXPECT_GE(spread, price * 0.0005);
    EXPECT_LE(spread, price * 0.002);
}

TEST_F(TickGeneratorTest, GenerateSpread_ScalesWithPrice) {
    std::vector<double> prices = {100.0, 1000.0, 5000.0, 10000.0};
    
    for (double price : prices) {
        double spread = generator->generate_spread(price);
        
        // Spread should scale proportionally with price
        double spread_pct = spread / price;
        EXPECT_GE(spread_pct, 0.0005);
        EXPECT_LE(spread_pct, 0.002);
    }
}

// ==================== Volume Generation Tests ====================

TEST_F(TickGeneratorTest, GenerateVolume_BasicBounds) {
    for (int i = 0; i < 100; ++i) {
        uint32_t volume = generator->generate_volume();
        EXPECT_GT(volume, 0);
        EXPECT_LT(volume, 10000000); // Reasonable upper bound
    }
}

TEST_F(TickGeneratorTest, GenerateVolume_Distribution) {
    std::vector<uint32_t> volumes;
    for (int i = 0; i < 1000; ++i) {
        volumes.push_back(generator->generate_volume());
    }
    
    // Calculate mean
    uint64_t sum = 0;
    for (auto vol : volumes) {
        sum += vol;
    }
    double mean = static_cast<double>(sum) / volumes.size();
    
    // Mean should be reasonable
    EXPECT_GT(mean, 0);
    EXPECT_LT(mean, 5000000);
}

// ==================== Message Type Tests ====================

TEST_F(TickGeneratorTest, ShouldGenerateQuote_Distribution) {
    int quote_count = 0;
    int trade_count = 0;
    
    for (int i = 0; i < 1000; ++i) {
        bool is_quote = generator->should_generate_quote();
        if (is_quote) {
            quote_count++;
        } else {
            trade_count++;
        }
    }
    
    // Should be approximately 70% quotes, 30% trades
    double quote_ratio = static_cast<double>(quote_count) / 1000.0;
    EXPECT_NEAR(quote_ratio, 0.70, 0.1);
}

TEST_F(TickGeneratorTest, ShouldGenerateQuote_ReturnsBool) {
    bool saw_true = false;
    bool saw_false = false;
    
    for (int i = 0; i < 100; ++i) {
        bool result = generator->should_generate_quote();
        if (result) saw_true = true;
        else saw_false = true;
        
        if (saw_true && saw_false) break;
    }
    
    EXPECT_TRUE(saw_true) << "should_generate_quote never returned true";
    EXPECT_TRUE(saw_false) << "should_generate_quote never returned false";
}

// ==================== Randomness Tests ====================

TEST_F(TickGeneratorTest, MultipleGenerators_ProduceDifferentResults) {
    TickGenerator gen1;
    TickGenerator gen2;
    
    double initial_price = 1000.0;
    double drift = 0.0;
    double volatility = 0.02;
    double dt = 0.001;
    
    bool found_difference = false;
    for (int i = 0; i < 100; i++) {
        double price1 = gen1.generate_next_price(initial_price, drift, volatility, dt);
        double price2 = gen2.generate_next_price(initial_price, drift, volatility, dt);
        
        if (std::abs(price1 - price2) > 0.01) {
            found_difference = true;
            break;
        }
    }
    
    EXPECT_TRUE(found_difference) << "Two generators produced identical results";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

