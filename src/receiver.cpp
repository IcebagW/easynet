#include "receiver.h"
#include "tcp.h"
#include "udp.h"

#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#include <mutex>

static void print_final(const Stats& stats, const char* label) {
    auto now = std::chrono::steady_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::milliseconds>(now - stats.start_time).count() / 1000.0;
    double avg_rate = sec > 0 ? stats.total_bytes * 8.0 / sec / 1e6 : 0;
    printf("\n--- %s Summary ---\n", label);
    printf("  Duration: %.2f s\n", sec);
    printf("  Total received: %.2f MB (%llu bytes)\n", stats.total_bytes / 1e6, (unsigned long long)stats.total_bytes);
    printf("  Packets: %llu\n", (unsigned long long)stats.total_packets);
    printf("  Avg rate: %.2f Mbit/s\n", avg_rate);
}

// --- TCP Receiver (multi-connection) ---

struct SharedReceiver {
    Stats stats;
    std::mutex mtx;
    int conn_count = 0;

    void print(uint64_t bytes) {
        std::lock_guard<std::mutex> lock(mtx);
        stats.print(bytes, "RX");
    }

    void merge_final(Stats& out) {
        std::lock_guard<std::mutex> lock(mtx);
        out.merge(stats);
    }
};

static void recv_tcp_connection(platform_socket_t client_fd, SharedReceiver* shared) {
    uint8_t buf[65536];
    while (g_running) {
        platform_ssize_t n = recv(client_fd, (char*)buf, sizeof(buf), 0);
        if (n == PLATFORM_SOCKET_ERROR) {
            if (platform_is_eintr()) continue;
            if (platform_is_econnreset()) {
                printf("Connection reset by peer.\n");
            } else {
                fprintf(stderr, "recv error: %s (code %d)\n",
                        platform_strerror(platform_get_error()),
                        platform_get_error());
            }
            break;
        }
        if (n == 0) {
            printf("Connection closed by peer.\n");
            break;
        }
        shared->print((uint64_t)n);
    }
    tcp_close(client_fd);
}

static void recv_tcp(const Config& cfg) {
    platform_socket_t server_fd = tcp_bind_listen(cfg.port);
    if (!platform_is_valid_socket(server_fd)) return;

    // Set accept timeout so we can poll g_running
    struct timeval tv{};
    tv.tv_sec = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    SharedReceiver shared;
    shared.stats.init();

    std::vector<std::thread> threads;
    int expected = cfg.threads > 0 ? cfg.threads : 1;

    printf("Receiving TCP data on port %d (expecting %d connection(s))...\n", cfg.port, expected);

    while (g_running) {
        platform_socket_t client_fd = tcp_accept(server_fd);
        if (!platform_is_valid_socket(client_fd)) {
            // timeout or error — loop to check g_running
            if (errno == EAGAIN || errno == EWOULDBLOCK || platform_is_eintr()) continue;
            break;
        }
        shared.conn_count++;
        threads.emplace_back(recv_tcp_connection, client_fd, &shared);

        if (shared.conn_count >= expected) break;
    }

    // Wait for all connection threads
    for (auto& t : threads) t.join();

    tcp_close(server_fd);
    print_final(shared.stats, "TCP Receive");
}

// --- UDP Receiver (unchanged, single-threaded) ---

static void recv_udp(const Config& cfg) {
    platform_socket_t fd = udp_create_socket();
    if (!platform_is_valid_socket(fd)) return;

    if (udp_bind(fd, cfg.port) < 0) {
        udp_close(fd);
        return;
    }

    Stats stats;
    stats.init();

    uint8_t buf[65536];
    sockaddr_in src;
    printf("Receiving UDP data...\n");

    while (g_running) {
        platform_ssize_t n = udp_recvfrom(fd, buf, sizeof(buf), &src);
        if (n == PLATFORM_SOCKET_ERROR) break;
        if (n == 0) continue;
        stats.print((uint64_t)n, "RX");
    }

    udp_close(fd);
    print_final(stats, "UDP Receive");
}

// --- Dispatcher ---

void run_receiver(const Config& cfg) {
    if (cfg.proto == Proto::TCP) {
        recv_tcp(cfg);
    } else {
        recv_udp(cfg);
    }
}
