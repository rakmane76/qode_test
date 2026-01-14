#include "server/client_manager.h"

namespace mdfh {

ClientManager::ClientManager() {
}

ClientManager::~ClientManager() {
}

void ClientManager::add_client(int fd) {
    std::scoped_lock lock(mutex_);
    
    ClientInfo info{};
    info.fd = fd;
    info.messages_sent = 0;
    info.bytes_sent = 0;
    info.send_errors = 0;
    info.is_slow = false;
    
    clients_[fd] = info;
}

void ClientManager::remove_client(int fd) {
    std::scoped_lock lock(mutex_);
    clients_.erase(fd);
    subscriptions_.erase(fd);  // Also clear subscriptions when removing client
}

std::vector<int> ClientManager::get_all_clients() const {
    std::scoped_lock lock(mutex_);
    
    std::vector<int> fds;
    fds.reserve(clients_.size());
    
    for (const auto& pair : clients_) {
        fds.push_back(pair.first);
    }
    
    return fds;
}

void ClientManager::mark_slow_client(int fd) {
    std::scoped_lock lock(mutex_);
    
    auto it = clients_.find(fd);
    if (it != clients_.end()) {
        it->second.is_slow = true;
    }
}

void ClientManager::update_stats(int fd, size_t bytes_sent, bool success) {
    std::scoped_lock lock(mutex_);
    
    auto it = clients_.find(fd);
    if (it != clients_.end()) {
        if (success) {
            it->second.messages_sent++;
            it->second.bytes_sent += bytes_sent;
        } else {
            it->second.send_errors++;
        }
    }
}

ClientInfo ClientManager::get_client_info(int fd) const {
    std::scoped_lock lock(mutex_);
    
    auto it = clients_.find(fd);
    if (it != clients_.end()) {
        return it->second;
    }
    
    return ClientInfo{};
}

size_t ClientManager::get_client_count() const {
    std::scoped_lock lock(mutex_);
    return clients_.size();
}

void ClientManager::subscribe(int fd, const std::unordered_set<uint16_t>& symbol_ids) {
    std::scoped_lock lock(mutex_);
    subscriptions_[fd] = symbol_ids;
}

void ClientManager::unsubscribe(int fd, uint16_t symbol_id) {
    std::scoped_lock lock(mutex_);
    auto it = subscriptions_.find(fd);
    if (it != subscriptions_.end()) {
        it->second.erase(symbol_id);
    }
}

void ClientManager::clear_subscriptions(int fd) {
    std::scoped_lock lock(mutex_);
    subscriptions_.erase(fd);
}

bool ClientManager::is_subscribed(int fd, uint16_t symbol_id) const {
    std::scoped_lock lock(mutex_);
    auto it = subscriptions_.find(fd);
    if (it == subscriptions_.end()) {
        return false;
    }
    return it->second.find(symbol_id) != it->second.end();
}

size_t ClientManager::get_subscription_count(int fd) const {
    std::scoped_lock lock(mutex_);
    auto it = subscriptions_.find(fd);
    if (it == subscriptions_.end()) {
        return 0;
    }
    return it->second.size();
}

std::vector<int> ClientManager::get_subscribed_clients(uint16_t symbol_id) const {
    std::scoped_lock lock(mutex_);
    std::vector<int> result;
    
    for (const auto& [fd, symbols] : subscriptions_) {
        if (symbols.find(symbol_id) != symbols.end()) {
            result.push_back(fd);
        }
    }
    
    return result;
}

} // namespace mdfh
