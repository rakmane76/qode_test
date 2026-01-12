#include "common/latency_tracker.h"
#include <fstream>
#include <cmath>
#include <algorithm>


namespace mdfh {

// Round up to next power of 2
static size_t next_power_of_2(size_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

LatencyTracker::LatencyTracker(size_t max_samples)
    : max_samples_(next_power_of_2(max_samples)),
      index_mask_(max_samples_ - 1),
      write_idx_(0),
      samples_(max_samples_),
      histogram_(NUM_BUCKETS) {
    
    for (auto& bucket : histogram_) {
        bucket.store(0, std::memory_order_relaxed);
    }
}

LatencyTracker::~LatencyTracker() {
}

LatencyStats LatencyTracker::get_stats() const {
    LatencyStats stats{};
    
    size_t current_idx = write_idx_.load(std::memory_order_relaxed);
    size_t num_samples = std::min(current_idx, max_samples_);
    
    if (num_samples == 0) {
        return stats;
    }
    
    stats.sample_count = num_samples;
    
    // Calculate min, max, mean from samples
    uint64_t min_val = UINT64_MAX;
    uint64_t max_val = 0;
    uint64_t sum = 0;
    
    for (size_t i = 0; i < num_samples; ++i) {
        uint64_t sample = samples_[i];
        min_val = std::min(min_val, sample);
        max_val = std::max(max_val, sample);
        sum += sample;
    }
    
    stats.min = min_val;
    stats.max = max_val;
    stats.mean = sum / num_samples;
    
    // Calculate percentiles by sorting samples for accuracy
    std::vector<uint64_t> sorted_samples(samples_.begin(), samples_.begin() + num_samples);
    std::sort(sorted_samples.begin(), sorted_samples.end());
    
    stats.p50 = sorted_samples[static_cast<size_t>(num_samples * 0.50)];
    stats.p95 = sorted_samples[static_cast<size_t>(num_samples * 0.95)];
    stats.p99 = sorted_samples[static_cast<size_t>(num_samples * 0.99)];
    stats.p999 = sorted_samples[static_cast<size_t>(num_samples * 0.999)];
    
    return stats;
}

void LatencyTracker::reset() {
    write_idx_.store(0, std::memory_order_relaxed);
    for (auto& bucket : histogram_) {
        bucket.store(0, std::memory_order_relaxed);
    }
}

bool LatencyTracker::export_to_csv(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << "Bucket,Count\n";
    for (size_t i = 0; i < NUM_BUCKETS; ++i) {
        uint64_t count = histogram_[i].load(std::memory_order_relaxed);
        if (count > 0) {
            file << i << "," << count << "\n";
        }
    }
    
    return true;
}

size_t LatencyTracker::get_bucket(uint64_t latency_ns) const {
    if (latency_ns >= MAX_LATENCY_NS) {
        return NUM_BUCKETS - 1;
    }
    return static_cast<size_t>((latency_ns * NUM_BUCKETS) / MAX_LATENCY_NS);
}

uint64_t LatencyTracker::calculate_percentile(double percentile) const {
    uint64_t total_count = 0;
    for (const auto& bucket : histogram_) {
        total_count += bucket.load(std::memory_order_relaxed);
    }
    
    if (total_count == 0) return 0;
    
    double target_pos = total_count * percentile;
    uint64_t cumulative = 0;
    
    for (size_t i = 0; i < NUM_BUCKETS; ++i) {
        uint64_t bucket_count = histogram_[i].load(std::memory_order_relaxed);
        if (cumulative + bucket_count >= target_pos) {
            // Interpolate within the bucket for better accuracy
            double fraction = (target_pos - cumulative) / std::max(1.0, static_cast<double>(bucket_count));
            uint64_t bucket_start = (i * MAX_LATENCY_NS) / NUM_BUCKETS;
            uint64_t bucket_size = MAX_LATENCY_NS / NUM_BUCKETS;
            return bucket_start + static_cast<uint64_t>(fraction * bucket_size);
        }
        cumulative += bucket_count;
    }
    
    return MAX_LATENCY_NS;
}

} // namespace mdfh
