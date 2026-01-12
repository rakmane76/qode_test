#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <cstdint>
#include <vector>
#include <mutex>
#include <unordered_map>

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
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<int, ClientInfo> clients_;
};

} // namespace mdfh

#endif // CLIENT_MANAGER_H
