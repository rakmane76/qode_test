#ifndef LATENCY_TRACKER_H
#define LATENCY_TRACKER_H

#include <cstdint>
#include <atomic>
#include <vector>
#include <algorithm>
#include <mutex>
#include <string>

namespace mdfh {

struct LatencyStats {
    uint64_t min;
    uint64_t max;
    uint64_t mean;
    uint64_t p50;
    uint64_t p95;
    uint64_t p99;
    uint64_t p999;
    uint64_t sample_count;
};

class LatencyTracker {
public:
    LatencyTracker(size_t max_samples = 1000000);
    ~LatencyTracker();
    
    // Record a latency sample (in nanoseconds)
    inline void record(uint64_t latency_ns) {
        // Ring buffer for samples - use bitwise AND for fast indexing
        size_t idx = write_idx_.fetch_add(1, std::memory_order_relaxed) & index_mask_;
        samples_[idx] = latency_ns;
    }
    
    // Get statistics
    LatencyStats get_stats() const;
    
    // Reset all samples
    void reset();
    
    // Export to CSV
    bool export_to_csv(const std::string& filename) const;
    
private:
    static constexpr size_t NUM_BUCKETS = 1000;
    static constexpr uint64_t MAX_LATENCY_NS = 10000000; // 10ms
    
    size_t max_samples_;
    size_t index_mask_; // For fast modulo with power-of-2 sizes
    std::atomic<size_t> write_idx_;
    std::vector<uint64_t> samples_;
    
    // Histogram for fast percentile calculation
    mutable std::mutex histogram_mutex_;
    std::vector<std::atomic<uint64_t>> histogram_;
    
    size_t get_bucket(uint64_t latency_ns) const;
    uint64_t calculate_percentile(double percentile) const;
};

} // namespace mdfh

#endif // LATENCY_TRACKER_H
