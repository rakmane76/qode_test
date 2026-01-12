#include <benchmark/benchmark.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <cstring>
#include <thread>
#include <chrono>

// Benchmark: Socket creation and configuration
static void BM_SocketCreation(benchmark::State& state) {
    for (auto _ : state) {
        int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        benchmark::DoNotOptimize(sock_fd);
        
        // Set TCP_NODELAY
        int flag = 1;
        setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        
        // Set non-blocking
        int flags = fcntl(sock_fd, F_GETFL, 0);
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
        
        close(sock_fd);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SocketCreation);

// Benchmark: Send/Recv latency (loopback)
static void BM_LoopbackLatency(benchmark::State& state) {
    // Create server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int flag = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(19876);
    
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 1);
    
    // Create client in separate thread
    std::thread server_thread([server_fd]() {
        int client_fd = accept(server_fd, nullptr, nullptr);
        char buffer[1024];
        while (true) {
            ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
            if (n <= 0) break;
            send(client_fd, buffer, n, 0);
        }
        close(client_fd);
    });
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Client socket
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    connect(client_fd, (sockaddr*)&addr, sizeof(addr));
    
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    
    char send_buf[64] = "PING";
    char recv_buf[64];
    
    for (auto _ : state) {
        send(client_fd, send_buf, 4, 0);
        recv(client_fd, recv_buf, 4, 0);
        benchmark::DoNotOptimize(recv_buf);
    }
    
    close(client_fd);
    close(server_fd);
    server_thread.join();
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LoopbackLatency)->Iterations(1000);

// Benchmark: Buffer size impact
static void BM_RecvBufferSize(benchmark::State& state) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    int buffer_size = state.range(0);
    
    for (auto _ : state) {
        setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));
        benchmark::ClobberMemory();
    }
    
    close(sock_fd);
    
    state.SetItemsProcessed(state.iterations());
    state.SetLabel(std::to_string(buffer_size / 1024) + " KB");
}
BENCHMARK(BM_RecvBufferSize)
    ->Arg(64 * 1024)
    ->Arg(256 * 1024)
    ->Arg(1024 * 1024)
    ->Arg(4 * 1024 * 1024);

// Benchmark: Socket option setting overhead
static void BM_SetSocketOptions(benchmark::State& state) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    int flag = 1;
    
    for (auto _ : state) {
        setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
        benchmark::ClobberMemory();
    }
    
    close(sock_fd);
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SetSocketOptions);

// Benchmark: Non-blocking mode toggle
static void BM_NonBlockingToggle(benchmark::State& state) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    for (auto _ : state) {
        int flags = fcntl(sock_fd, F_GETFL, 0);
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
        benchmark::ClobberMemory();
    }
    
    close(sock_fd);
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_NonBlockingToggle);

BENCHMARK_MAIN();
