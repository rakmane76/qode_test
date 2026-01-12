# Geometric Brownian Motion (GBM) for Market Data Simulation

## 1. Mathematical Background

### 1.1 Stochastic Differential Equation

Geometric Brownian Motion models stock price evolution as:

```
dS(t) = μ S(t) dt + σ S(t) dW(t)
```

**Components:**
- `S(t)` = Stock price at time t
- `μ` = Drift coefficient (expected return rate)
- `σ` = Volatility coefficient (standard deviation of returns)
- `dt` = Infinitesimal time increment
- `dW(t)` = Wiener process increment (Brownian motion)

### 1.2 Why GBM for Stock Prices?

**Advantages:**
1. **Positivity**: S(t) > 0 always (prices can't go negative)
2. **Log-normal distribution**: Returns are normally distributed
3. **Multiplicative dynamics**: Returns are independent of price level
4. **Realistic volatility**: Captures market uncertainty

**Comparison with Arithmetic Brownian Motion:**

| Property | GBM | Arithmetic BM |
|----------|-----|---------------|
| Can go negative? | No ✓ | Yes ✗ |
| Volatility scales with price? | Yes ✓ | No ✗ |
| Used in finance? | Yes ✓ | Rarely |

### 1.3 Solution to GBM SDE

The analytical solution is:

```
S(t) = S(0) * exp((μ - σ²/2)t + σW(t))
```

Where `W(t)` is standard Brownian motion: `W(t) ~ N(0, t)`

**Returns are log-normal:**
```
ln(S(t)/S(0)) ~ N((μ - σ²/2)t, σ²t)
```

## 2. Discretization for Simulation

### 2.1 Euler-Maruyama Method

For numerical simulation with discrete time steps Δt:

```
S(t + Δt) = S(t) + μ S(t) Δt + σ S(t) √Δt * Z
```

Where `Z ~ N(0,1)` is a standard normal random variable.

**In code:**
```cpp
double drift_component = mu * S * dt;
double diffusion_component = sigma * S * sqrt(dt) * Z;
S_new = S + drift_component + diffusion_component;
```

### 2.2 Time Step Selection

**Trade-off:**
- Large Δt: Faster simulation, less accurate
- Small Δt: Slower simulation, more accurate

For our feed handler:
- **Δt = 0.001** (1 millisecond in normalized time)
- Simulates high-frequency tick data
- Balances realism and computational cost

### 2.3 Alternative: Exact Simulation

For better accuracy:

```
S(t + Δt) = S(t) * exp((μ - σ²/2)Δt + σ√Δt * Z)
```

**Advantages:**
- Mathematically exact solution
- Always positive
- No discretization error

**Disadvantages:**
- More computationally expensive (exp function)
- Overkill for our simulation needs

## 3. Box-Muller Transform

### 3.1 Algorithm

Generate two independent uniform random variables U₁, U₂ ~ Uniform(0,1):

```
R = sqrt(-2 ln(U₁))
θ = 2π U₂

Z₀ = R cos(θ)
Z₁ = R sin(θ)
```

Both Z₀ and Z₁ are independent N(0,1) random variables.

### 3.2 Implementation

```cpp
double TickGenerator::generate_normal() {
    // Use cached value if available
    if (has_spare_normal_) {
        has_spare_normal_ = false;
        return spare_normal_;
    }
    
    double u1, u2;
    do {
        u1 = uniform_(rng_);
    } while (u1 == 0.0);  // Avoid log(0)
    
    u2 = uniform_(rng_);
    
    double R = sqrt(-2.0 * log(u1));
    double theta = 2.0 * M_PI * u2;
    
    double z0 = R * cos(theta);
    double z1 = R * sin(theta);
    
    // Cache second value
    spare_normal_ = z1;
    has_spare_normal_ = true;
    
    return z0;
}
```

### 3.3 Why Box-Muller?

**Alternatives:**

| Method | Speed | Quality | Complexity |
|--------|-------|---------|------------|
| Box-Muller | Medium | Excellent | Low |
| Ziggurat | Fast | Excellent | High |
| Central Limit Theorem | Slow | Poor | Low |
| Inverse CDF | Very Slow | Excellent | Low |

Box-Muller chosen for:
- Good balance of speed and simplicity
- Exact normal distribution
- Easy to understand and verify

## 4. Parameter Selection

### 4.1 Drift (μ)

**Physical meaning:** Expected return rate

**Typical values:**
- Bull market: μ = +0.05 (5% annual return)
- Neutral market: μ = 0.0 (no trend)
- Bear market: μ = -0.05 (-5% annual return)

**Our simulation:**
- Randomize μ ∈ [-0.05, +0.05] per symbol
- Creates mix of rising, falling, and stable stocks

### 4.2 Volatility (σ)

**Physical meaning:** Standard deviation of returns

**Typical annual volatilities:**
- Large-cap stable stocks: σ = 0.15 (15%)
- Mid-cap stocks: σ = 0.25 (25%)
- Small-cap/tech stocks: σ = 0.40 (40%)
- Cryptocurrencies: σ = 1.00 (100%+)

**Our simulation:**
- σ ∈ [0.01, 0.06] (normalized for dt=0.001)
- Creates variety of volatility profiles

**Scaling for time:**
- Daily volatility: σ_day = σ_annual / √252
- Tick volatility: σ_tick = σ_annual / √(252 * ticks_per_day)

### 4.3 Starting Prices

**Distribution:**
```
Price ∈ [₹100, ₹5000]
```

Chosen to reflect Indian stock market:
- Small caps: ₹100 - ₹500
- Mid caps: ₹500 - ₹1500
- Large caps: ₹1500 - ₹5000

## 5. Bid-Ask Spread Generation

### 5.1 Spread Model

The bid-ask spread represents market microstructure:

```
spread = price * spread_percentage
spread_percentage ∈ [0.05%, 0.2%]

bid = mid_price - spread/2
ask = mid_price + spread/2
```

### 5.2 Why This Range?

**Real-world spreads:**
- Highly liquid stocks (RELIANCE, TCS): ~0.01% - 0.05%
- Medium liquidity: ~0.05% - 0.15%
- Low liquidity: ~0.15% - 0.50%+

Our range (0.05% - 0.2%) captures medium-to-low liquidity stocks.

### 5.3 Implementation

```cpp
double TickGenerator::generate_spread(double price) {
    double min_pct = 0.0005;  // 0.05%
    double max_pct = 0.002;   // 0.20%
    
    double pct = min_pct + uniform_(rng_) * (max_pct - min_pct);
    return price * pct;
}
```

## 6. Volume Generation

### 6.1 Log-Uniform Distribution

Volumes follow log-uniform distribution:

```
log₁₀(volume) ∈ [2, 5]
volume ∈ [100, 100,000]
```

**Rationale:**
- Small orders (100-1000): Common retail trades
- Medium orders (1000-10000): Institutional
- Large orders (10000-100000): Block trades

### 6.2 Implementation

```cpp
uint32_t TickGenerator::generate_volume() {
    double log_volume = 2.0 + uniform_(rng_) * 3.0;
    return static_cast<uint32_t>(pow(10.0, log_volume));
}
```

## 7. Message Type Distribution

### 7.1 Quote vs Trade Ratio

**Real markets:**
- Quotes: 70-90% of messages
- Trades: 10-30% of messages

**Our simulation:** 70% quotes, 30% trades

### 7.2 Implementation

```cpp
bool TickGenerator::should_generate_quote() {
    return uniform_(rng_) < 0.7;
}
```

## 8. Realism Considerations

### 8.1 Price Discretization

**Real stocks have tick sizes:**
- ₹0.05 for prices < ₹1000
- ₹0.10 for prices ≥ ₹1000

**Optional enhancement:**
```cpp
double round_to_tick(double price) {
    double tick = (price < 1000.0) ? 0.05 : 0.10;
    return round(price / tick) * tick;
}
```

### 8.2 Volume Clustering

Real volumes cluster around round numbers (100, 500, 1000).

**Optional enhancement:**
```cpp
uint32_t cluster_volume(uint32_t vol) {
    uint32_t buckets[] = {100, 200, 500, 1000, 2000, 5000, 10000};
    // Find nearest bucket
}
```

### 8.3 Correlation (Advanced)

Real stocks are correlated (market movements affect all stocks).

**Multivariate GBM:**
```
dS₁ = μ₁ S₁ dt + σ₁ S₁ dW₁
dS₂ = μ₂ S₂ dt + σ₂ S₂ (ρ dW₁ + √(1-ρ²) dW₂)
```

Where ρ is correlation coefficient.

**Not implemented** for simplicity, but could be added.

## 9. Validation

### 9.1 Statistical Tests

**Mean return:**
```
E[S(T)/S(0)] ≈ exp(μT)
```

**Variance of log returns:**
```
Var[ln(S(T)/S(0))] ≈ σ²T
```

### 9.2 Simulation Code

```python
import numpy as np
import matplotlib.pyplot as plt

# Parameters
S0 = 1000
mu = 0.05
sigma = 0.02
dt = 0.001
T = 1.0
N = int(T / dt)

# Simulate
S = [S0]
for i in range(N):
    Z = np.random.normal()
    dS = mu * S[-1] * dt + sigma * S[-1] * np.sqrt(dt) * Z
    S.append(S[-1] + dS)

plt.plot(S)
plt.title('GBM Simulation')
plt.show()
```

### 9.3 Expected Properties

After 1000 time steps (T = 1.0):
- Mean price: ~₹1000 * exp(0.05) ≈ ₹1051
- ~68% of paths within [₹980, ₹1020] (1σ)
- ~95% of paths within [₹960, ₹1040] (2σ)

## 10. Alternatives to GBM

### 10.1 Heston Model (Stochastic Volatility)

```
dS = μS dt + √v S dW₁
dv = κ(θ - v) dt + ξ√v dW₂
```

- More realistic (volatility clustering)
- Much more complex to implement

### 10.2 Jump Diffusion (Merton Model)

```
dS = μS dt + σS dW + S dJ
```

- Models sudden price jumps (news events)
- Requires Poisson process for jumps

### 10.3 Ornstein-Uhlenbeck (Mean Reversion)

```
dS = θ(μ - S) dt + σ dW
```

- Mean-reverting (good for FX, interest rates)
- Not suitable for stocks (can go negative)

## 11. Summary

**GBM is chosen because:**
1. Simple to implement and understand
2. Captures essential stock price dynamics
3. Always produces positive prices
4. Well-studied and validated model
5. Good enough for feed handler testing

**Key parameters:**
- Time step: dt = 0.001
- Drift: μ ∈ [-0.05, +0.05]
- Volatility: σ ∈ [0.01, 0.06]
- Spread: 0.05% - 0.2%

**Implementation:**
- Box-Muller for normal random variables
- Euler-Maruyama discretization
- 70% quotes, 30% trades
- Log-uniform volume distribution

This provides realistic-looking market data suitable for testing high-performance feed handlers without needing real exchange connectivity.
