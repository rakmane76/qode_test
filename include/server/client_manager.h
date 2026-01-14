#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <cstdint>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace mdfh {

struct ClientInfo {
    int fd;
    uint64_t messages_sent;
    uint64_t bytes_sent;
    uint64_t send_errors;
    bool is_slow;
};

class ClientManager {
public:
    ClientManager();
    ~ClientManager();
    
    // Add new client
    void add_client(int fd);
    
    // Remove client
    void remove_client(int fd);
    
    // Get all client file descriptors
    std::vector<int> get_all_clients() const;
    
    // Mark client as slow
    void mark_slow_client(int fd);
    
    // Update statistics
    void update_stats(int fd, size_t bytes_sent, bool success);
    
    // Get client info
    ClientInfo get_client_info(int fd) const;
    
    // Get total number of clients
    size_t get_client_count() const;
    
    // Subscription management
    void subscribe(int fd, const std::unordered_set<uint16_t>& symbol_ids);
    void unsubscribe(int fd, uint16_t symbol_id);
    void clear_subscriptions(int fd);
    bool is_subscribed(int fd, uint16_t symbol_id) const;
    size_t get_subscription_count(int fd) const;
    std::vector<int> get_subscribed_clients(uint16_t symbol_id) const;
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<int, ClientInfo> clients_;
    std::unordered_map<int, std::unordered_set<uint16_t>> subscriptions_;
};

} // namespace mdfh

#endif // CLIENT_MANAGER_H
