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

} // namespace mdfh
